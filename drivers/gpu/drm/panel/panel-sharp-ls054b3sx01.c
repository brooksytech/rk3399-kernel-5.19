// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Maya Matuszczyk <maccraft123mc@gmail.com>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct sharp_ls054 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *iovcc_supply;
	struct regulator *vsp_supply;
	struct regulator *vsn_supply;
	struct gpio_desc *reset_gpio;
	enum drm_panel_orientation orientation;
	bool prepared;
};

static inline struct sharp_ls054 *to_sharp_ls054(struct drm_panel *panel)
{
	return container_of(panel, struct sharp_ls054, panel);
}

#define dsi_dcs_write_seq(dsi, cmd, seq...) do {                        \
                static const u8 b[] = { cmd, seq };                     \
                int ret;                                                \
                ret = mipi_dsi_dcs_write_buffer(dsi, b, ARRAY_SIZE(b)); \
                if (ret < 0)                                            \
                        return ret;                                     \
        } while (0)

#define SHARP_LS054_SETEXTC		0xB9
#define SHARP_LS054_SETSEQUENCE		0xB0
#define SHARP_LS054_SETGAMMACURVE	0xE0
#define SHARP_LS054_SETPOWER		0xB1
#define SHARP_LS054_SETVREF		0xD2
#define SHARP_LS054_SETGIP0		0xD3
#define SHARP_LS054_SETGIP1		0xD5
#define SHARP_LS054_SETGIP2		0xD6
#define SHARP_LS054_SETGIP3		0xD8
#define SHARP_LS054_SETDISP		0xB2
#define SHARP_LS054_SETCYC		0xB4
#define SHARP_LS054_SETMIPI		0xBA
#define SHARP_LS054_SETPTBA		0xBF

static int sharp_ls054_init_sequence(struct sharp_ls054 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	int ret;

	dsi_dcs_write_seq(dsi, SHARP_LS054_SETEXTC,
			0xFF, 0x83, 0x99);
	//dsi_dcs_write_seq(dsi, SHARP_LS054_SETSEQUENCE,
	//		0x00, 0x00, 0x65);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETGAMMACURVE,
			0x01, 0x13, 0x17, 0x34, 0x38, 0x3E, 0x2C, 0x47,
			0x07, 0x0C, 0x0F, 0x12, 0x14, 0x11, 0x13, 0x12,
			0x18, 0x0B, 0x17, 0x07, 0x13, 0x02, 0x14, 0x18,
			0x32, 0x37, 0x3D, 0x29, 0x43, 0x07, 0x0E, 0x0C,
			0x0F, 0x11, 0x10, 0x12, 0x12, 0x18, 0x0C, 0x17,
			0x07, 0x13);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETPOWER,
			0x00, 0x7C, 0x38, 0x35, 0x99, 0x09, 0x22, 0x22,
			0x72, 0xF2, 0x68, 0x58);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETVREF,
			0x99);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETGIP0,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00,
			0x10, 0x05, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07,
			0x07, 0x03, 0x00, 0x00, 0x00, 0x05, 0x08);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETGIP1,
			0x00, 0x00, 0x01, 0x00, 0x03, 0x02, 0x00, 0x00,
			0x00, 0x00, 0x19, 0x00, 0x18, 0x00, 0x21, 0x20,
			0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x32, 0x32, 0x31, 0x31, 0x30, 0x30);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETGIP2,
			0x40, 0x40, 0x02, 0x03, 0x00, 0x01, 0x40, 0x40,
			0x40, 0x40, 0x18, 0x40, 0x19, 0x40, 0x20, 0x21,
			0x40, 0x18, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x40, 0x40, 0x32, 0x32, 0x31, 0x31, 0x30, 0x30);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETGIP3,
			0x28, 0x2A, 0x00, 0x2A, 0x28, 0x02, 0xC0, 0x2A,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x28, 0x02, 0x00, 0x2A, 0x28, 0x02, 0xC0, 0x2A);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETDISP,
			0x00, 0x80, 0x10, 0x7F, 0x05, 0x01, 0x23, 0x4D,
			0x21, 0x01);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETCYC,
			0x00, 0x3F, 0x00, 0x41, 0x00, 0x3D, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x0F, 0x01, 0x02,
			0x05, 0x40, 0x00, 0x00, 0x3A, 0x00, 0x41, 0x00,
			0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
			0x0F, 0x01, 0x02, 0x05, 0x00, 0x00, 0x00, 0x3A);
	dsi_dcs_write_seq(dsi, SHARP_LS054_SETMIPI,
			0x03, 0x82, 0xA0, 0xE5);
	//dsi_dcs_write_seq(dsi, SHARP_LS054_SETPTBA,
	//		0xCF, 0x00, 0x46, 0x00, 0x00, 0x00, 0x02, 0x54,
	//		0x04, 0x61, 0x1C, 0x8C);

	
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0)
	{
		dev_err(&dsi->dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(130);

	dsi_dcs_write_seq(dsi, 0x29);
	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0)
	{
		dev_err(&dsi->dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(50);

	mipi_dsi_dcs_set_display_brightness(dsi, 0xFF);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x00);
	dsi_dcs_write_seq(dsi, 0x53, 0x24); //MIPI_DCS_WRITE_CONTROL_DISPLAY ?
	mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);

	return 0;
}

static int sharp_ls054_prepare(struct drm_panel *panel)
{
	struct sharp_ls054 *ctx = to_sharp_ls054(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	
	ret = regulator_enable(ctx->iovcc_supply);
	if (ret < 0)
		return ret;

	usleep_range(1500, 3000);

	ret = regulator_enable(ctx->vsp_supply);
	if (ret < 0)
		goto err_vsp;

	usleep_range(1500, 3000);

	ret = regulator_enable(ctx->vsn_supply);
	if (ret < 0)
		goto err_on;

	// TODO: move resetting into its own function
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	msleep(200);

	ret = sharp_ls054_init_sequence(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		goto err_on;
	}
	
	ctx->prepared = true;

	return 0;

	// TODO: make sure those labels are ok
err_on:
	regulator_disable(ctx->vsn_supply);

err_vsp:
	regulator_disable(ctx->vsp_supply);

err_iovcc:
	regulator_disable(ctx->iovcc_supply);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);

	return ret;
}

// FIXME
static int sharp_ls054_unprepare(struct drm_panel *panel)
{
	struct sharp_ls054 *ctx = to_sharp_ls054(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(2000, 3000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(5);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);

	usleep_range(500, 1000);
	regulator_disable(ctx->vsn_supply);
	usleep_range(500, 1000);
	regulator_disable(ctx->vsp_supply);
	usleep_range(500, 1000);
	regulator_disable(ctx->iovcc_supply);

	ctx->prepared = false;
	return 0;
}

// FIXME: should we strive for perfect 60hz?
static const struct drm_display_mode sharp_ls054_mode = {
	.clock		= ((1152 + 64 + 8 + 28) * (1920 + 56 + 3 + 6) * 60) / 1000,
	.hdisplay	= 1152,
	.hsync_start	= 1152 + 64,
	.hsync_end	= 1152 + 64 + 8,
	.htotal		= 1152 + 64 + 8 + 28,
	.vdisplay	= 1920,
	.vsync_start	= 1920 + 56,
	.vsync_end	= 1920 + 56 + 3,
	.vtotal		= 1920 + 56 + 3 + 6,
	.width_mm	= 70,
	.height_mm	= 117,
	.flags		= DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

static int sharp_ls054_get_modes(struct drm_panel *panel,
				 struct drm_connector *connector)
{
	struct sharp_ls054 *ctx = to_sharp_ls054(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &sharp_ls054_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);
	drm_connector_set_panel_orientation(connector, ctx->orientation);

	return 1;
}

static const struct drm_panel_funcs sharp_ls054_panel_funcs = {
	.prepare = sharp_ls054_prepare,
	.unprepare = sharp_ls054_unprepare,
	.get_modes = sharp_ls054_get_modes,
};

static int sharp_ls054_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct sharp_ls054 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->iovcc_supply = devm_regulator_get(dev, "iovcc");
	if (IS_ERR(ctx->iovcc_supply))
		return PTR_ERR(ctx->iovcc_supply); // TODO: replace with dev_err_probe

	ctx->vsp_supply = devm_regulator_get(dev, "vsp");
	if (IS_ERR(ctx->vsp_supply))
		return PTR_ERR(ctx->vsp_supply);

	ctx->vsn_supply = devm_regulator_get(dev, "vsn");
	if (IS_ERR(ctx->vsn_supply))
		return PTR_ERR(ctx->vsn_supply);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ret = of_drm_get_panel_orientation(dev->of_node, &ctx->orientation);
	if (ret < 0)
		dev_err(dev, "%pOF: failed to get orientation, %d\n", dev->of_node, ret);

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO_AUTO_VERT;
	// are all those flags really needed?

	drm_panel_init(&ctx->panel, dev, &sharp_ls054_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static int sharp_ls054_remove(struct mipi_dsi_device *dsi)
{
	struct sharp_ls054 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id sharp_ls054b3sx01_of_match[] = {
	{ .compatible = "sharp,ls054b3sx01" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sharp_ls054b3sx01_of_match);

static struct mipi_dsi_driver sharp_ls054_driver = {
	.probe = sharp_ls054_probe,
	.remove = sharp_ls054_remove,
	.driver = {
		.name = "panel-sharp-ls054b3sx01",
		.of_match_table = sharp_ls054b3sx01_of_match,
	},
};
module_mipi_dsi_driver(sharp_ls054_driver);

MODULE_AUTHOR("Maya Matuszczyk <maccraft123mc@gmail.com>");
MODULE_DESCRIPTION("Panel driver for Sharp LS054B3SX01 1152x1080 Video Mode DSI Panel");
MODULE_LICENSE("GPL v2");
