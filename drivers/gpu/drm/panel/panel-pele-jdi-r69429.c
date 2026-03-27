// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct pele_jdi_r69429 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data *supplies;
	struct gpio_desc *reset_gpio;
	//struct gpio_desc *vled_gpio;
};

static const struct regulator_bulk_data pele_jdi_r69429_supplies[] = {
	{ .supply = "vcc" },    /* GPIO 2 */
	{ .supply = "vsp" },    /* GPIO 97 */
	{ .supply = "vsn" },    /* GPIO 32 */
	{ .supply = "vled" },   /* GPIO 109 */
	{ .supply = "bl" },     /* GPIO 3 */
};

static inline
struct pele_jdi_r69429 *to_pele_jdi_r69429(struct drm_panel *panel)
{
	return container_of_const(panel, struct pele_jdi_r69429, panel);
}

static void pele_jdi_r69429_reset(struct pele_jdi_r69429 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
}

static int pele_jdi_r69429_on(struct pele_jdi_r69429 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	// Der Dump erzwingt LP-Mode für die Initialisierung
	ctx->dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	// B0: Hersteller-Spezifischer Unlock / Page Select
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb0, 0x00);

	// B3: Power Control / VGH/VGL Spannungen
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb3, 0x04, 0x08, 0x00, 0x22, 0x00);

	// B6: Interface Mode
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb6, 0x3a, 0xd3);

	// B8, B9, BA: Panel Timing & Source Driver Settings
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb8, 0x07, 0x90, 0x1e, 0x00, 0x1e, 0x32);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb9, 0x07, 0x82, 0x3c, 0x00, 0x3c, 0x87);
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xba, 0x07, 0x9e, 0x20, 0x00, 0x20, 0x8f);

	// CE: Gamma-Korrektur (Sehr wichtig für die Farbdarstellung)
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xce,
					 0x7d, 0x40, 0x43, 0x49, 0x55, 0x62,
					 0x71, 0x82, 0x94, 0xa8, 0xb9, 0xcb,
					 0xdb, 0xe9, 0xf5, 0xfc, 0xff, 0x01,
					 0x38, 0x02, 0x02, 0x44, 0x24);

	// D6: Panel-Konfiguration
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xd6, 0x01);

	// C6: Oszillator / Clock Settings
	mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xc6,
					 0x78, 0x01, 0x45, 0x05, 0x67, 0x67,
					 0x0a, 0x01, 0x01, 0x01, 0x01, 0x01,
					 0x01, 0x01, 0x01, 0x01, 0x01, 0x0a,
					 0x19, 0x05);

	// 35: Tear Effect ON (Vblank)
	mipi_dsi_dcs_set_tear_on_multi(&dsi_ctx, MIPI_DSI_DCS_TEAR_MODE_VBLANK);

	// 36: Address Mode (RGB/BGR etc)
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_ADDRESS_MODE, 0x00);

	// 3A: Pixel Format (0x77 = 24-bit RGB888)
	mipi_dsi_dcs_set_pixel_format_multi(&dsi_ctx, 0x77);

	// Spalten & Zeilen Adressierung (Full Resolution 1200x1920)
	mipi_dsi_dcs_set_column_address_multi(&dsi_ctx, 0x0000, 0x04af);
	mipi_dsi_dcs_set_page_address_multi(&dsi_ctx, 0x0000, 0x077f);

	// 51: Brightness Level (Initial 0, wird später vom Backlight-Treiber gesetzt)
	mipi_dsi_dcs_set_display_brightness_multi(&dsi_ctx, 0x00);

	// 53: Display Control (Brightness an, BLC an)
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x24);

	// 5E: CABC Minimum Brightness
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0x06);

	// 55: CABC Mode (0x01 = UI Mode laut DTS)
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_POWER_SAVE, 0x01);

	// Exit Sleep & Display ON
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 20);

	return dsi_ctx.accum_err;
}

static int pele_jdi_r69429_off(struct pele_jdi_r69429 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	ctx->dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 20);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 80);

	return dsi_ctx.accum_err;
}

static int pele_jdi_r69429_prepare(struct drm_panel *panel)
{
	struct pele_jdi_r69429 *ctx = to_pele_jdi_r69429(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	dev_dbg(dev, "Preparing panel: Enabling regulators and GPIOs\n");
	ret = regulator_bulk_enable(ARRAY_SIZE(pele_jdi_r69429_supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}
	msleep(20);

	/*gpiod_set_value_cansleep(ctx->vled_gpio, 1);
	msleep(10);*/

	pele_jdi_r69429_reset(ctx);

	dev_dbg(dev, "Sending init sequence...\n");
	ret = pele_jdi_r69429_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel commands: %d\n", ret);
		goto err_on;
	}
	dev_dbg(dev, "Panel is now ON\n");

	return 0;

err_on:
	//gpiod_set_value_cansleep(ctx->vled_gpio, 0);
	regulator_bulk_disable(ARRAY_SIZE(pele_jdi_r69429_supplies), ctx->supplies);
	return ret;
}

static int pele_jdi_r69429_unprepare(struct drm_panel *panel)
{
	struct pele_jdi_r69429 *ctx = to_pele_jdi_r69429(panel);

	pele_jdi_r69429_off(ctx);

	/*gpiod_set_value_cansleep(ctx->vled_gpio, 0);
	msleep(20);*/

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(10);

	regulator_bulk_disable(ARRAY_SIZE(pele_jdi_r69429_supplies), ctx->supplies);
	msleep(100);

	return 0;
}

static const struct drm_display_mode pele_jdi_r69429_mode = {
	.clock = (1200 + 80 + 16 + 64) * (1920 + 32 + 16 + 32) * 60 / 1000,
	.hdisplay = 1200,
	.hsync_start = 1200 + 80,
	.hsync_end = 1200 + 80 + 16,
	.htotal = 1200 + 80 + 16 + 64,
	.vdisplay = 1920,
	.vsync_start = 1920 + 32,
	.vsync_end = 1920 + 32 + 16,
	.vtotal = 1920 + 32 + 16 + 32,
	.width_mm = 95,
	.height_mm = 151,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int pele_jdi_r69429_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &pele_jdi_r69429_mode);
}

static const struct drm_panel_funcs pele_jdi_r69429_panel_funcs = {
	.prepare = pele_jdi_r69429_prepare,
	.unprepare = pele_jdi_r69429_unprepare,
	.get_modes = pele_jdi_r69429_get_modes,
};

static int pele_jdi_r69429_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static int pele_jdi_r69429_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness & 0xff;
}

static const struct backlight_ops pele_jdi_r69429_bl_ops = {
	.update_status = pele_jdi_r69429_bl_update_status,
	.get_brightness = pele_jdi_r69429_bl_get_brightness,
};

static struct backlight_device *
pele_jdi_r69429_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 255,
		.max_brightness = 255,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &pele_jdi_r69429_bl_ops, &props);
}

static int pele_jdi_r69429_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct pele_jdi_r69429 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ret = devm_regulator_bulk_get_const(dev,
					    ARRAY_SIZE(pele_jdi_r69429_supplies),
					    pele_jdi_r69429_supplies,
					    &ctx->supplies);
	if (ret < 0)
		return ret;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	/*ctx->vled_gpio = devm_gpiod_get(dev, "vled", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->vled_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->vled_gpio),
				     "Failed to get vled-gpios\n");*/

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &pele_jdi_r69429_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = pele_jdi_r69429_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");
            
	drm_panel_add(&ctx->panel);
	dev_dbg(dev, "Probing JDI R69429: Lanes=%d, Format=%d, Flags=0x%lx\n",
		dsi->lanes, dsi->format, dsi->mode_flags);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}
	dev_info(dev, "JDI R69429 panel probed successfully\n");

	return 0;
}

static void pele_jdi_r69429_remove(struct mipi_dsi_device *dsi)
{
	struct pele_jdi_r69429 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id pele_jdi_r69429_of_match[] = {
	{ .compatible = "pele,jdi-r69429" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, pele_jdi_r69429_of_match);

static struct mipi_dsi_driver pele_jdi_r69429_driver = {
	.probe = pele_jdi_r69429_probe,
	.remove = pele_jdi_r69429_remove,
	.driver = {
		.name = "panel-pele-jdi-r69429",
		.of_match_table = pele_jdi_r69429_of_match,
	},
};
module_mipi_dsi_driver(pele_jdi_r69429_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for JDI_R69429_7_1200P_CMD");
MODULE_LICENSE("GPL");
