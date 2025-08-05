/*
 * BOE TV110XUM-LB2 panel driver.
 *
 * This panel was found in FPlus T1100, and it is connected via
 * ITE IT6112 DSI-DUAL_DSI bridge.
 *
 * This i2c driver controls the MIPI TX and MIPI RX of bridge,
 * presenting a DSI device with a drm_panel.
 *
 * Copyright (c) 2025 Arseniy Velikanov <me@adomerle.pw>
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#define IT6112_MIPI_RX_I2CADDR		(0x5e)

#define IT6112_CHIPID			(0x02)
#define IT6112_REVID			(0x03)

#define IT6112_SW_RST			(1 << 1)
#define IT6112_HOLD_TX			(1 << 5)
#define IT6112_TX_INVERSE_MCLK		(1 << 1)
#define IT6112_MIPI_TX_LANE_MODE	(1 << 3)
#define IT6112_MIPI_TX_4LANE_MODE	(1 << 3)
#define IT6112_MIPI_TX_8LANE_MODE	(0)
#define IT6112_MIPI_TX_V_LPS_TIME	(0x0f)
#define IT6112_MIPI_TX_LPX_NUM		(0x0f)
#define IT6112_MIPI_TX_HS_PREPARE	(0xf0)
#define IT6112_MIPI_TX_PRE_1T_EN	(1 << 2)

#define IT6112_MIPI_RX_INVERSE_MCLK	(1 << 1)

struct it6112_ctx {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct i2c_client *i2c_mipi_tx;
	struct i2c_client *i2c_mipi_rx;
	struct regulator_bulk_data regulators[3];
	struct gpio_desc *reset_gpio;
};

static struct it6112_ctx *panel_to_it6112(struct drm_panel *panel)
{
	return container_of(panel, struct it6112_ctx, base);
}

static int it6112_mipi_tx_read(struct it6112_ctx *ctx, u8 reg)
{
	return i2c_smbus_read_byte_data(ctx->i2c_mipi_tx, reg);
}

static void it6112_mipi_tx_write(struct it6112_ctx *ctx, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(ctx->i2c_mipi_tx, reg, val);
	if (reg)
		dev_err(&ctx->i2c_mipi_tx->dev, "I2C write failed: %d\n", ret);
}

static void it6112_mipi_tx_set_bits(struct it6112_ctx *ctx, u8 reg,
				   u8 mask, u8 data)
{
	int temp;

	temp = it6112_mipi_tx_read(ctx, reg);
	temp = (temp & ((~mask) & 0xff)) + (mask & data);
	it6112_mipi_tx_write(ctx, reg, temp);
}

static int it6112_mipi_rx_read(struct it6112_ctx *ctx, u8 reg)
{
	return i2c_smbus_read_byte_data(ctx->i2c_mipi_rx, reg);
}

static void it6112_mipi_rx_write(struct it6112_ctx *ctx, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(ctx->i2c_mipi_rx, reg, val);
	if (reg)
		dev_err(&ctx->i2c_mipi_tx->dev, "I2C write failed: %d\n", ret);
}

static void it6112_mipi_rx_set_bits(struct it6112_ctx *ctx, u8 reg,
				   u8 mask, u8 data)
{
	int temp;

	temp = it6112_mipi_rx_read(ctx, reg);
	temp = (temp & ((~mask) & 0xff)) + (mask & data);
	it6112_mipi_rx_write(ctx, reg, temp);
}

static int it6112_get_pid(struct it6112_ctx *ctx)
{
	int revid, chipid;

	revid = it6112_mipi_tx_read(ctx, IT6112_REVID);
	if (revid < 0)
		return revid;

	chipid = it6112_mipi_tx_read(ctx, IT6112_CHIPID);
	if (chipid < 0)
		return chipid;

	return (revid << 8) | chipid;
}

static void it6112_mipi_power_on(struct it6112_ctx *ctx)
{
	/* Video Clock Domain Reset */
	it6112_mipi_tx_set_bits(ctx, 0x06, 0x02, 0x00);
	mdelay(2);
	it6112_mipi_tx_set_bits(ctx, 0x05, 0x0f, 0x00);
	/* tx mclk still hold */
	it6112_mipi_tx_set_bits(ctx, 0x05, 0xcc, 0x00);
	mdelay(2);
}

static void it6112_tv110xum_lb2_init_tx(struct it6112_ctx *ctx)
{
	// Reversed LK init config
	it6112_mipi_tx_set_bits(ctx, 0x5, IT6112_SW_RST, IT6112_SW_RST);
	it6112_mipi_tx_set_bits(ctx, 0x5, IT6112_HOLD_TX, IT6112_HOLD_TX);
	it6112_mipi_tx_set_bits(ctx, 0x10, IT6112_TX_INVERSE_MCLK, 0);
	it6112_mipi_tx_set_bits(ctx, 0x11, IT6112_MIPI_TX_LANE_MODE,
			       IT6112_MIPI_TX_8LANE_MODE);
	it6112_mipi_tx_set_bits(ctx, 0x24, IT6112_MIPI_TX_V_LPS_TIME, 1);
	it6112_mipi_tx_set_bits(ctx, 0x3c, 0x20, 0x20);
	it6112_mipi_tx_set_bits(ctx, 0x44, IT6112_MIPI_TX_PRE_1T_EN,
			       IT6112_MIPI_TX_PRE_1T_EN);
	it6112_mipi_tx_set_bits(ctx, 0x45, IT6112_MIPI_TX_LPX_NUM, 3);
	it6112_mipi_tx_set_bits(ctx, 0x47, IT6112_MIPI_TX_HS_PREPARE, 16);
	it6112_mipi_tx_set_bits(ctx, 0xb0, 0xff, 0x27);
}

static void it6112_tv110xum_lb2_init_rx(struct it6112_ctx *ctx)
{
	// Reversed LK init config

	//Enable MPRX interrupt
	it6112_mipi_rx_write(ctx, 0x09, 0xff);
	it6112_mipi_rx_write(ctx, 0x0a, 0xff);
	it6112_mipi_rx_write(ctx, 0x0b, 0x3f);
	it6112_mipi_rx_write(ctx, 0x05, 0x03);
	it6112_mipi_rx_write(ctx, 0x05, 0x00);

	it6112_mipi_rx_set_bits(ctx, 0x0c, 0x0f, 3);
	it6112_mipi_rx_set_bits(ctx, 0x11, IT6112_MIPI_RX_INVERSE_MCLK,
			       IT6112_MIPI_RX_INVERSE_MCLK);
	it6112_mipi_rx_set_bits(ctx, 0x18, 0xff, 67);
	it6112_mipi_rx_set_bits(ctx, 0x19, 0xf3, 3);
	it6112_mipi_rx_set_bits(ctx, 0x20, 0xf7, 0x03);
	
	/* rx auto detect video format */
	it6112_mipi_rx_set_bits(ctx, 0x21, 0x08, 0x08);
	it6112_mipi_rx_set_bits(ctx, 0x44, 0x22, 0x22);

	it6112_mipi_rx_write(ctx, 0x27, 0x3e);
	it6112_mipi_rx_write(ctx, 0x72, 0x07);
	it6112_mipi_rx_set_bits(ctx, 0x8a, 0x07, 0x02);
	it6112_mipi_rx_set_bits(ctx, 0xa0, 0x01, 0x01);

	it6112_mipi_rx_set_bits(ctx, 0x80, 0x3f, 0x07);
}

static void it6112_mipi_rx_calc_clk(struct it6112_ctx *ctx)
{
	int mclk, rclk, sum = 0;

	it6112_mipi_rx_set_bits(ctx, 0x94, 0x80, 0x80);
	mdelay(100);
	it6112_mipi_rx_set_bits(ctx, 0x94, 0x80, 0x0);

	rclk = it6112_mipi_rx_read(ctx, 0x95) +
	(it6112_mipi_rx_read(ctx, 0x96) << 8) +
	(it6112_mipi_rx_read(ctx, 0x97) << 16);
	rclk /= 100;

	dev_info(&ctx->i2c_mipi_rx->dev, "RCLK=%dMHz\n", rclk / 1000);

	for (int i = 0; i < 5; i++)
	{
		it6112_mipi_rx_set_bits(ctx, 0x9b, 0x80, 0x80);
		mdelay(1);
		it6112_mipi_rx_set_bits(ctx, 0x9b, 0x80, 0x00);

		sum += it6112_mipi_rx_read(ctx, 0x9a);
		sum += ((it6112_mipi_rx_read(ctx, 0x9b) & 0x0f) << 8);
	}

	sum /= 5;
	mclk = rclk * 2048 / sum;

	dev_info(&ctx->i2c_mipi_tx->dev, "MCLK=%dMHz\n", mclk / 1000);
}

static void it6112_bridge_init(struct it6112_ctx *ctx)
{
	it6112_mipi_power_on(ctx);
	it6112_tv110xum_lb2_init_rx(ctx);
	it6112_tv110xum_lb2_init_tx(ctx);
	it6112_mipi_rx_calc_clk(ctx);
}

static int boe_tv110xum_lb2_enable(struct drm_panel *panel)
{
	struct it6112_ctx *ctx = panel_to_it6112(panel);

	it6112_mipi_tx_set_bits(ctx, 0x11, 0x80, 0x80);
	msleep(10);

	// write dcs exit sleep mode
	it6112_mipi_tx_write(ctx, 0x73, 0x05);
	it6112_mipi_tx_write(ctx, 0x73, MIPI_DCS_EXIT_SLEEP_MODE);
	it6112_mipi_tx_write(ctx, 0x73, 0x00);
	it6112_mipi_tx_write(ctx, 0x73, 0x36);
	it6112_mipi_tx_write(ctx, 0x74, 0x44);
	it6112_mipi_tx_write(ctx, 0x75, 0x87);
	mdelay(120);

	// write dcs set display on
	it6112_mipi_tx_write(ctx, 0x73, 0x05);
	it6112_mipi_tx_write(ctx, 0x73, MIPI_DCS_SET_DISPLAY_ON);
	it6112_mipi_tx_write(ctx, 0x73, 0x00);
	it6112_mipi_tx_write(ctx, 0x73, 0x1c);
	it6112_mipi_tx_write(ctx, 0x74, 0x44);
	it6112_mipi_tx_write(ctx, 0x75, 0x87);
	mdelay(20);

	it6112_mipi_tx_set_bits(ctx, 0x11, 0x80, 0x00);
	mdelay(2);
	it6112_mipi_tx_write(ctx, 0x05, 0xfe);
	it6112_mipi_tx_write(ctx, 0x05, 0x00);

	return 0;
}

static int boe_tv110xum_lb2_disable(struct drm_panel *panel)
{
	struct it6112_ctx *ctx = panel_to_it6112(panel);

	// write dcs enter sleep mode
	it6112_mipi_tx_write(ctx, 0x73, 0x05);
	it6112_mipi_tx_write(ctx, 0x73, MIPI_DCS_ENTER_SLEEP_MODE);
	it6112_mipi_tx_write(ctx, 0x73, 0x00);
	it6112_mipi_tx_write(ctx, 0x73, 0x2c);
	it6112_mipi_tx_write(ctx, 0x74, 0x44);
	it6112_mipi_tx_write(ctx, 0x75, 0x87);
	mdelay(120);

	// write dcs set display off
	it6112_mipi_tx_write(ctx, 0x73, 0x05);
	it6112_mipi_tx_write(ctx, 0x73, MIPI_DCS_SET_DISPLAY_OFF);
	it6112_mipi_tx_write(ctx, 0x73, 0x00);
	it6112_mipi_tx_write(ctx, 0x73, 0x06);
	it6112_mipi_tx_write(ctx, 0x74, 0x44);
	it6112_mipi_tx_write(ctx, 0x75, 0x87);
	mdelay(20);

	return 0;
}

static int boe_tv110xum_lb2_prepare(struct drm_panel *panel)
{
	struct it6112_ctx *ctx = panel_to_it6112(panel);
	int ret;
	
	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->regulators),
				   ctx->regulators);
	if (ret) {
		dev_err(&ctx->dsi->dev,
		       "failed to enable regulators: %d\n", ret);
		return ret;
	}

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(12000, 13000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(12000, 13000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(12000, 13000);

	it6112_bridge_init(ctx);

	return 0;
}

static int boe_tv110xum_lb2_unprepare(struct drm_panel *panel)
{
	struct it6112_ctx *ctx = panel_to_it6112(panel);

	boe_tv110xum_lb2_disable(panel);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);

	regulator_bulk_disable(ARRAY_SIZE(ctx->regulators),
			      ctx->regulators);

	return 0;
}

static const struct drm_display_mode boe_tv110xum_lb2_mode = {
	.clock = (1600 + 109 + 34 + 10) * (2176 + 220 + 28 + 8) * 60 / 1000,
	.hdisplay = 1600,
	.hsync_start = 1600 + 109,
	.hsync_end = 1600 + 109 + 34,
	.htotal = 1600 + 109 + 34 + 10,
	.vdisplay = 2176,
	.vsync_start = 2176 + 220,
	.vsync_end = 2176 + 220 + 28,
	.vtotal = 2176 + 220 + 28 + 8,
	.width_mm = 166,
	.height_mm = 226,
};

static int boe_tv110xum_lb2_get_modes(struct drm_panel *panel,
				     struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &boe_tv110xum_lb2_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs boe_tv110xum_lb2_funcs = {
	.disable = boe_tv110xum_lb2_disable,
	.enable = boe_tv110xum_lb2_enable,
	.prepare = boe_tv110xum_lb2_prepare,
	.unprepare = boe_tv110xum_lb2_unprepare,
	.get_modes = boe_tv110xum_lb2_get_modes
};

static int it6112_bridge_probe(struct i2c_client *i2c)
{
	struct device *dev = &i2c->dev;
	struct i2c_client *i2c_mipi_rx;
	struct it6112_ctx *ctx;
	struct device_node *endpoint, *dsi_host_node;
	struct mipi_dsi_host *host;
	int ret;
	int pid;

	struct mipi_dsi_device_info info = {
		.type = "boe-tv110xum-lb2",
		.channel = 0,
		.node = NULL,
	};

	ctx = devm_drm_panel_alloc(dev, __typeof(*ctx), base,
				  &boe_tv110xum_lb2_funcs,
				  DRM_MODE_CONNECTOR_DSI);

	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	i2c_mipi_rx = devm_i2c_new_dummy_device(dev, i2c->adapter,
				IT6112_MIPI_RX_I2CADDR);
	
	if (IS_ERR(i2c_mipi_rx))
		return dev_err_probe(dev, PTR_ERR(i2c_mipi_rx),
			"Failed to register MIPI RX I2C client\n");

	ctx->i2c_mipi_tx = i2c;
	ctx->i2c_mipi_rx = i2c_mipi_rx;
	i2c_set_clientdata(i2c, ctx);

	ctx->regulators[0].supply = "dvdd";
	ctx->regulators[1].supply = "vpos";
	ctx->regulators[2].supply = "vneg";
	ret = devm_regulator_bulk_get(dev,
				     ARRAY_SIZE(ctx->regulators),
				     ctx->regulators);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get regulators\n");

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "failed to get reset gpio\n");

	pid = it6112_get_pid(ctx);
	if (pid < 0)
		return dev_err_probe(dev, -ENODEV, "read product id failed: %d\n", pid);
	if (pid != 0x6112)
		return dev_err_probe(dev, -ENODEV, "unknown product id: 0x%x\n", pid);

	endpoint = of_graph_get_endpoint_by_regs(dev->of_node, 0, -1);
	if (!endpoint)
		return dev_err_probe(dev, -ENODEV, "failed to get dsi endpoint\n");

	dsi_host_node = of_graph_get_remote_port_parent(endpoint);
	if (!dsi_host_node)
		goto error;

	host = of_find_mipi_dsi_host_by_node(dsi_host_node);
	of_node_put(dsi_host_node);
	if (!host) {
		of_node_put(endpoint);
		return -EPROBE_DEFER;
	}

	info.node = of_graph_get_remote_port(endpoint);
	if (!info.node)
		goto error;

	of_node_put(endpoint);

	ctx->dsi = mipi_dsi_device_register_full(host, &info);
	if (IS_ERR(ctx->dsi)) {
		dev_err(dev, "DSI device registration failed: %ld\n",
		       PTR_ERR(ctx->dsi));
		return PTR_ERR(ctx->dsi);
	}

	ret = drm_panel_of_backlight(&ctx->base);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->base);

	dev_err(dev, "We are registered dsi device!!\n");

	return 0;

error:
	of_node_put(endpoint);
	return -ENODEV;
}

static void it6112_bridge_remove(struct i2c_client *i2c)
{
	struct it6112_ctx *ctx = i2c_get_clientdata(i2c);

	mipi_dsi_detach(ctx->dsi);
	drm_panel_remove(&ctx->base);
	mipi_dsi_device_unregister(ctx->dsi);
}

static int boe_tv110xum_lb2_dsi_probe(struct mipi_dsi_device *dsi)
{
	int ret;

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;

	ret = mipi_dsi_attach(dsi);

	if (ret)
		dev_err(&dsi->dev, "failed to attach dsi to host: %d\n", ret);

	return ret;
}

static struct mipi_dsi_driver boe_tv110xum_lb2_dsi_driver = {
	.driver.name = "panel-boe-tv110xum-lb2-it6112",
	.probe = boe_tv110xum_lb2_dsi_probe,
};

static const struct of_device_id it6112_bridge_of_ids[] = {
	{ .compatible = "boe,tv110xum-lb2-it6112" },
	{ } /* sentinel */
};
MODULE_DEVICE_TABLE(of, it6112_bridge_of_ids);

static struct i2c_driver it6112_bridge_driver = {
	.driver = {
		.name = "it6112_bridge",
		.of_match_table = it6112_bridge_of_ids,
	},
	.probe = it6112_bridge_probe,
	.remove = it6112_bridge_remove,
};

static int __init it6112_init(void)
{
	mipi_dsi_driver_register(&boe_tv110xum_lb2_dsi_driver);
	return i2c_add_driver(&it6112_bridge_driver);
}
module_init(it6112_init);

static void __exit it6112_exit(void)
{
	i2c_del_driver(&it6112_bridge_driver);
	mipi_dsi_driver_unregister(&boe_tv110xum_lb2_dsi_driver);
}
module_exit(it6112_exit);

MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_DESCRIPTION("BOE TV110XUM-LB2 panel driver");
MODULE_LICENSE("GPL v2");
