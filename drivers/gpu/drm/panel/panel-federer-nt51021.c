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
    struct gpio_desc *bl_gpio;
    struct gpio_desc *vcc_gpio;
    struct gpio_desc *blpwr_gpio;
    bool bl_enabled;
};

#define NT51021_REG_BKLT_PWM 0x9f

static inline
struct huawei_nt51021 *to_huawei_nt51021(struct drm_panel *panel)
{
    return return container_of_const(panel, struct huawei_nt51021, panel);
}

static void huawei_nt51021_reset(struct huawei_nt51021 *ctx)
{
    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    msleep(20);
    gpiod_set_value_cansleep(ctx->reset_gpio, 0);
    msleep(10);
    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    msleep(20);
    gpiod_set_value_cansleep(ctx->reset_gpio, 0);
    msleep(120);
}

static int huawei_nt51021_on(struct huawei_nt51021 *ctx)
{
    struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

    ctx->dsi->mode_flags |= MIPI_DSI_MODE_LPM;

    /* Write Protect open*/
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);
    mipi_dsi_usleep_range(&dsi_ctx, 5000, 6000);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x01, 0x00);
    mipi_dsi_msleep(&dsi_ctx, 30);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);
    mipi_dsi_usleep_range(&dsi_ctx, 1000, 2000);

    /* Page 0 */
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x83, 0x00);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x84, 0x00);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8c, 0x80);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xcd, 0x6c);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xc8, 0xfc);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, NT51021_REG_BKLT_PWM, 0x00);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x97, 0x00);

    /* Magic Page Switch & Settings */
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x83, 0xbb);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x84, 0x22);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x96, 0x00);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x91, 0xa0);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x9a, 0x10);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x94, 0x78);
    
    /* Gamma / Color */
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa1, 0xff);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa2, 0xfa);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa3, 0xf3);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa4, 0xed);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa5, 0xe7);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa6, 0xe2);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa7, 0xdc);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa8, 0xd7);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa9, 0xd1);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xaa, 0xcc);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb4, 0x1c);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb5, 0x38);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xb6, 0x30);

    /* Final Page Switch */
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x83, 0xaa);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x84, 0x11);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0xa9, 0x4b);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x85, 0x04);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x86, 0x08);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x9c, 0x10);

    /* PWM-Frequenz (Page BB/22) */
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x83, 0xBB); 
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x84, 0x22);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x90, 0xc0);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x95, 0x11);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x94, 0x58); 

    /* Zurück zu Page 0*/
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x83, 0x00);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x84, 0x00);
    mipi_dsi_usleep_range(&dsi_ctx, 1000, 2000);

    /* Display aktivieren */
    mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
    mipi_dsi_msleep(&dsi_ctx, 120);
    mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
    mipi_dsi_msleep(&dsi_ctx, 20);

    /* Write Protect close
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0x00); */

    return dsi_ctx.accum_err;
}

static int huawei_nt51021_off(struct huawei_nt51021 *ctx)
{
    struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);
    mipi_dsi_usleep_range(&dsi_ctx, 5000, 6000);
    mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
    msleep(80);
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

    /* Hardware Power-Up Sequenz */
    gpiod_set_value_cansleep(ctx->vcc_gpio, 1);
    msleep(500);
    gpiod_set_value_cansleep(ctx->blpwr_gpio, 1);
    msleep(50);

    huawei_nt51021_reset(ctx);

    ret = huawei_nt51021_on(ctx);
    if (ret < 0) {
        dev_err(dev, "Failed to initialize panel: %d\n", ret);
        gpiod_set_value_cansleep(ctx->reset_gpio, 1);
        gpiod_set_value_cansleep(ctx->blpwr_gpio, 0);
        msleep(50);
        gpiod_set_value_cansleep(ctx->vcc_gpio, 0);
        return ret;
    }

    return 0;
}

static int huawei_nt51021_unprepare(struct drm_panel *panel)
{
    struct huawei_nt51021 *ctx = to_huawei_nt51021(panel);
    struct device *dev = &ctx->dsi->dev;
    int ret;

    ret = huawei_nt51021_off(ctx);
    if (ret < 0)
        dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

    gpiod_set_value_cansleep(ctx->reset_gpio, 1);
    gpiod_set_value_cansleep(ctx->blpwr_gpio, 0);
    msleep(50);
    gpiod_set_value_cansleep(ctx->vcc_gpio, 0);
    msleep(500);

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
    .unprepare = huawei_nt51021_unprepare,
    .get_modes = huawei_nt51021_get_modes,
};

static int huawei_nt51021_set_brightness(struct mipi_dsi_device *dsi, u16 brightness)
{
    struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };
    u8 val = (u8)brightness;
    u16 temp_val;
    u8 tx_buf[2];

    /* Clamping & Offset gegen Überlauf und Dunkelwerden bei Max */
    if (val > 0) {
        temp_val = (u16)val + 7;
        val = (temp_val > 255) ? 255 : (u8)temp_val;
    }

    if (val >= 23 && val <= 30)
        val = 30;

    tx_buf[0] = NT51021_REG_BKLT_PWM;
    tx_buf[1] = val;

    /* Zwinge Befehle in stabilen LPM Modus */
    dsi->mode_flags |= MIPI_DSI_MODE_LPM;

    //Write Protect öffnen & Page 0 sicherstellen
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0xa5);
/* 
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x83, 0xBB); 
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x84, 0x22);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x90, 0xc0);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x95, 0xb1);
    mipi_dsi_usleep_range(&dsi_ctx, 1000, 1500);  
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x83, 0x00);
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x84, 0x00); */
    
    mipi_dsi_usleep_range(&dsi_ctx, 1000, 1500);

    mipi_dsi_dcs_write_buffer_multi(&dsi_ctx, tx_buf, sizeof(tx_buf));

    /* Write Protect wieder schließen */
    mipi_dsi_generic_write_seq_multi(&dsi_ctx, 0x8f, 0x00);

    return dsi_ctx.accum_err;
}

static int huawei_nt51021_bl_update_status(struct backlight_device *bl)
{
    struct mipi_dsi_device *dsi = bl_get_data(bl);
    struct huawei_nt51021 *ctx = mipi_dsi_get_drvdata(dsi);
    u16 brightness = backlight_get_brightness(bl);
    bool want_on = !!brightness;

    if (want_on && !ctx->bl_enabled) {
        gpiod_set_value_cansleep(ctx->bl_gpio, 1);
        msleep(120); 
        ctx->bl_enabled = true;
    } 
    else if (!want_on && ctx->bl_enabled) {
        huawei_nt51021_set_brightness(dsi, 0);
        msleep(20);
        gpiod_set_value_cansleep(ctx->bl_gpio, 0);
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

    ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(ctx->reset_gpio))
        return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "Failed to get reset-gpios\n");

    ctx->vcc_gpio = devm_gpiod_get(dev, "vcc", GPIOD_OUT_LOW);
    if (IS_ERR(ctx->vcc_gpio))
        return dev_err_probe(dev, PTR_ERR(ctx->vcc_gpio), "Failed to get vcc-gpios\n");

    ctx->blpwr_gpio = devm_gpiod_get(dev, "blpwr", GPIOD_OUT_LOW);
    if (IS_ERR(ctx->blpwr_gpio))
        return dev_err_probe(dev, PTR_ERR(ctx->blpwr_gpio), "Failed to get backlight-power-gpios\n");

    ctx->bl_gpio = devm_gpiod_get(dev, "backlight", GPIOD_OUT_LOW);
    if (IS_ERR(ctx->bl_gpio))
        return dev_err_probe(dev, PTR_ERR(ctx->bl_gpio), "Failed to get backlight-gpios\n");

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
