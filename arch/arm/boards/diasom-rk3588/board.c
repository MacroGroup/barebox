/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <common.h>
#include <aiodev.h>
#include <bootsource.h>
#include <deep-probe.h>
#include <environment.h>
#include <envfs.h>
#include <globalvar.h>
#include <init.h>
#include <machine_id.h>
#include <i2c/i2c.h>

static int som_revision = -1;

static int __init diasom_rk3588_probe_i2c(struct i2c_adapter *adapter,
					  const int addr)
{
	u8 buf[1];
	struct i2c_msg msg = {
		.addr = addr,
		.buf = buf,
		.len = sizeof(buf),
		.flags = I2C_M_RD,
	};

	return (i2c_transfer(adapter, &msg, 1) == 1) ? 0: -ENODEV;
}

static struct i2c_adapter __init *diasom_rk3588_i2c_get_adapter(const int nr)
{
	char *alias = basprintf("i2c%i", nr);
	struct device *dev;

	dev = of_device_enable_and_register_by_alias(alias);
	free(alias);
	if (!dev)
		return NULL;

	return i2c_get_adapter(nr);
}

static int __init diasom_rk3588_get_adc_value(const char *name, int *val)
{
	struct aiochannel *aio_ch = aiochannel_by_name(name);
	int ret;

	if (IS_ERR(aio_ch)) {
		ret = PTR_ERR(aio_ch);
		pr_err("Could not find ADC channel \"%s\": %i!\n", name, ret);
		return ret;
	}

	ret = aiochannel_get_value(aio_ch, val);
	if (ret)
		pr_err("Could not get ADC value: %i!\n", ret);

	return ret;
}

static int __init diasom_rk3588_check_adc(void)
{
	struct device *aio_dev;
	struct i2c_adapter *adapter;
	int val = 0, ret;

	if (!of_machine_is_compatible("diasom,ds-rk3588-btb"))
		return 0;

	adapter = diasom_rk3588_i2c_get_adapter(2);
	if (!adapter) {
		pr_err("Could get I2C2 bus. "
		       "Probably this is not Diasom board!\n");
		return -ENODEV;
	}

	if (diasom_rk3588_probe_i2c(adapter, 0x42)) {
		pr_err("Could get I2C2 device. "
		       "Probably this is not Diasom board!\n");
		return -ENODEV;
	}

	aio_dev = of_device_enable_and_register_by_name("adc@fec10000");
	if (!aio_dev) {
		pr_err("Unable to get ADC device!\n");
		return -ENODEV;
	}

	ret = diasom_rk3588_get_adc_value("aiodev0.in_value1_mV", &val);
	if (ret)
		return ret;

	if (val < 50) {
		pr_info("Recovery key pressed, enforce gadget mode...\n");
		globalvar_add_simple("board.recovery", "true");
	}

	ret = diasom_rk3588_get_adc_value("aiodev0.in_value7_mV", &val);
	if (ret)
		return ret;

	if (val > 150 && val < 350) {
		som_revision = 0;
	} else {
		pr_warn("Unhandled BTB revision ADC value: %i!\n", val);
	}

	return 0;
}
device_initcall(diasom_rk3588_check_adc);

static int __init diasom_rk3588_machine_id(void)
{
	const char *serial;

	if (!of_machine_is_compatible("rockchip,rk3588"))
		return 0;

	serial = getenv("mmc0.cid_psn");
	if (!serial) {
		pr_err("Unable to get MCI device!\n");
		return -ENODEV;
	}

	pr_info("Setup Machine ID from EMMC serial: %s\n", serial);

	machine_id_set_hashable(serial, strlen(serial));

	return 0;
}
of_populate_initcall(diasom_rk3588_machine_id);

static int __init diasom_rk3588_late_init(void)
{
	if (!of_machine_is_compatible("diasom,ds-rk3588-btb"))
		return 0;

	switch (som_revision) {
	case 0:
		break;
	default:
		pr_err("Cannot determine BTB version.\n");
		return -ENOTSUPP;
	}

	pr_info("BTB version: %i\n", som_revision);

	return 0;
}
late_initcall(diasom_rk3588_late_init);

static int __init diasom_rk3588_probe(struct device *dev)
{
	enum bootsource bootsource = bootsource_get();
	int instance = bootsource_get_instance();

	barebox_set_hostname("diasom");

	if (bootsource != BOOTSOURCE_MMC || !instance) {
		if (bootsource != BOOTSOURCE_MMC) {
			pr_info("Boot source: %s, instance %i\n",
				bootsource_to_string(bootsource),
				instance);
			globalvar_add_simple("board.bootsource",
					     bootsource_to_string(bootsource));
		} else
			of_device_enable_path("/chosen/environment-emmc");
	} else
		of_device_enable_path("/chosen/environment-sd");

	defaultenv_append_directory(defaultenv_diasom_rk3588);

	return 0;
}

static const struct of_device_id __init diasom_rk3588_of_match[] = {
	{ .compatible = "diasom,ds-rk3588-btb" },
	{ },
};
BAREBOX_DEEP_PROBE_ENABLE(diasom_rk3588_of_match);

static struct driver __init diasom_rk3588_driver = {
	.name = "board-ds-rk3588",
	.probe = diasom_rk3588_probe,
	.of_compatible = diasom_rk3588_of_match,
};
coredevice_platform_driver(diasom_rk3588_driver);
