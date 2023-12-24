// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2023 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct s6e3ha8_amb577px01_wqhd {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[3];
	bool prepared;
};

static inline
struct s6e3ha8_amb577px01_wqhd *to_s6e3ha8_amb577px01_wqhd(struct drm_panel *panel)
{
	return container_of(panel, struct s6e3ha8_amb577px01_wqhd, panel);
}

static int s6e3ha8_power_on(struct s6e3ha8_amb577px01_wqhd *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	return 0;
}

static int s6e3ha8_power_off(struct s6e3ha8_amb577px01_wqhd *ctx)
{
	return regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
}

static void s6e3ha8_amb577px01_wqhd_reset(struct s6e3ha8_amb577px01_wqhd *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
}

static int s6e3ha8_amb577px01_wqhd_on(struct s6e3ha8_amb577px01_wqhd *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_generic_write_seq(dsi, 0x9f, 0xa5, 0xa5);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);

	ret = mipi_dsi_compression_mode(dsi, true);
	if (ret < 0) {
		dev_err(dev, "Failed to set compression mode: %d\n", ret);
		return ret;
	}

	mipi_dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	usleep_range(10000, 11000);

	mipi_dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xb0, 0x07);
	mipi_dsi_generic_write_seq(dsi, 0xf2, 0x3c, 0x10);
	mipi_dsi_generic_write_seq(dsi, 0xb0, 0x0b);
	mipi_dsi_generic_write_seq(dsi, 0xf2, 0x30);
	mipi_dsi_generic_write_seq(dsi, 0x2a, 0x00, 0x00, 0x05, 0x9f);
	mipi_dsi_generic_write_seq(dsi, 0x2b, 0x00, 0x00, 0x0b, 0x8f);
	mipi_dsi_generic_write_seq(dsi, 0xba, 0x01);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_generic_write_seq(dsi, 0x35, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xed, 0x4c);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_generic_write_seq(dsi, 0xfc, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xc5, 0x0d, 0x10, 0xb4, 0x3e, 0x01);
	mipi_dsi_generic_write_seq(dsi, 0xfc, 0xa5, 0xa5);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xb9,
				   0x00, 0xb0, 0x81, 0x09, 0x00, 0x00, 0x00,
				   0x11, 0x03);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xb0, 0x03);
	mipi_dsi_generic_write_seq(dsi, 0xf6, 0x43);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xca,
				   0x07, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x00, 0x00, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0xb1, 0x00, 0x0c);
	mipi_dsi_generic_write_seq(dsi, 0xb5,
				   0x19, 0xdc, 0x16, 0x01, 0x34, 0x67, 0x9a,
				   0xcd, 0x01, 0x22, 0x33, 0x44, 0x00, 0x00,
				   0x05, 0x55, 0xcc, 0x0c, 0x01, 0x11, 0x11,
				   0x10);
	mipi_dsi_generic_write_seq(dsi, 0xf4, 0xeb, 0x28);
	mipi_dsi_generic_write_seq(dsi, 0xf7, 0x03);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_generic_write_seq(dsi, 0x9f, 0x5a, 0x5a);

	return 0;
}

static int s6e3ha8_amb577px01_wqhd_off(struct s6e3ha8_amb577px01_wqhd *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_generic_write_seq(dsi, 0x9f, 0xa5, 0xa5);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}

	mipi_dsi_generic_write_seq(dsi, 0x9f, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_generic_write_seq(dsi, 0xe2, 0x00, 0x00);
	mipi_dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	msleep(20);
	mipi_dsi_generic_write_seq(dsi, 0x9f, 0xa5, 0xa5);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}

	mipi_dsi_generic_write_seq(dsi, 0x9f, 0x5a, 0x5a);
	msleep(160);

	return 0;
}

static int s6e3ha8_amb577px01_wqhd_prepare(struct drm_panel *panel)
{
	struct s6e3ha8_amb577px01_wqhd *ctx = to_s6e3ha8_amb577px01_wqhd(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	if (ctx->prepared)
		return 0;

	s6e3ha8_power_on(ctx);
	msleep(120);
	s6e3ha8_amb577px01_wqhd_reset(ctx);
	ret = s6e3ha8_amb577px01_wqhd_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		goto err;
	}

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);

	ret = mipi_dsi_picture_parameter_set(ctx->dsi, &pps);
	if (ret < 0) {
		dev_err(panel->dev, "failed to transmit PPS: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_compression_mode(ctx->dsi, true);
	if (ret < 0) {
		dev_err(dev, "failed to enable compression mode: %d\n", ret);
		return ret;
	}

	msleep(28); /* TODO: Is this panel-dependent? */

	ctx->prepared = true;
err:
    s6e3ha8_power_off(ctx);
	return ret;
}

static int s6e3ha8_amb577px01_wqhd_unprepare(struct drm_panel *panel)
{
	struct s6e3ha8_amb577px01_wqhd *ctx = to_s6e3ha8_amb577px01_wqhd(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = s6e3ha8_amb577px01_wqhd_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	s6e3ha8_power_off(ctx);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode s6e3ha8_amb577px01_wqhd_mode = {
	.clock = (1440 + 116 + 44 + 120) * (2960 + 120 + 80 + 124) * 60 / 1000,
	.hdisplay = 1440,
	.hsync_start = 1440 + 116,
	.hsync_end = 1440 + 116 + 44,
	.htotal = 1440 + 116 + 44 + 120,
	.vdisplay = 2960,
	.vsync_start = 2960 + 120,
	.vsync_end = 2960 + 120 + 80,
	.vtotal = 2960 + 120 + 80 + 124,
	.width_mm = 64,
	.height_mm = 132,
};

static int s6e3ha8_amb577px01_wqhd_get_modes(struct drm_panel *panel,
					     struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &s6e3ha8_amb577px01_wqhd_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs s6e3ha8_amb577px01_wqhd_panel_funcs = {
	.prepare = s6e3ha8_amb577px01_wqhd_prepare,
	.unprepare = s6e3ha8_amb577px01_wqhd_unprepare,
	.get_modes = s6e3ha8_amb577px01_wqhd_get_modes,
};

static int s6e3ha8_amb577px01_wqhd_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness_large(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

// TODO: Check if /sys/class/backlight/.../actual_brightness actually returns
// correct values. If not, remove this function.
//static int s6e3ha8_amb577px01_wqhd_bl_get_brightness(struct backlight_device *bl)
//{
//	struct mipi_dsi_device *dsi = bl_get_data(bl);
//	u16 brightness;
//	int ret;
//
//	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
//
//	ret = mipi_dsi_dcs_get_display_brightness_large(dsi, &brightness);
//	if (ret < 0)
//		return ret;
//
//	dsi->mode_flags |= MIPI_DSI_MODE_LPM;
//
//	return brightness;
//}

static const struct backlight_ops s6e3ha8_amb577px01_wqhd_bl_ops = {
	.update_status = s6e3ha8_amb577px01_wqhd_bl_update_status,
//	.get_brightness = s6e3ha8_amb577px01_wqhd_bl_get_brightness,
};

static struct backlight_device *
s6e3ha8_amb577px01_wqhd_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 50000,
		.max_brightness = 50000,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &s6e3ha8_amb577px01_wqhd_bl_ops, &props);
}

static int s6e3ha8_amb577px01_wqhd_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct s6e3ha8_amb577px01_wqhd *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supplies[0].supply = "vdd3";
	ctx->supplies[1].supply = "vci";
	ctx->supplies[2].supply = "vddr";

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to get regulators: %d\n", ret);
		return ret;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &s6e3ha8_amb577px01_wqhd_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = s6e3ha8_amb577px01_wqhd_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;

	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;

	/* TODO: Pass slice_per_pkt = 1 */
	ctx->dsc.slice_height = 40;
	ctx->dsc.slice_width = 720;
	/*
	 * TODO: hdisplay should be read from the selected mode once
	 * it is passed back to drm_panel (in prepare?)
	 */
	WARN_ON(1440 % ctx->dsc.slice_width);
	ctx->dsc.slice_count = 1440 / ctx->dsc.slice_width;
	ctx->dsc.bits_per_component = 8;
	ctx->dsc.bits_per_pixel = 8 << 4; /* 4 fractional bits */
	ctx->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void s6e3ha8_amb577px01_wqhd_remove(struct mipi_dsi_device *dsi)
{
	struct s6e3ha8_amb577px01_wqhd *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id s6e3ha8_amb577px01_wqhd_of_match[] = {
	{ .compatible = "mdss,s6e3ha8-amb577px01-wqhd" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s6e3ha8_amb577px01_wqhd_of_match);

static struct mipi_dsi_driver s6e3ha8_amb577px01_wqhd_driver = {
	.probe = s6e3ha8_amb577px01_wqhd_probe,
	.remove = s6e3ha8_amb577px01_wqhd_remove,
	.driver = {
		.name = "panel-s6e3ha8-amb577px01-wqhd",
		.of_match_table = s6e3ha8_amb577px01_wqhd_of_match,
	},
};
module_mipi_dsi_driver(s6e3ha8_amb577px01_wqhd_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for ss_dsi_panel_S6E3HA8_AMB577PX01_WQHD");
MODULE_LICENSE("GPL");
