// SPDX-License-Identifier: GPL-2.0-only
/*
 * This is really simple DSI driver for xiaomi emerald panel
 * for testing dsi functionality
 * drm says "cannot find any crtc or sizes"
 * probably mtk issue
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

static const char * const emerald_regulator_names[] = {
	"vddi",
	"vdd",
	"vci",
};

struct panel_desc {
	const struct drm_display_mode *display_mode;
	unsigned int width_mm;
	unsigned int height_mm;
	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	unsigned int lanes;
};

struct emerald_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct panel_desc *desc;
	struct gpio_desc *reset_gpio;

	struct regulator_bulk_data supplies[ARRAY_SIZE(emerald_regulator_names)];
};

static inline struct emerald_panel *to_emerald_panel(struct drm_panel *panel)
{
	return container_of(panel, struct emerald_panel, panel);
}

static int emerald_panel_prepare(struct drm_panel *panel)
{
	struct emerald_panel *ctx = to_emerald_panel(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	dev_info(panel->dev, "panel prepare\n");

	return 0;
}

static int emerald_panel_unprepare(struct drm_panel *panel)
{
	return 0;
}

/* FIXME: These clocks are not adjusted for DSC */
static const struct drm_display_mode xiaomi_emerald_mode_120Hz = {
	.clock = (1080 + 116 + 8 + 16) * (2400 + 20 + 4 + 8) * 120 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 116,
	.hsync_end = 1080 + 116 + 8,
	.htotal = 1080 + 116 + 8 + 16,
	.vdisplay = 2400,
	.vsync_start = 2400 + 20,
	.vsync_end = 2400 + 20 + 4,
	.vtotal = 2400 + 20 + 4 + 8,
};

static const struct panel_desc emerald_panel_desc = {
	.display_mode = &xiaomi_emerald_mode_120Hz,
	.width_mm = 70,
	.height_mm = 155,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
				  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
				  MIPI_DSI_CLOCK_NON_CONTINUOUS,
	.format = MIPI_DSI_FMT_RGB888,
	.lanes = 4,
};

static int emerald_panel_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct emerald_panel *ctx = to_emerald_panel(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, ctx->desc->display_mode);

	if (!mode) {
		dev_err(panel->dev, "failed to add mode %ux%u@%u\n",
			ctx->desc->display_mode->hdisplay, ctx->desc->display_mode->vdisplay, drm_mode_vrefresh(ctx->desc->display_mode));
		return -ENOMEM;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	dev_info(panel->dev, "return mode %ux%u@%u\n",
		mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode));

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = ctx->desc->width_mm;
	connector->display_info.height_mm = ctx->desc->height_mm;
	connector->display_info.bpc = 8;

	return 1;
}

static const struct drm_panel_funcs emerald_drm_funcs = {
	.prepare = emerald_panel_prepare,
	.unprepare = emerald_panel_unprepare,
	.get_modes = emerald_panel_get_modes,
};

static int emerald_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct emerald_panel *ctx;
	int i, ret = 0;

	dev_err(dev, "probe\n");

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->desc = of_device_get_match_data(dev);
	if (!ctx->desc) {
		dev_err(dev, "missing device configuration\n");
		return -ENODEV;
	}

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = ctx->desc->lanes;
	dsi->format = ctx->desc->format;
	dsi->mode_flags = ctx->desc->mode_flags;

	drm_panel_init(&ctx->panel, dev, &emerald_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	ctx->panel.prepare_prev_first = true;

	drm_panel_add(&ctx->panel);

	dev_err(dev, "dsi attach\n");
	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		goto err_dsi_attach;
	}

	return 0;

err_dsi_attach:
	drm_panel_remove(&ctx->panel);
	return ret;
}

static void emerald_panel_remove(struct mipi_dsi_device *dsi)
{
	struct emerald_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(ctx->dsi);
	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id emerald_of_match[] = {
	{
		.compatible = "xiaomi,emerald-csot",
		.data = &emerald_panel_desc,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, emerald_of_match);

static struct mipi_dsi_driver emerald_panel_driver = {
	.driver = {
		.name = "panel-xiaomi-emerald",
		.of_match_table = emerald_of_match,
	},
	.probe = emerald_panel_probe,
	.remove = emerald_panel_remove,
};
module_mipi_dsi_driver(emerald_panel_driver);

MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_DESCRIPTION("Xiaomi Emerald DSI Panel Driver");
MODULE_LICENSE("GPL");
