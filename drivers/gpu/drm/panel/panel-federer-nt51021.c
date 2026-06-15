// SPDX-License-Identifier: GPL-2.0-only
/*
 * DRM Driver for BOE NT51021 1200p Video Mode Panel
 * Based on Huawei MediaPad T2 Pro (Federer) Vendor Sources
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct huawei_nt51021 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data *supplies;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bl_gpio;            /* huawei,platform-enable-vled (GPIO 97) */
	struct gpio_desc *power_blk_gpio;     /* huawei,platform-power-blk (GPIO 109) */
	struct gpio_desc *power_panel_gpio;   /* huawei,platform-power-panel (GPIO 32) */
	bool bl_enabled;
};

static const struct regulator_bulk_data huawei_nt51021_supplies[] = {
	{ .supply = "vddio" },
};

#define NT51021_REG_BKLT_PWM 0x9f

static inline
struct huawei_nt51021 *to_huawei_nt51021(struct drm_panel *panel)
{
	return container_of_const(panel, struct huawei_nt51021, panel);
}

static void huawei_nt51021_reset(struct huawei_nt51021 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(30);
}

static int huawei_nt51021_on(struct huawei_nt51021 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	/* 1. Optional Huawei Write-Protect Block (just in case) */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);
	mipi_dsi_usleep_range(&dsi_ctx, 5000, 6000);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x01, 0x00);
	mipi_dsi_msleep(&dsi_ctx, 30);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);
	mipi_dsi_usleep_range(&dsi_ctx, 1000, 2000);

	/* 2. Page 0 Initialisierung */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x83, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x84, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x8c, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xcd, 0x6c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xc8, 0xfc);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, NT51021_REG_BKLT_PWM, 0x00); /* 0x9f */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x97, 0x00);

	/* Page-Switch & Settings */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x83, 0xbb);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x84, 0x22);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x96, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x90, 0xc0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x91, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x9a, 0x10);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x94, 0x78);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x95, 0xb1);

	/* Gamma / Color */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa1, 0xff);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa2, 0xfa);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa3, 0xf3);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa4, 0xed);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa5, 0xe7);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa6, 0xe2);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa7, 0xdc);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa8, 0xd7);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa9, 0xd1);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xaa, 0xcc);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb4, 0x1c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb5, 0x38);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb6, 0x30);

	/* Page-Switch & Display Tuning */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x83, 0xaa);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x84, 0x11);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xa9, 0x4b);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x85, 0x04);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x86, 0x08);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x9c, 0x10);

	/* Page 0 */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x83, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x84, 0x00);
	mipi_dsi_usleep_range(&dsi_ctx, 1000, 2000);

	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	/* Write Protect close
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0x00); */

	return dsi_ctx.accum_err;
}

static int huawei_nt51021_off(struct huawei_nt51021 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);
	mipi_dsi_usleep_range(&dsi_ctx, 5000, 6000);
	/*mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	msleep(80);*/
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	msleep(120);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0x00);

	return dsi_ctx.accum_err;
}

static int huawei_nt51021_prepare(struct drm_panel *panel)
{
	struct huawei_nt51021 *ctx = to_huawei_nt51021(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(huawei_nt51021_supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable vddio-regulator: %d\n", ret);
		return ret;
	}
	usleep_range(1000, 2000);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	gpiod_set_value_cansleep(ctx->power_blk_gpio, 1);   /* disp_power_backlight */
	gpiod_set_value_cansleep(ctx->power_panel_gpio, 1); /* disp_power_panel */
	msleep(500);

	huawei_nt51021_reset(ctx);

	ret = huawei_nt51021_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		gpiod_set_value_cansleep(ctx->power_blk_gpio, 0);
		gpiod_set_value_cansleep(ctx->power_panel_gpio, 0);
		regulator_bulk_disable(ARRAY_SIZE(huawei_nt51021_supplies), ctx->supplies);
		return ret;
	}

	return 0;
}

static int huawei_nt51021_enable(struct drm_panel *panel)
{
	struct huawei_nt51021 *ctx = to_huawei_nt51021(panel);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 20);
	
	return dsi_ctx.accum_err;
}

static int huawei_nt51021_disable(struct drm_panel *panel)
{
	struct huawei_nt51021 *ctx = to_huawei_nt51021(panel);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_usleep_range(&dsi_ctx, 5000, 6000);
	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	msleep(80);

	return dsi_ctx.accum_err;
}

static int huawei_nt51021_unprepare(struct drm_panel *panel)
{
	struct huawei_nt51021 *ctx = to_huawei_nt51021(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = huawei_nt51021_off(ctx);
	if (ret < 0)
	dev_err(dev, "Failed to un-initialize panel: %d\n", ret);
	msleep(100);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	gpiod_set_value_cansleep(ctx->power_blk_gpio, 0);
	gpiod_set_value_cansleep(ctx->power_panel_gpio, 0);

	regulator_bulk_disable(ARRAY_SIZE(huawei_nt51021_supplies), ctx->supplies);

	return 0;
}

static const struct drm_display_mode huawei_nt51021_mode = {
	.clock = (1200 + 64 + 4 + 36) * (1920 + 104 + 2 + 24) * 60 / 1000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 64,
	.hsync_end = 1200 + 64 + 4,
	.htotal = 1200 + 64 + 4 + 36,
	.vdisplay = 1920,
	.vsync_start = 1920 + 104,
	.vsync_end = 1920 + 104 + 2,
	.vtotal = 1920 + 104 + 2 + 24,
	.width_mm = 135,
	.height_mm = 217,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int huawei_nt51021_get_modes(struct drm_panel *panel,
					  struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &huawei_nt51021_mode);
}

static const struct drm_panel_funcs huawei_nt51021_panel_funcs = {
	.prepare = huawei_nt51021_prepare,
	.enable = huawei_nt51021_enable,
	.disable = huawei_nt51021_disable,
	.unprepare = huawei_nt51021_unprepare,
	.get_modes = huawei_nt51021_get_modes,
};

static int huawei_nt51021_set_brightness(struct mipi_dsi_device *dsi, u16 brightness)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };
	u8 val = (u8)brightness;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x83, 0x00); /* chang_page0_index0 */
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x84, 0x00); /* chang_page0_index1 */

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, NT51021_REG_BKLT_PWM, val);

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0x8f, 0x00);

	/* 
	 * HINWEIS zur Modus-Rückstellung:
	 * Im Mainline-Framework für MSM-SoCs (Qualcomm) belässt man nach der 
	 * Helligkeitsänderung das Flag im HS-Modus, da das eigentliche Video-Streaming 
	 * ebenfalls im HS-Modus läuft. Ein hartes Erzwingen von LPM an dieser Stelle 
	 * kann den Video-Stream zerstören.
	 */

	return dsi_ctx.accum_err;
}

static int huawei_nt51021_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct huawei_nt51021 *ctx = mipi_dsi_get_drvdata(dsi);
	u16 brightness = backlight_get_brightness(bl);
	bool want_on = !!brightness;

	if (want_on && !ctx->bl_enabled) {
		msleep(20);
		gpiod_set_value_cansleep(ctx->bl_gpio, 1); /* hw_panel_bias_en(1) */
		msleep(20);
		ctx->bl_enabled = true;
	} 
	else if (!want_on && ctx->bl_enabled) {
		huawei_nt51021_set_brightness(dsi, 0);
		msleep(20);
		gpiod_set_value_cansleep(ctx->bl_gpio, 0); /* hw_panel_bias_en(0) */
		msleep(100);
		ctx->bl_enabled = false;
		return 0;
	}

	return huawei_nt51021_set_brightness(dsi, brightness);
}

static const struct backlight_ops huawei_nt51021_bl_ops = {
	.update_status = huawei_nt51021_bl_update_status,
};

static struct backlight_device *
huawei_nt51021_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 112,
		.max_brightness = 255,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					&huawei_nt51021_bl_ops, &props);
}

static int huawei_nt51021_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct huawei_nt51021 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ret = devm_regulator_bulk_get_const(dev,
					    ARRAY_SIZE(huawei_nt51021_supplies),
					    huawei_nt51021_supplies,
					    &ctx->supplies);
	if (ret < 0)
		return ret;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
					if (IS_ERR(ctx->reset_gpio))
					return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "Failed to get reset-gpios\n");

	ctx->bl_gpio = devm_gpiod_get(dev, "backlight", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->bl_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->bl_gpio), "Failed to get backlight-gpios\n");

	ctx->power_blk_gpio = devm_gpiod_get(dev, "power-blk", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->power_blk_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->power_blk_gpio), "Failed to get power-blk-gpio\n");

	ctx->power_panel_gpio = devm_gpiod_get(dev, "power-panel", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->power_panel_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->power_panel_gpio), "Failed to get power-panel-gpio\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE;
	/*MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM;*/
			  /*MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;*/
	/*MIPI_DSI_MODE_VIDEO | //orig
		MIPI_DSI_MODE_NO_EOT_PACKET |
		MIPI_DSI_MODE_LPM;*/

	drm_panel_init(&ctx->panel, dev, &huawei_nt51021_panel_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;
	ctx->bl_enabled = false;

	ctx->panel.backlight = huawei_nt51021_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight), "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void huawei_nt51021_remove(struct mipi_dsi_device *dsi)
{
	struct huawei_nt51021 *ctx = mipi_dsi_get_drvdata(dsi);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id huawei_nt51021_of_match[] = {
	{ .compatible = "huawei,boe-nt51021" },
	{ }
};
MODULE_DEVICE_TABLE(of, huawei_nt51021_of_match);

static struct mipi_dsi_driver huawei_nt51021_driver = {
	.probe = huawei_nt51021_probe,
	.remove = huawei_nt51021_remove,
	.driver = {
	.name = "panel-federer-nt51021",
	.of_match_table = huawei_nt51021_of_match,
	},
};
module_mipi_dsi_driver(huawei_nt51021_driver);

MODULE_DESCRIPTION("DRM driver for BOE_NT51021_10_1200P_VIDEO");
MODULE_LICENSE("GPL v2");
