/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <common.h>
#include <aiodev.h>
#include <bootsource.h>
#include <deep-probe.h>
#include <environment.h>
#include <envfs.h>
#include <init.h>

static int som_revision = -1;

static int __init diasom_rk3588_check_adc(void)
{
	struct device *aio_dev;
	struct aiochannel *aio_ch;
	int val, ret;

	if (!of_machine_is_compatible("diasom,ds-rk3588-btb"))
		return 0;

	aio_dev = of_device_enable_and_register_by_name("adc@fec10000");
	if (!aio_dev) {
		pr_err("Unable to get ADC device!\n");
		return -ENODEV;
	}

	aio_ch = aiochannel_by_name("aiodev0.in_value7_mV");
	if (IS_ERR(aio_ch)) {
		ret = PTR_ERR(aio_ch);
		pr_err("Could not find ADC channel: %i!\n", ret);
		return ret;
	}

	ret = aiochannel_get_value(aio_ch, &val);
	if (ret) {
		pr_err("Could not get ADC value: %i!\n", ret);
		return ret;
	}

	if (val < 300) {
		som_revision = 0;
	} else {
		pr_warn("Unhandled BTB revision ADC value: %i!\n", val);
	}

	return 0;
}
device_initcall(diasom_rk3588_check_adc);

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

	pr_info("Boot source: %s, instance %i\n",
		bootsource_to_string(bootsource), instance);

	defaultenv_append_directory(defaultenv_diasom_rk3588);

	return 0;
}

static const struct of_device_id diasom_rk3588_of_match[] = {
	{ .compatible = "diasom,ds-rk3588-btb" },
	{ },
};
BAREBOX_DEEP_PROBE_ENABLE(diasom_rk3588_of_match);

static struct driver diasom_rk3588_driver = {
	.name = "board-ds-rk3588",
	.probe = diasom_rk3588_probe,
	.of_compatible = diasom_rk3588_of_match,
};
coredevice_platform_driver(diasom_rk3588_driver);
