// SPDX-License-Identifier: GPL-2.0-only
/*
 * Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
 *  Copyright (c) 2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Dzmitry Sankouski <dsankouski@gmail.com>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_panel.h>

struct s6e3ha8 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[3];
};

static inline
struct s6e3ha8 *to_s6e3ha8_amb577px01_wqhd(struct drm_panel *panel)
{
	return container_of(panel, struct s6e3ha8, panel);
}

#define S6E3HA8_TEST_KEY_ON_LVL1	0x9f, 0xa5, 0xa5
#define S6E3HA8_TEST_KEY_OFF_LVL1	0x9f, 0x5a, 0x5a
#define S6E3HA8_TEST_KEY_ON_LVL2	0xf0, 0x5a, 0x5a
#define S6E3HA8_TEST_KEY_OFF_LVL2	0xf0, 0xa5, 0xa5
#define S6E3HA8_TEST_KEY_ON_LVL3	0xfc, 0x5a, 0x5a
#define S6E3HA8_TEST_KEY_OFF_LVL3	0xfc, 0xa5, 0xa5
#define S6E3HA8_AFC_OFF			0xe2, 0x00, 0x00

static void s6e3ha8_amb577px01_wqhd_reset(struct s6e3ha8 *priv)
{
	gpiod_set_value_cansleep(priv->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(priv->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(priv->reset_gpio, 1);
	usleep_range(5000, 6000);
}

static int s6e3ha8_amb577px01_wqhd_on(struct s6e3ha8 *priv)
{
	struct mipi_dsi_device *dsi = priv->dsi;
	struct device *dev = &dsi->dev;
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL1);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);

	ret = mipi_dsi_compression_mode(dsi, true);
	if (ret < 0) {
		dev_err(dev, "Failed to set compression mode: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	usleep_range(5000, 6000);

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xf2, 0x13);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);

	usleep_range(10000, 11000);

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xf2, 0x13);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);

	/* OMOK setting 1 (Initial setting) - Scaler Latch Setting Guide */
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xb0, 0x07);
	/* latch setting 1 : Scaler on/off & address setting & PPS setting -> Image update latch */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xf2, 0x3c, 0x10);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xb0, 0x0b);
	/* latch setting 2 : Ratio change mode -> Image update latch */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xf2, 0x30);
	/* OMOK setting 2 - Seamless setting guide : WQHD */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x2a, 0x00, 0x00, 0x05, 0x9f); /* CASET */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x2b, 0x00, 0x00, 0x0b, 0x8f); /* PASET */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xba, 0x01); /* scaler setup : scaler off */
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x35, 0x00); /* TE Vsync ON */
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xed, 0x4c); /* ERR_FG */
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL3);
	/* FFC Setting 897.6Mbps */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xc5, 0x0d, 0x10, 0xb4, 0x3e, 0x01);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL3);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xb9,
				   0x00, 0xb0, 0x81, 0x09, 0x00, 0x00, 0x00,
				   0x11, 0x03); /* TSP HSYNC Setting */
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xb0, 0x03);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xf6, 0x43);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	/* Brightness condition set */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xca,
				   0x07, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
				   0x80, 0x80, 0x80, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xb1, 0x00, 0x0c); /* AID Set : 0% */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xb5,
				   0x19, 0xdc, 0x16, 0x01, 0x34, 0x67, 0x9a,
				   0xcd, 0x01, 0x22, 0x33, 0x44, 0x00, 0x00,
				   0x05, 0x55, 0xcc, 0x0c, 0x01, 0x11, 0x11,
				   0x10); /* MPS/ELVSS Setting */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xf4, 0xeb, 0x28); /* VINT */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xf7, 0x03); /* Gamma, LTPS(AID) update */
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL1);

	return 0;
}

static int s6e3ha8_enable(struct drm_panel *panel)
{
	struct s6e3ha8 *priv = to_s6e3ha8_amb577px01_wqhd(panel);
	struct mipi_dsi_device *dsi = priv->dsi;
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL1);
	ctx.accum_err = mipi_dsi_dcs_set_display_on(dsi);
	mipi_dsi_dcs_write_seq_multi(&ctx,  S6E3HA8_TEST_KEY_OFF_LVL1);

	return ctx.accum_err;
}

static int s6e3ha8_disable(struct drm_panel *panel)
{
	struct s6e3ha8 *priv = to_s6e3ha8_amb577px01_wqhd(panel);
	struct mipi_dsi_device *dsi = priv->dsi;
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL1);
	ctx.accum_err = mipi_dsi_dcs_set_display_off(dsi);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL1);
	msleep(20);

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL2);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_AFC_OFF);
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL2);

	msleep(160);

	return 0;
}

static int s6e3ha8_amb577px01_wqhd_prepare(struct drm_panel *panel)
{
	struct s6e3ha8 *priv = to_s6e3ha8_amb577px01_wqhd(panel);
	struct mipi_dsi_device *dsi = priv->dsi;
	struct device *dev = &dsi->dev;
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(priv->supplies), priv->supplies);
        if (ret < 0)
        	return ret;
	msleep(120);
	s6e3ha8_amb577px01_wqhd_reset(priv);
	ret = s6e3ha8_amb577px01_wqhd_on(priv);

	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(priv->reset_gpio, 1);
		goto err;
	}

	drm_dsc_pps_payload_pack(&pps, &priv->dsc);

	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_ON_LVL1);
	ret = mipi_dsi_picture_parameter_set(priv->dsi, &pps);
	if (ret < 0) {
		dev_err(panel->dev, "failed to transmit PPS: %d\n", ret);
		return ret;
	}
	mipi_dsi_dcs_write_seq_multi(&ctx, S6E3HA8_TEST_KEY_OFF_LVL1);

	msleep(28);

	return 0;
err:
	regulator_bulk_disable(ARRAY_SIZE(priv->supplies), priv->supplies);
	return ret;
}

static int s6e3ha8_amb577px01_wqhd_unprepare(struct drm_panel *panel)
{
	struct s6e3ha8 *priv = to_s6e3ha8_amb577px01_wqhd(panel);

	return regulator_bulk_disable(ARRAY_SIZE(priv->supplies), priv->supplies);
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
	return drm_connector_helper_get_modes_fixed(connector, &s6e3ha8_amb577px01_wqhd_mode);
}

static const struct drm_panel_funcs s6e3ha8_amb577px01_wqhd_panel_funcs = {
	.prepare = s6e3ha8_amb577px01_wqhd_prepare,
	.unprepare = s6e3ha8_amb577px01_wqhd_unprepare,
	.get_modes = s6e3ha8_amb577px01_wqhd_get_modes,
	.enable = s6e3ha8_enable,
	.disable = s6e3ha8_disable,
};

static int s6e3ha8_amb577px01_wqhd_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct s6e3ha8 *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->supplies[0].supply = "vdd3";
	priv->supplies[1].supply = "vci";
	priv->supplies[2].supply = "vddr";

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(priv->supplies),
				      priv->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to get regulators: %d\n", ret);
		return ret;
	}

	priv->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(priv->reset_gpio),
				     "Failed to get reset-gpios\n");

	priv->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, priv);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS |
		MIPI_DSI_MODE_VIDEO_NO_HFP | MIPI_DSI_MODE_VIDEO_NO_HBP |
		MIPI_DSI_MODE_VIDEO_NO_HSA | MIPI_DSI_MODE_NO_EOT_PACKET;

	drm_panel_init(&priv->panel, dev, &s6e3ha8_amb577px01_wqhd_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	priv->panel.prepare_prev_first = true;

	drm_panel_add(&priv->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &priv->dsc;

	priv->dsc.dsc_version_major = 1;
	priv->dsc.dsc_version_minor = 1;

	priv->dsc.slice_height = 40;
	priv->dsc.slice_width = 720;
	WARN_ON(1440 % priv->dsc.slice_width);
	priv->dsc.slice_count = 1440 / priv->dsc.slice_width;
	priv->dsc.bits_per_component = 8;
	priv->dsc.bits_per_pixel = 8 << 4; /* 4 fractional bits */
	priv->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&priv->panel);
		return ret;
	}

	return 0;
}

static void s6e3ha8_amb577px01_wqhd_remove(struct mipi_dsi_device *dsi)
{
	struct s6e3ha8 *priv = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&priv->panel);
}

static const struct of_device_id s6e3ha8_amb577px01_wqhd_of_match[] = {
	{ .compatible = "samsung,s6e3ha8" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, s6e3ha8_amb577px01_wqhd_of_match);

static struct mipi_dsi_driver s6e3ha8_amb577px01_wqhd_driver = {
	.probe = s6e3ha8_amb577px01_wqhd_probe,
	.remove = s6e3ha8_amb577px01_wqhd_remove,
	.driver = {
		.name = "panel-s6e3ha8",
		.of_match_table = s6e3ha8_amb577px01_wqhd_of_match,
	},
};
module_mipi_dsi_driver(s6e3ha8_amb577px01_wqhd_driver);

MODULE_AUTHOR("Dzmitry Sankouski <dsankouski@gmail.com>");
MODULE_DESCRIPTION("DRM driver for S6E3HA8 panel");
MODULE_LICENSE("GPL");
