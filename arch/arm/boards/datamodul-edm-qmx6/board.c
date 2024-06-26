// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2012 Steffen Trumtrar, Pengutronix

#include <asm/mach-types.h>
#include <environment.h>
#include <bootsource.h>
#include <common.h>
#include <envfs.h>
#include <linux/sizes.h>
#include <init.h>
#include <gpio.h>
#include <of.h>

#include <linux/mdio.h>
#include <linux/phy.h>
#include <linux/micrel_phy.h>
#include <mfd/stmpe-i2c.h>

#include <asm/armlinux.h>
#include <asm/io.h>

#include <mach/imx/imx6-regs.h>
#include <mach/imx/iomux-mx6.h>
#include <mach/imx/generic.h>
#include <mach/imx/imx6.h>
#include <mach/imx/bbu.h>

#define RQ7_GPIO_ENET_PHYADD2	IMX_GPIO_NR(6, 30)
#define RQ7_GPIO_ENET_MODE0	IMX_GPIO_NR(6, 25)
#define RQ7_GPIO_ENET_MODE1	IMX_GPIO_NR(6, 27)
#define RQ7_GPIO_ENET_MODE2	IMX_GPIO_NR(6, 28)
#define RQ7_GPIO_ENET_MODE3	IMX_GPIO_NR(6, 29)
#define RQ7_GPIO_ENET_EN_CLK125	IMX_GPIO_NR(6, 24)
#define RQ7_GPIO_ENET_RESET     IMX_GPIO_NR(1, 25)

static iomux_v3_cfg_t realq7_pads_gpio[] = {
	MX6Q_PAD_RGMII_RXC__GPIO_6_30,
	MX6Q_PAD_RGMII_RD0__GPIO_6_25,
	MX6Q_PAD_RGMII_RD1__GPIO_6_27,
	MX6Q_PAD_RGMII_RD2__GPIO_6_28,
	MX6Q_PAD_RGMII_RD3__GPIO_6_29,
	MX6Q_PAD_RGMII_RX_CTL__GPIO_6_24,
	MX6Q_PAD_ENET_CRS_DV__GPIO_1_25,
};

static int ksz9031rn_phy_fixup(struct phy_device *dev)
{
	/*
	 * min rx data delay, max rx/tx clock delay,
	 * min rx/tx control delay
	 */
	phy_write_mmd(dev, MDIO_MMD_WIS, 4, 0);
	phy_write_mmd(dev, MDIO_MMD_WIS, 5, 0);
	phy_write_mmd(dev, MDIO_MMD_WIS, 8, 0x03ff);

	return 0;
}

static int realq7_enet_init(void)
{
	if (!of_machine_is_compatible("dmo,imx6q-edmqmx6"))
		return 0;

	mxc_iomux_v3_setup_multiple_pads(realq7_pads_gpio, ARRAY_SIZE(realq7_pads_gpio));
	gpio_direction_output(RQ7_GPIO_ENET_PHYADD2, 0);
	gpio_direction_output(RQ7_GPIO_ENET_MODE0, 1);
	gpio_direction_output(RQ7_GPIO_ENET_MODE1, 1);
	gpio_direction_output(RQ7_GPIO_ENET_MODE2, 1);
	gpio_direction_output(RQ7_GPIO_ENET_MODE3, 1);
	gpio_direction_output(RQ7_GPIO_ENET_EN_CLK125, 1);

	gpio_direction_output(RQ7_GPIO_ENET_RESET, 0);
	mdelay(50);

	gpio_direction_output(RQ7_GPIO_ENET_RESET, 1);
	mdelay(50);

	gpio_free(RQ7_GPIO_ENET_RESET);

	phy_register_fixup_for_uid(PHY_ID_KSZ9031, MICREL_PHY_ID_MASK,
					   ksz9031rn_phy_fixup);

	return 0;
}
fs_initcall(realq7_enet_init);

static int realq7_env_init(void)
{
	if (!of_machine_is_compatible("dmo,imx6q-edmqmx6"))
		return 0;

	imx6_bbu_internal_spi_i2c_register_handler("spiflash", "/dev/m25p0.barebox",
		BBU_HANDLER_FLAG_DEFAULT);
	imx6_bbu_internal_mmc_register_handler("mmc", "/dev/mmc3.barebox", 0);
	return 0;
}
late_initcall(realq7_env_init);

static int realq7_device_init(void)
{
	if (!of_machine_is_compatible("dmo,imx6q-edmqmx6"))
		return 0;

	gpio_direction_output(IMX_GPIO_NR(2, 22), 1);
	gpio_direction_output(IMX_GPIO_NR(2, 21), 1);

	switch (bootsource_get()) {
	case BOOTSOURCE_MMC:
		switch (bootsource_get_instance()) {
		case 2:
			of_device_enable_path("/chosen/environment-sd");
			break;
		case 3:
			of_device_enable_path("/chosen/environment-emmc");
			break;
		}
		break;
	default:
	case BOOTSOURCE_SPI_NOR:
		of_device_enable_path("/chosen/environment-spi");
		break;
	}

	barebox_set_hostname("eDM-QMX6");

	return 0;
}
device_initcall(realq7_device_init);
