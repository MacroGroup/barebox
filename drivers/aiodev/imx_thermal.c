// SPDX-License-Identifier: GPL-2.0-only
/*
 * imx_thermal
 *
 * Copyright (c) 2015 Zodiac Inflight Innovation
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * based on the code of analogous driver from U-Boot:
 * (C) Copyright 2014-2015 Freescale Semiconductor, Inc.
 * Author: Nitin Garg <nitin.garg@freescale.com>
 *             Ye Li <Ye.Li@freescale.com>
 */

#include <common.h>
#include <init.h>
#include <malloc.h>
#include <clock.h>
#include <driver.h>
#include <xfuncs.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/math64.h>
#include <linux/log2.h>
#include <linux/clk.h>
#include <linux/nvmem-consumer.h>
#include <mach/imx/imx6-anadig.h>
#include <io.h>
#include <aiodev.h>
#include <mfd/syscon.h>

#define FACTOR0			10000000
#define MEASURE_FREQ		327

struct imx_thermal_data {
	int c1, c2;
	void __iomem *base;
	struct clk *clk;

	struct aiodevice aiodev;
	struct aiochannel aiochan;
};

static inline struct imx_thermal_data *
to_imx_thermal_data(struct aiochannel *chan)
{
	return container_of(chan, struct imx_thermal_data, aiochan);
}


static int imx_thermal_read(struct aiochannel *chan, int *val)
{
	uint64_t start;
	uint32_t tempsense0, tempsense1;
	uint16_t n_meas;
	struct imx_thermal_data *imx_thermal = to_imx_thermal_data(chan);

	/*
	 * now we only use single measure, every time we read
	 * the temperature, we will power on/down anadig thermal
	 * module
	 */
	writel(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
	       imx_thermal->base + HW_ANADIG_TEMPSENSE0_CLR);
	writel(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF,
	       imx_thermal->base + HW_ANADIG_ANA_MISC0_SET);

	/* setup measure freq */
	tempsense1 = readl(imx_thermal->base + HW_ANADIG_TEMPSENSE1);
	tempsense1 &= ~BM_ANADIG_TEMPSENSE1_MEASURE_FREQ;
	tempsense1 |= MEASURE_FREQ;
	writel(tempsense1, imx_thermal->base + HW_ANADIG_TEMPSENSE1);

	/* start the measurement process */
	writel(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
		imx_thermal->base + HW_ANADIG_TEMPSENSE0_CLR);
	writel(BM_ANADIG_TEMPSENSE0_FINISHED,
		imx_thermal->base + HW_ANADIG_TEMPSENSE0_CLR);
	writel(BM_ANADIG_TEMPSENSE0_MEASURE_TEMP,
	       imx_thermal->base + HW_ANADIG_TEMPSENSE0_SET);

	/* make sure that the latest temp is valid */
	start = get_time_ns();
	do {
		tempsense0 = readl(imx_thermal->base + HW_ANADIG_TEMPSENSE0);

		if (is_timeout(start, 1 * SECOND)) {
			dev_err(imx_thermal->aiodev.hwdev,
				"Timeout waiting for measurement\n");
			return -EIO;
		}
	} while (!(tempsense0 & BM_ANADIG_TEMPSENSE0_FINISHED));

	n_meas = (tempsense0 & BM_ANADIG_TEMPSENSE0_TEMP_VALUE)
		>> BP_ANADIG_TEMPSENSE0_TEMP_VALUE;
	writel(BM_ANADIG_TEMPSENSE0_FINISHED,
	       imx_thermal->base + HW_ANADIG_TEMPSENSE0_CLR);

	*val = (int)n_meas * imx_thermal->c1 + imx_thermal->c2;

	/* power down anatop thermal sensor */
	writel(BM_ANADIG_TEMPSENSE0_POWER_DOWN,
	       imx_thermal->base + HW_ANADIG_TEMPSENSE0_SET);
	writel(BM_ANADIG_ANA_MISC0_REFTOP_SELBIASOFF,
	       imx_thermal->base + HW_ANADIG_ANA_MISC0_CLR);

	return 0;
}

static int imx_thermal_probe(struct device *dev)
{
	uint32_t ocotp_ana1;
	struct imx_thermal_data *imx_thermal;
	int t1, n1, t2, n2;
	int ret;

	ret = nvmem_cell_read_variable_le_u32(dev, "calib", &ocotp_ana1);
	if (ret)
		return ret;

	imx_thermal = xzalloc(sizeof(*imx_thermal));
	imx_thermal->base = syscon_base_lookup_by_phandle(dev->of_node,
							  "fsl,tempmon");
	if (IS_ERR(imx_thermal->base)) {
		dev_err(dev, "Could not get ANATOP address\n");
		ret = PTR_ERR(imx_thermal->base);
		goto free_imx_thermal;
	}

	n1 = ocotp_ana1 >> 20;
	t1 = 25;
	n2 = (ocotp_ana1 & 0x000FFF00) >> 8;
	t2 = ocotp_ana1 & 0xFF;

	imx_thermal->c1 = (-1000 * (t2 - t1)) / (n1 - n2);
	imx_thermal->c2 = 1000 * t2 + (1000 * n2 * (t2 - t1)) / (n1 - n2);

	imx_thermal->clk = clk_get(dev, NULL);
	if (IS_ERR(imx_thermal->clk)) {
		ret = PTR_ERR(imx_thermal->clk);
		goto free_imx_thermal;
	}

	ret = clk_enable(imx_thermal->clk);
	if (ret) {
		dev_err(dev, "Failed to enable clock: %pe\n",
			  ERR_PTR(ret));
		goto put_clock;
	}

	imx_thermal->aiodev.num_channels = 1;
	imx_thermal->aiodev.hwdev = dev;
	imx_thermal->aiodev.name = "thermal-sensor";
	imx_thermal->aiodev.channels =
		xmalloc(imx_thermal->aiodev.num_channels *
			sizeof(imx_thermal->aiodev.channels[0]));
	imx_thermal->aiodev.channels[0] = &imx_thermal->aiochan;
	imx_thermal->aiochan.unit = "mC";
	imx_thermal->aiodev.read = imx_thermal_read;

	ret = aiodevice_register(&imx_thermal->aiodev);
	if (!ret)
		goto done;

	clk_disable(imx_thermal->clk);
put_clock:
	clk_put(imx_thermal->clk);
free_imx_thermal:
	kfree(imx_thermal);
done:
	return ret;
}

static const struct of_device_id of_imx_thermal_match[] = {
	{ .compatible = "fsl,imx6q-tempmon", },
	{ .compatible = "fsl,imx6sx-tempmon", },
	{ /* end */ }
};
MODULE_DEVICE_TABLE(of, of_imx_thermal_match);


static struct driver imx_thermal_driver = {
	.name		= "imx_thermal",
	.probe		= imx_thermal_probe,
	.of_compatible	= DRV_OF_COMPAT(of_imx_thermal_match),
};

device_platform_driver(imx_thermal_driver);
