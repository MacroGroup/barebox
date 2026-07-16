/* SPDX-License-Identifier: GPL-2.0+ */
/* SPDX-FileCopyrightText: Alexander Shiyan <shc_work@mail.ru> */

/*
 * Based on: Copyright (c) 2023 Macro Limited
 * Author: Alex Skupov <skupov93@2gmail.com>
 */

#include <video/backlight.h>
#include <video/vpl.h>
#include <clock.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <video/media-bus-format.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <of_device.h>
#include <regulator.h>

#include <video/mipi_display.h>

#include <video/mipi_dsi.h>
#include <video/mipi_display.h>
#include <video/drm/drm_modes.h>
#include <video/videomode.h>

#define DIASOM_INIT_CMD_LEN	2

struct diasom_init_cmd {
	u8 data[DIASOM_INIT_CMD_LEN];
};

struct diasom_panel_desc {
	const struct drm_display_mode mode;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	const struct diasom_init_cmd *init_cmds;
	u32 num_init_cmds;
};

struct diasom {
	struct device *dev;
	struct vpl vpl;

	const struct diasom_panel_desc *desc;

	struct regulator *vdd;
	struct regulator *vccio;

	struct gpio_desc *reset;
	struct gpio_desc *enable;

	struct backlight_device *backlight;
};

static const struct diasom_init_cmd cz101b4001_init_cmds[] = {
	{ .data = { 0xE0, 0x00 } },
	{ .data = { 0xE1, 0x93 } },
	{ .data = { 0xE2, 0x65 } },
	{ .data = { 0xE3, 0xF8 } },
	{ .data = { 0x80, 0x03 } },
	{ .data = { 0xE0, 0x01 } },
	{ .data = { 0x00, 0x00 } },
	{ .data = { 0x01, 0x3B } },
	{ .data = { 0x0C, 0x74 } },
	{ .data = { 0x17, 0x00 } },
	{ .data = { 0x18, 0xAF } },
	{ .data = { 0x19, 0x00 } },
	{ .data = { 0x1A, 0x00 } },
	{ .data = { 0x1B, 0xAF } },
	{ .data = { 0x1C, 0x00 } },
	{ .data = { 0x35, 0x26 } },
	{ .data = { 0x37, 0x09 } },
	{ .data = { 0x38, 0x04 } },
	{ .data = { 0x39, 0x00 } },
	{ .data = { 0x3A, 0x01 } },
	{ .data = { 0x3C, 0x78 } },
	{ .data = { 0x3D, 0xFF } },
	{ .data = { 0x3E, 0xFF } },
	{ .data = { 0x3F, 0x7F } },
	{ .data = { 0x40, 0x06 } },
	{ .data = { 0x41, 0xA0 } },
	{ .data = { 0x42, 0x81 } },
	{ .data = { 0x43, 0x14 } },
	{ .data = { 0x44, 0x23 } },
	{ .data = { 0x45, 0x28 } },
	{ .data = { 0x55, 0x02 } },
	{ .data = { 0x57, 0x69 } },
	{ .data = { 0x59, 0x0A } },
	{ .data = { 0x5A, 0x2A } },
	{ .data = { 0x5B, 0x17 } },
	{ .data = { 0x5D, 0x7F } },
	{ .data = { 0x5E, 0x6B } },
	{ .data = { 0x5F, 0x5C } },
	{ .data = { 0x60, 0x4F } },
	{ .data = { 0x61, 0x4D } },
	{ .data = { 0x62, 0x3F } },
	{ .data = { 0x63, 0x42 } },
	{ .data = { 0x64, 0x2B } },
	{ .data = { 0x65, 0x44 } },
	{ .data = { 0x66, 0x43 } },
	{ .data = { 0x67, 0x43 } },
	{ .data = { 0x68, 0x63 } },
	{ .data = { 0x69, 0x52 } },
	{ .data = { 0x6A, 0x5A } },
	{ .data = { 0x6B, 0x4F } },
	{ .data = { 0x6C, 0x4E } },
	{ .data = { 0x6D, 0x20 } },
	{ .data = { 0x6E, 0x0F } },
	{ .data = { 0x6F, 0x00 } },
	{ .data = { 0x70, 0x7F } },
	{ .data = { 0x71, 0x6B } },
	{ .data = { 0x72, 0x5C } },
	{ .data = { 0x73, 0x4F } },
	{ .data = { 0x74, 0x4D } },
	{ .data = { 0x75, 0x3F } },
	{ .data = { 0x76, 0x42 } },
	{ .data = { 0x77, 0x2B } },
	{ .data = { 0x78, 0x44 } },
	{ .data = { 0x79, 0x43 } },
	{ .data = { 0x7A, 0x43 } },
	{ .data = { 0x7B, 0x63 } },
	{ .data = { 0x7C, 0x52 } },
	{ .data = { 0x7D, 0x5A } },
	{ .data = { 0x7E, 0x4F } },
	{ .data = { 0x7F, 0x4E } },
	{ .data = { 0x80, 0x20 } },
	{ .data = { 0x81, 0x0F } },
	{ .data = { 0x82, 0x00 } },
	{ .data = { 0xE0, 0x02 } },
	{ .data = { 0x00, 0x02 } },
	{ .data = { 0x01, 0x02 } },
	{ .data = { 0x02, 0x00 } },
	{ .data = { 0x03, 0x00 } },
	{ .data = { 0x04, 0x1E } },
	{ .data = { 0x05, 0x1E } },
	{ .data = { 0x06, 0x1F } },
	{ .data = { 0x07, 0x1F } },
	{ .data = { 0x08, 0x1F } },
	{ .data = { 0x09, 0x17 } },
	{ .data = { 0x0A, 0x17 } },
	{ .data = { 0x0B, 0x37 } },
	{ .data = { 0x0C, 0x37 } },
	{ .data = { 0x0D, 0x47 } },
	{ .data = { 0x0E, 0x47 } },
	{ .data = { 0x0F, 0x45 } },
	{ .data = { 0x10, 0x45 } },
	{ .data = { 0x11, 0x4B } },
	{ .data = { 0x12, 0x4B } },
	{ .data = { 0x13, 0x49 } },
	{ .data = { 0x14, 0x49 } },
	{ .data = { 0x15, 0x1F } },
	{ .data = { 0x16, 0x01 } },
	{ .data = { 0x17, 0x01 } },
	{ .data = { 0x18, 0x00 } },
	{ .data = { 0x19, 0x00 } },
	{ .data = { 0x1A, 0x1E } },
	{ .data = { 0x1B, 0x1E } },
	{ .data = { 0x1C, 0x1F } },
	{ .data = { 0x1D, 0x1F } },
	{ .data = { 0x1E, 0x1F } },
	{ .data = { 0x1F, 0x17 } },
	{ .data = { 0x20, 0x17 } },
	{ .data = { 0x21, 0x37 } },
	{ .data = { 0x22, 0x37 } },
	{ .data = { 0x23, 0x46 } },
	{ .data = { 0x24, 0x46 } },
	{ .data = { 0x25, 0x44 } },
	{ .data = { 0x26, 0x44 } },
	{ .data = { 0x27, 0x4A } },
	{ .data = { 0x28, 0x4A } },
	{ .data = { 0x29, 0x48 } },
	{ .data = { 0x2A, 0x48 } },
	{ .data = { 0x2B, 0x1F } },
	{ .data = { 0x2C, 0x01 } },
	{ .data = { 0x2D, 0x01 } },
	{ .data = { 0x2E, 0x00 } },
	{ .data = { 0x2F, 0x00 } },
	{ .data = { 0x30, 0x1F } },
	{ .data = { 0x31, 0x1F } },
	{ .data = { 0x32, 0x1E } },
	{ .data = { 0x33, 0x1E } },
	{ .data = { 0x34, 0x1F } },
	{ .data = { 0x35, 0x17 } },
	{ .data = { 0x36, 0x17 } },
	{ .data = { 0x37, 0x37 } },
	{ .data = { 0x38, 0x37 } },
	{ .data = { 0x39, 0x08 } },
	{ .data = { 0x3A, 0x08 } },
	{ .data = { 0x3B, 0x0A } },
	{ .data = { 0x3C, 0x0A } },
	{ .data = { 0x3D, 0x04 } },
	{ .data = { 0x3E, 0x04 } },
	{ .data = { 0x3F, 0x06 } },
	{ .data = { 0x40, 0x06 } },
	{ .data = { 0x41, 0x1F } },
	{ .data = { 0x42, 0x02 } },
	{ .data = { 0x43, 0x02 } },
	{ .data = { 0x44, 0x00 } },
	{ .data = { 0x45, 0x00 } },
	{ .data = { 0x46, 0x1F } },
	{ .data = { 0x47, 0x1F } },
	{ .data = { 0x48, 0x1E } },
	{ .data = { 0x49, 0x1E } },
	{ .data = { 0x4A, 0x1F } },
	{ .data = { 0x4B, 0x17 } },
	{ .data = { 0x4C, 0x17 } },
	{ .data = { 0x4D, 0x37 } },
	{ .data = { 0x4E, 0x37 } },
	{ .data = { 0x4F, 0x09 } },
	{ .data = { 0x50, 0x09 } },
	{ .data = { 0x51, 0x0B } },
	{ .data = { 0x52, 0x0B } },
	{ .data = { 0x53, 0x05 } },
	{ .data = { 0x54, 0x05 } },
	{ .data = { 0x55, 0x07 } },
	{ .data = { 0x56, 0x07 } },
	{ .data = { 0x57, 0x1F } },
	{ .data = { 0x58, 0x40 } },
	{ .data = { 0x5B, 0x30 } },
	{ .data = { 0x5C, 0x16 } },
	{ .data = { 0x5D, 0x34 } },
	{ .data = { 0x5E, 0x05 } },
	{ .data = { 0x5F, 0x02 } },
	{ .data = { 0x63, 0x00 } },
	{ .data = { 0x64, 0x6A } },
	{ .data = { 0x67, 0x73 } },
	{ .data = { 0x68, 0x1D } },
	{ .data = { 0x69, 0x08 } },
	{ .data = { 0x6A, 0x6A } },
	{ .data = { 0x6B, 0x08 } },
	{ .data = { 0x6C, 0x00 } },
	{ .data = { 0x6D, 0x00 } },
	{ .data = { 0x6E, 0x00 } },
	{ .data = { 0x6F, 0x88 } },
	{ .data = { 0x75, 0xFF } },
	{ .data = { 0x77, 0xDD } },
	{ .data = { 0x78, 0x3F } },
	{ .data = { 0x79, 0x15 } },
	{ .data = { 0x7A, 0x17 } },
	{ .data = { 0x7D, 0x14 } },
	{ .data = { 0x7E, 0x82 } },
	{ .data = { 0xE0, 0x04 } },
	{ .data = { 0x00, 0x0E } },
	{ .data = { 0x02, 0xB3 } },
	{ .data = { 0x09, 0x61 } },
	{ .data = { 0x0E, 0x48 } },
	{ .data = { 0xE0, 0x00 } },
	{ .data = { 0xE6, 0x02 } },
	{ .data = { 0xE7, 0x0C } },
};

static const struct diasom_panel_desc cz101b4001_desc = {
	.mode = {
		.clock		= 70000,

		.hdisplay	= 800,
		.hsync_start	= 800 + 40,
		.hsync_end	= 800 + 40 + 18,
		.htotal		= 800 + 40 + 18 + 20,

		.vdisplay	= 1280,
		.vsync_start	= 1280 + 20,
		.vsync_end	= 1280 + 20 + 4,
		.vtotal		= 1280 + 20 + 4 + 20,

		.width_mm	= 62,
		.height_mm	= 110,
		.type		= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.init_cmds = cz101b4001_init_cmds,
	.num_init_cmds = ARRAY_SIZE(cz101b4001_init_cmds),
};

static const struct diasom_init_cmd rfh1010j_init_cmds[] = {
	{ .data = { 0xB2, 0x30 } },
	{ .data = { 0x80, 0x5B } },
	{ .data = { 0x81, 0x47 } },
	{ .data = { 0x82, 0x84 } },
	{ .data = { 0x83, 0x88 } },
	{ .data = { 0x84, 0x88 } },
	{ .data = { 0x85, 0x23 } },
	{ .data = { 0x86, 0xB6 } },
};

static const struct diasom_panel_desc rfh1010j_desc = {
	.mode = {
		.clock		= 51200,

		.hdisplay	= 1024,
		.hsync_start	= 1024 + 160,
		.hsync_end	= 1024 + 160 + 70,
		.htotal		= 1024 + 160 + 160 + 70,

		.vdisplay	= 600,
		.vsync_start	= 600 + 12,
		.vsync_end	= 600 + 12 + 20,
		.vtotal		= 600 + 12 + 20 + 23,

		.width_mm	= 125,
		.height_mm	= 222,
		.type		= DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
	},
	.lanes = 4,
	.format = MIPI_DSI_FMT_RGB888,
	.init_cmds = rfh1010j_init_cmds,
	.num_init_cmds = ARRAY_SIZE(rfh1010j_init_cmds),
};

static int diasom_enable(struct diasom *diasom)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(diasom->dev);
	struct mipi_dsi_multi_context dsi_ctx = {.dsi = dsi};
	unsigned int i;
	int err;

	for (i = 0; i < diasom->desc->num_init_cmds; i++) {
		const struct diasom_init_cmd *cmd = &diasom->desc->init_cmds[i];

		err = mipi_dsi_dcs_write_buffer(dsi, cmd->data, DIASOM_INIT_CMD_LEN);
		if (err < 0)
			return err;
	}

	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);

	mipi_dsi_mdelay(&dsi_ctx, 120);

	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);

	if  (dsi_ctx.accum_err)
		return dsi_ctx.accum_err;

	return backlight_enable(diasom->backlight);
}

static int diasom_disable(struct diasom *diasom)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(diasom->dev);
	struct mipi_dsi_multi_context dsi_ctx = {.dsi = dsi};

	backlight_disable(diasom->backlight);

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);

	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);

	mipi_dsi_mdelay(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int diasom_prepare(struct diasom *diasom)
{
	int ret;

	ret = regulator_enable(diasom->vccio);
	if (ret)
		return ret;

	ret = regulator_enable(diasom->vdd);
	if (ret)
		return ret;

	gpiod_set_value_cansleep(diasom->enable, 1);
	mdelay(20);

	gpiod_set_value_cansleep(diasom->reset, 1);
	mdelay(20);

	gpiod_set_value_cansleep(diasom->reset, 0);
	mdelay(30);

	return 0;
}

static int diasom_unprepare(struct diasom *diasom)
{
	gpiod_set_value_cansleep(diasom->reset, 1);
	mdelay(120);

	gpiod_set_value_cansleep(diasom->enable, 0);

	regulator_disable(diasom->vdd);
	regulator_disable(diasom->vccio);

	return 0;
}

static int diasom_get_modes(struct diasom *diasom,
			    struct display_timings *timings)
{
	struct fb_videomode *mode;

	mode = xzalloc(sizeof(*mode));

	drm_display_mode_to_fb_videomode(&diasom->desc->mode, mode);

	timings->modes = mode;
	timings->num_modes = 1;

	return 0;
}

static int diasom_ioctl(struct vpl *vpl, unsigned int port,
			unsigned int cmd, void *ptr)
{
	struct diasom *diasom = container_of(vpl, struct diasom, vpl);

	switch (cmd) {
	case VPL_PREPARE:
		return diasom_prepare(diasom);
	case VPL_ENABLE:
		return diasom_enable(diasom);
	case VPL_DISABLE:
		return diasom_disable(diasom);
	case VPL_UNPREPARE:
		return diasom_unprepare(diasom);
	case VPL_GET_VIDEOMODES:
		return diasom_get_modes(diasom, ptr);
	case VPL_GET_BUS_FORMAT:
		*(u32 *)ptr = MEDIA_BUS_FMT_RGB888_1X24;
		fallthrough;
	default:
		return 0;
	}
}

static int diasom_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct device_node *backlight_node;
	struct device *dev = &dsi->dev;
	struct diasom *diasom;
	int ret;

	diasom = devm_kzalloc(dev, sizeof(*diasom), GFP_KERNEL);
	if (!diasom)
		return -ENOMEM;

	diasom->reset = gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(diasom->reset))
		return dev_err_probe(&dsi->dev, PTR_ERR(diasom->reset),
				     "failed to get reset GPIO\n");

	diasom->enable = gpiod_get(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(diasom->enable))
		return dev_err_probe(&dsi->dev, PTR_ERR(diasom->enable),
				     "failed to get enable GPIO\n");

	diasom->vdd = regulator_get(dev, "vdd");
	if (IS_ERR(diasom->vdd))
		return dev_err_probe(&dsi->dev, PTR_ERR(diasom->vdd),
				     "failed to get vdd regulator\n");

	diasom->vccio = regulator_get(dev, "vccio");
	if (IS_ERR(diasom->vccio))
		return dev_err_probe(&dsi->dev, PTR_ERR(diasom->vccio),
				     "failed to get vccio regulator\n");

	backlight_node = of_parse_phandle(dev->device_node, "backlight", 0);
	if (backlight_node) {
		diasom->backlight = of_backlight_find(backlight_node);
		if (!diasom->backlight) {
			dev_err(dev, "Cannot find backlight\n");
			return -ENODEV;
		}
	}

	mipi_dsi_set_drvdata(dsi, diasom);

	diasom->dev = dev;
	diasom->desc = of_device_get_match_data(dev);

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_NO_EOT_PACKET;
	dsi->format = diasom->desc->format;
	dsi->lanes = diasom->desc->lanes;

	diasom->vpl.node = dev->of_node;
	diasom->vpl.ioctl = diasom_ioctl;

	ret = vpl_register(&diasom->vpl);
	if (ret)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		return dev_err_probe(dev, ret, "mipi_dsi_attach failed\n");

	dev_info(dev, "%ux%u@%u %ubpp dsi %udl - ready\n",
		 diasom->desc->mode.hdisplay, diasom->desc->mode.vdisplay,
		 drm_mode_vrefresh(&diasom->desc->mode),
		 mipi_dsi_pixel_format_to_bpp(dsi->format), dsi->lanes);

	return 0;
}

static const struct of_device_id diasom_of_match[] = {
	{
		.compatible = "chongzhou,cz101b4001",
		.data = &cz101b4001_desc
	},
	{
		.compatible = "radxa,display-10hd-ad001",
		.data = &cz101b4001_desc
	},
	{
		.compatible = "raystar,display-rfh1010j-ayh",
		.data = &rfh1010j_desc
	},
	{ }
};
MODULE_DEVICE_TABLE(of, diasom_of_match);

static struct mipi_dsi_driver diasom_driver = {
	.probe = diasom_dsi_probe,
	.driver = {
		.name = "diasom-panel",
		.of_match_table = diasom_of_match,
	},
};
module_mipi_dsi_driver(diasom_driver);
