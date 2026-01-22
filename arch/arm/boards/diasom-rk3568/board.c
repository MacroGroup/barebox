/* SPDX-License-Identifier: GPL-2.0-only */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

#include <bootsource.h>
#include <common.h>
#include <aiodev.h>
#include <environment.h>
#include <envfs.h>
#include <globalvar.h>
#include <init.h>
#include <machine_id.h>
#include <i2c/i2c.h>
#include <linux/nvmem-consumer.h>
#include <mach/rockchip/bbu.h>

static int module_revision = -1;

static int diasom_rk3568_probe_i2c(struct i2c_adapter *adapter, const int addr)
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

static struct i2c_adapter *diasom_rk3568_i2c_get_adapter(const int nr)
{
	char *alias = basprintf("i2c%i", nr);
	struct device *dev;

	dev = of_device_enable_and_register_by_alias(alias);
	free(alias);
	if (!dev)
		return NULL;

	return i2c_get_adapter(nr);
}

/*
	Cameras mapping:
	SOM-EVB:
		camera0 = XC7160/I2C4
		camera1 = IMX335/I2C4
		camera2 = IMX335/I2C7
		camera3 = DS90UB954/I2C7 -> DS90UB953 -> AR0233
		camera4 = IMX415/I2C4
		camera5 = IMX415/I2C7
		camera6 = IMX327/I2C4
	SOM-SMARC-EVB:
		camera0 = IMX335/I2C7
		camera1 = IMX415/I2C7
*/

#define SONY_CAMERA_I2C_ADDR	0x1a

struct cameras {
	const char *alias;
	int (*detect)(struct i2c_client *, const char *);
};

static int diasom_rk3568_sony_imx327_detect(struct i2c_client *client,
					    const char *alias)
{
	u16 buf;
	int ret;

	/* 0x3441 == (0x0a0a || 0x0c0c) => IMX327 */
	ret = i2c_read_reg(client, 0x3441 | I2C_ADDR_16_BIT, (u8 *)&buf, sizeof(buf));
	if (ret == sizeof(buf) && (buf == 0x0a0a || buf == 0x0c0c)) {
		pr_info("Camera IMX327 detected.\n");
		of_register_set_status_fixup(alias, true);

		return 0;
	}

	return -ENODEV;
}

static int diasom_rk3568_sony_imx335_detect(struct i2c_client *client,
					    const char *alias)
{
	u16 buf;
	int ret;

	/* 0x341c == (0x01ff || 0x0047) => IMX335 */
	ret = i2c_read_reg(client, 0x341c | I2C_ADDR_16_BIT, (u8 *)&buf, sizeof(buf));
	if (ret == sizeof(buf) && (buf == 0x01ff || buf == 0x0047)) {
		pr_info("Camera IMX335 detected.\n");
		of_register_set_status_fixup(alias, true);

		return 0;
	}

	return -ENODEV;
}

static int diasom_rk3568_sony_imx415_detect(struct i2c_client *client,
					    const char *alias)
{
	u8 buf;
	int ret;

	/* 0x4001 == (0x01 || 0x03) => IMX415 */
	ret = i2c_read_reg(client, 0x4001 | I2C_ADDR_16_BIT, &buf, sizeof(buf));
	if (ret == sizeof(buf) && (buf == 0x01 || buf == 0x03)) {
		pr_info("Camera IMX415 detected.\n");
		of_register_set_status_fixup(alias, true);

		return 0;
	}

	return -ENODEV;
}

static int diasom_rk3568_sony_imx662_detect(struct i2c_client *client,
					    const char *alias)
{
	u8 buf;
	int ret;

	/* 0x3046 == 0x4c => IMX662 */
	ret = i2c_read_reg(client, 0x3046 | I2C_ADDR_16_BIT, &buf, sizeof(buf));
	if (ret == sizeof(buf) && buf == 0x4c) {
		pr_info("Camera IMX662 detected.\n");
		//TODO:

		return 0;
	}

	return -ENODEV;
}

static int diasom_rk3568_sony_camera_detect(struct i2c_adapter *adapter,
					    const struct cameras *cameras)
{
	struct i2c_client client;

	if (diasom_rk3568_probe_i2c(adapter, SONY_CAMERA_I2C_ADDR))
		return -ENODEV;

	client.adapter = adapter;
	client.addr = SONY_CAMERA_I2C_ADDR;

	for (int i = 0; cameras[i].alias && cameras[i].detect; i++)
		if (!cameras[i].detect(&client, cameras[i].alias))
			return 0;

	return -ENODEV;
}

static int diasom_rk3568_evb_fixup(struct device_node *root, void *unused)
{
	struct i2c_adapter *adapter = diasom_rk3568_i2c_get_adapter(4);
	static const struct cameras cameras[] = {
		{ "camera1", diasom_rk3568_sony_imx335_detect },
		{ "camera4", diasom_rk3568_sony_imx415_detect },
		{ "camera6", diasom_rk3568_sony_imx327_detect },
		{ "camera7", diasom_rk3568_sony_imx662_detect },
		{ }
	};

	if (!adapter)
		return -ENODEV;

	if (diasom_rk3568_probe_i2c(adapter, 0x10)) {
		pr_warn("ES8388 codec not found, disabling soundcard.\n");
		of_register_set_status_fixup("sound0", false);
	}

	if (!diasom_rk3568_sony_camera_detect(adapter, cameras))
		return 0;

	pr_info("Assume camera XC7160 is used.\n");
	of_register_set_status_fixup("camera0", true);

	return 0;
}

static int diasom_rk3568_smarc_evb_fixup(struct device_node *root, void *unused)
{
	struct i2c_adapter *adapter = diasom_rk3568_i2c_get_adapter(7);
	static const struct cameras cameras[] = {
		{ "camera0", diasom_rk3568_sony_imx335_detect },
		{ "camera1", diasom_rk3568_sony_imx415_detect },
		{ }
	};

	if (!adapter)
		return -ENODEV;

	diasom_rk3568_sony_camera_detect(adapter, cameras);

	return 0;
}

static int diasom_rk3568_evb_ver1_3_0_fixup(struct device_node *root,
					    void *unused)
{
	struct i2c_adapter *adapter = diasom_rk3568_i2c_get_adapter(7);

	if (!adapter)
		return -ENODEV;
	
	if (!diasom_rk3568_probe_i2c(adapter, 0x30)) {
		pr_info("FPD-Link deserializer detected.\n");
		of_register_set_status_fixup("camera3", true);
	} else {
		static const struct cameras cameras[] = {
			{ "camera2", diasom_rk3568_sony_imx335_detect },
			{ "camera5", diasom_rk3568_sony_imx415_detect },
			{ }
		};

		diasom_rk3568_sony_camera_detect(adapter, cameras);
	}
	
	return 0;
}

static int diasom_rk3568_can_fixup(struct device_node *root, void *unused)
{
	struct device_node *node;

	for_each_compatible_node_from(node, root, NULL, "rockchip,rk3568v2-canfd")
		of_property_write_string(node, "compatible", "rockchip,rk3568v3-canfd");

	return 0;
}

static int __init diasom_rk3568_machine_id(void)
{
	const char *serial;

	if (!of_machine_is_compatible("rockchip,rk3568"))
		return 0;

	serial = getenv("mmc0.cid_psn");
	if (!serial) {
		pr_err("Unable to get MCI device!\n");
		return -ENODEV;
	}

	pr_info("Setup Machine ID from EMMC serial: %s\n", serial);

	machine_id_set_hashable(serial, strlen(serial));

	nvvar_add("bootm.provide_machine_id", "1");

	return 0;
}
of_populate_initcall(diasom_rk3568_machine_id);

static int __init diasom_rk3568_get_adc_value(const char *name, int *val)
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

static void __init diasom_rk3568_check_adc(void)
{
	struct device *aio_dev;
	int val;

	aio_dev = of_device_enable_and_register_by_name("saradc@fe720000");
	if (!aio_dev) {
		pr_err("Unable to get ADC device!\n");
		return;
	}

	if (!diasom_rk3568_get_adc_value("aiodev0.in_value0_mV", &val)) {
		if (val <= 40) {
			pr_info("Recovery key pressed, enforce gadget mode...\n");
			globalvar_add_simple("board.recovery", "true");
		}
	}

	if (diasom_rk3568_get_adc_value("aiodev0.in_value1_mV", &val))
		return;

	if (of_machine_is_compatible("diasom,ds-rk3568-som-smarc")) {
		if (val <= 100) {
			module_revision = 0x111;
		} else {
			pr_warn("Unhandled SMARC revision ADC value: %i!\n", val);
		}
	} else if (of_machine_is_compatible("diasom,ds-rk3568-som-sodimm")) {
		if (val <= 100) {
			module_revision = 0x111;
		} else {
			pr_warn("Unhandled SODIMM revision ADC value: %i!\n", val);
		}
	}
}

static bool __init diasom_rk3568_load_overlay(const void *ovl)
{
	if (ovl) {
		int ret;

		ret = of_overlay_apply_dtbo(of_get_root_node(), ovl);
		if (!ret)
			return true;

		pr_err("Cannot apply overlay: %pe!\n", ERR_PTR(ret));
	}

	return false;
}

static int __init diasom_rk3568_init(void)
{
	bool do_probe = false;
	int ret = 0;

	if (of_machine_is_compatible("rockchip,rk3568")) {
		struct device_node *otp = of_find_node_by_name_address(NULL, "nvmem@fe38c000");
		char ver = 0;

		if (otp) {
			if (!of_device_ensure_probed(otp)) {
				struct device_node *cpuver;
				cpuver = of_find_node_by_name_address(NULL, "cpu-info");
				if (cpuver) {
					char *val = nvmem_cell_get_and_read(cpuver, "cpu-version", sizeof(char));
					if (val) {
						ver = *val;
						free(val);
					}
				}
			}
		}

		if (ver) {
			pr_info("CPU version 0x%02x detected.\n", ver);
			if (ver > 2)
				of_register_fixup(diasom_rk3568_can_fixup, NULL);
		} else
			pr_warn("CPU version Unknown!\n");
	} else
		return 0;

	if (of_machine_is_compatible("diasom,ds-rk3568-som")) {
		struct i2c_adapter *adapter =
			diasom_rk3568_i2c_get_adapter(0);
		void *som_ovl;

		diasom_rk3568_check_adc();

		if (!adapter) {
			pr_err("Cannot determine SOM version.\n");
			return -ENOTSUPP;
		}

		if (!diasom_rk3568_probe_i2c(adapter, 0x1c)) {
			extern char __dtbo_rk3568_diasom_som_ver2_start[];

			som_ovl = __dtbo_rk3568_diasom_som_ver2_start;
			pr_info("SOM version 2+ detected.\n");
		} else {
			extern char __dtbo_rk3568_diasom_som_ver1_start[];

			som_ovl = __dtbo_rk3568_diasom_som_ver1_start;
			pr_info("SOM version 1 detected.\n");
		}

		if (diasom_rk3568_load_overlay(som_ovl))
			do_probe = true;
	} else
		return 0;

	if (of_machine_is_compatible("diasom,ds-rk3568-som-smarc")) {
		switch (module_revision) {
			case 0x111:
				break;
			default:
				pr_err("Cannot determine SMARC revision.\n");
				ret = -ENOTSUPP;
				goto out;
		}

		pr_info("SMARC revision: %i.%d.%d\n", module_revision >> 8,
			(module_revision & 0xff) >> 4, module_revision & 0xf);
	} else if (of_machine_is_compatible("diasom,ds-rk3568-som-sodimm")) {
		switch (module_revision) {
			case 0x111:
				break;
			default:
				pr_err("Cannot determine SODIMM revision.\n");
				ret = -ENOTSUPP;
				goto out;
		}

		pr_info("SODIMM revision: %i.%d.%d\n", module_revision >> 8,
			(module_revision & 0xff) >> 4, module_revision & 0xf);
	} else
		pr_info("RAW module variant used.\n");

	if (of_machine_is_compatible("diasom,ds-rk3568-som-evb")) {
		struct i2c_adapter *adapter =
			diasom_rk3568_i2c_get_adapter(4);
		void *evb_ovl;

		if (!adapter) {
			pr_err("Cannot determine EVB version.\n");
			ret = -ENOTSUPP;
			goto out;
		}

		if (!diasom_rk3568_probe_i2c(adapter, 0x70)) {
			extern char __dtbo_rk3568_diasom_som_evb_ver1_3_0_start[];

			pr_info("EVB version 1.3.0+ detected.\n");
			evb_ovl = __dtbo_rk3568_diasom_som_evb_ver1_3_0_start;

			of_register_fixup(diasom_rk3568_evb_ver1_3_0_fixup, NULL);
		} else if (!diasom_rk3568_probe_i2c(adapter, 0x50)) {
			extern char __dtbo_rk3568_diasom_som_evb_ver1_2_1_start[];

			pr_info("EVB version 1.2.1+ detected.\n");
			evb_ovl = __dtbo_rk3568_diasom_som_evb_ver1_2_1_start;

			of_register_fixup(diasom_rk3568_evb_fixup, NULL);
		} else {
			extern char __dtbo_rk3568_diasom_som_evb_ver1_1_0_start[];

			pr_info("EVB version 1.2.0 or earlier detected.\n");
			evb_ovl = __dtbo_rk3568_diasom_som_evb_ver1_1_0_start;

			of_register_fixup(diasom_rk3568_evb_fixup, NULL);
		}

		if (diasom_rk3568_load_overlay(evb_ovl))
			do_probe = true;
	} else 	if (of_machine_is_compatible("diasom,ds-rk3568-som-smarc-evb")) {
		of_register_fixup(diasom_rk3568_smarc_evb_fixup, NULL);
	} else 	if (of_machine_is_compatible("diasom,ds-rk3568-som-sodimm-evb")) {
		//TODO:
	}

out:
	if (do_probe) {
		struct device_node *root = of_get_root_node();

		of_probe();

		/* Ensure reload aliases & model name */
		of_set_root_node(NULL);
		of_set_root_node(root);
	}

	return ret;
}
device_initcall(diasom_rk3568_init);

static int __init diasom_rk3568_probe(struct device *dev)
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

	rk3568_bbu_mmc_register("sd", 0, "/dev/mmc1");
	rk3568_bbu_mmc_register("emmc", BBU_HANDLER_FLAG_DEFAULT,
				"/dev/mmc0");

	defaultenv_append_directory(defaultenv_diasom_rk3568);

	return 0;
}

static const struct of_device_id __init diasom_rk3568_of_match[] = {
	{ .compatible = "diasom,ds-rk3568-som" },
	{ },
};

static struct driver __init diasom_rk3568_driver = {
	.name = "board-ds-rk3568-som",
	.probe = diasom_rk3568_probe,
	.of_compatible = diasom_rk3568_of_match,
};
coredevice_platform_driver(diasom_rk3568_driver);
