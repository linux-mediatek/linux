// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for SouthChip SC6601 SubPMIC
 *
 * Copyright (C) 2025 Arseniy Velikanov
 */

#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "sc6601.h"

#define SC6601_TCPC_I2CADDR	0x62

#define SC6601_MAX_ADDRLEN	2

#define SC6601_REG_HK_DID	0x100
#define SC6601_REG_HK_IRQ	0x102
#define SC6601_REG_HK_IRQ_MASK	0x103

#define SC6601_DEVICE_ID	0x66
#define SC6601_1P1_DEVICE_ID	0x61

static const struct regmap_irq sc6601_irqs[] = {
	REGMAP_IRQ_REG_LINE(SC6601_IRQ_CHARGER, 8),
	REGMAP_IRQ_REG_LINE(SC6601_IRQ_DVCHG, 8),
	REGMAP_IRQ_REG_LINE(SC6601_IRQ_LED, 8),
	REGMAP_IRQ_REG_LINE(SC6601_IRQ_DPDM, 8),
	REGMAP_IRQ_REG_LINE(SC6601_IRQ_UFCS, 8),
	REGMAP_IRQ_REG_LINE(SC6601_IRQ_HK, 8),
	REGMAP_IRQ_REG_LINE(SC6601_IRQ_CID, 8),
};

static const struct regmap_irq_chip sc6601_irq_chip = {
	.name		= "sc6601-irqs",
	.status_base	= SC6601_REG_HK_IRQ,
	.mask_base	= SC6601_REG_HK_IRQ_MASK,
	.num_regs	= 1,
	.irqs		= sc6601_irqs,
	.num_irqs	= ARRAY_SIZE(sc6601_irqs),
};

static const struct resource sc6601_charger_irqs[] = {
	DEFINE_RES_IRQ_NAMED(SC6601_IRQ_CHARGER, "charger"),
	DEFINE_RES_IRQ_NAMED(SC6601_IRQ_DVCHG, "dvchg"),
	DEFINE_RES_IRQ_NAMED(SC6601_IRQ_DPDM, "dpdm"),
	DEFINE_RES_IRQ_NAMED(SC6601_IRQ_UFCS, "ufcs"),
	DEFINE_RES_IRQ_NAMED(SC6601_IRQ_HK, "hourse_keeping"),
};

static const struct mfd_cell sc6601_devices[] = {
	MFD_CELL_OF("sc6601-charger", sc6601_charger_irqs,
		    NULL, 0, 0, "southchip,sc6601-charger"),
	MFD_CELL_OF("sc6601-tcpc",
		    NULL, NULL, 0, 0, "southchip,sc6601-tcpc"),
};

static int sc6601_regmap_write(void *context, const void *data, size_t count)
{
	struct sc6601_info *info = context;
	const u8 *u8_buf = data;
	u8 bank_idx, bank_addr;
	int len = count - SC6601_MAX_ADDRLEN;

	bank_idx = u8_buf[0];
	bank_addr = u8_buf[1];

	return i2c_smbus_write_i2c_block_data(info->i2c[bank_idx], bank_addr,
					      len, data + SC6601_MAX_ADDRLEN);
}

static int sc6601_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
	int ret;
	struct sc6601_info *info = context;
	const u8 *u8_buf = reg_buf;
	u8 bank_idx, bank_addr;

	bank_idx = u8_buf[0];
	bank_addr = u8_buf[1];

	ret = i2c_smbus_read_i2c_block_data(info->i2c[bank_idx], bank_addr,
					    val_size, val_buf);
	if (ret < 0)
		return ret;

	return ret != val_size ? -EIO : 0;
}

static const struct regmap_bus sc6601_regmap_bus = {
	.write = sc6601_regmap_write,
	.read = sc6601_regmap_read,
};

static const struct regmap_config sc6601_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
};

static int sc6601_check_device_id(struct device *dev, struct regmap *rmap)
{
	int ret;
	unsigned int device_id;

	ret = regmap_read(rmap, SC6601_REG_HK_DID, &device_id);
	if (ret)
		return ret;

	switch (device_id) {
	case SC6601_DEVICE_ID:
	case SC6601_1P1_DEVICE_ID:
		return 0;
	default:
		dev_err(dev, "Unknown Device ID 0x%02x\n", device_id);
		return -ENODEV;
	}
}

static int sc6601_probe(struct i2c_client *i2c)
{
	struct sc6601_info *info;
	struct device *dev = &i2c->dev;
	struct i2c_client *tcpc_i2c;
	struct regmap *regmap;
	int ret;

	dev_err(dev, "probing!\n");

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	tcpc_i2c = devm_i2c_new_dummy_device(dev, i2c->adapter,
					     SC6601_TCPC_I2CADDR);
	if (IS_ERR(tcpc_i2c))
		return dev_err_probe(dev, PTR_ERR(tcpc_i2c),
				     "Failed to register TCPC I2C client\n");

	info->i2c[SC6601_CHARGER_I2C] = i2c;
	info->i2c[SC6601_TCPC_I2C] = tcpc_i2c;

	regmap = devm_regmap_init(dev, &sc6601_regmap_bus,
				  info, &sc6601_regmap_config);
	if (IS_ERR(regmap))
		return dev_err_probe(dev, PTR_ERR(regmap),
				     "Failed to init regmap\n");

	ret = sc6601_check_device_id(dev, regmap);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to check chip device id\n");

	ret = devm_regmap_add_irq_chip(dev, regmap, i2c->irq,
				       IRQF_ONESHOT, -1, &sc6601_irq_chip,
				       &info->irq_data);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add irq chip\n");

	dev_err(dev, "add mfd devices!\n");

	return devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO,
				    sc6601_devices, ARRAY_SIZE(sc6601_devices),
				    NULL, 0,
				    regmap_irq_get_domain(info->irq_data));
}

static const struct of_device_id sc6601_match_table[] = {
	{ .compatible = "southchip,sc6601" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sc6601_match_table);

static struct i2c_driver sc6601_driver = {
	.driver = {
		.name = "sc6601",
		.of_match_table = sc6601_match_table,
	},
	.probe = sc6601_probe,
};
module_i2c_driver(sc6601_driver);

MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_DESCRIPTION("SouthChip SC6601 SubPMIC Driver");
MODULE_LICENSE("GPL v2");
