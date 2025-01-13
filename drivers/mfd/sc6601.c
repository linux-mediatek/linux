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

static const struct mfd_cell sc6601_devices[] = {
	MFD_CELL_OF("sc6601-adc",
		    NULL, NULL, 0, 0, "southchip,sc6601-adc"),
	MFD_CELL_OF("sc6601-charger",
		    NULL, NULL, 0, 0, "southchip,sc6601-charger"),
};

static int sc6601_regmap_write(void *context, const void *data, size_t count)
{
	struct sc6601_info *info = context;
	const u8 *_data = data;

	return i2c_smbus_write_i2c_block_data(info->i2c, _data[1], count - 2, _data + 2);
}

static int sc6601_regmap_read(void *context, const void *reg_buf,
			      size_t reg_size, void *val_buf, size_t val_size)
{
	int ret;
	struct sc6601_info *info = context;
	const u8 *_reg_buf = reg_buf;

	ret = i2c_smbus_read_i2c_block_data(info->i2c, _reg_buf[1], val_size, val_buf);
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

static void sc6601_irq_lock(struct irq_data *data)
{
	struct sc6601_info *info = irq_data_get_irq_chip_data(data);
	mutex_lock(&info->irq_lock);
}

static void sc6601_irq_sync_unlock(struct irq_data *data)
{
	struct sc6601_info *info = irq_data_get_irq_chip_data(data);
	regmap_bulk_write(info->regmap, SC6601_REG_HK_IRQ_MASK, &info->irq_mask, 1);
	mutex_unlock(&info->irq_lock);
}

static void sc6601_irq_enable(struct irq_data *data)
{
	struct sc6601_info *info = irq_data_get_irq_chip_data(data);

	info->irq_mask &= ~BIT(data->hwirq);
}

static void sc6601_irq_disable(struct irq_data *data)
{
	struct sc6601_info *info = irq_data_get_irq_chip_data(data);

	info->irq_mask |= BIT(data->hwirq);
}

static int sc6601_irq_map(struct irq_domain *h, unsigned int virq,
						   irq_hw_number_t hwirq)
{
	struct sc6601_info *info = h->host_data;
	irq_set_chip_data(virq, info);
	irq_set_chip(virq, &info->irq_chip);
	irq_set_nested_thread(virq, 1);
	irq_set_parent(virq, info->i2c->irq);
	irq_set_noprobe(virq);
	return 0;
}

static const struct irq_domain_ops sc6601_domain_ops = {
	.map = sc6601_irq_map,
	.xlate = irq_domain_xlate_onetwocell,
};

static irqreturn_t sc6601_irq_thread(int irq, void *data)
{
	struct sc6601_info *info = data;
	u8 evt = 0;
	bool handle = false;
	int i, ret;

	pm_stay_awake(&info->i2c->dev);

	ret = regmap_bulk_read(info->regmap, SC6601_REG_HK_IRQ, &evt, 1);
	if (ret) {
		dev_err(&info->i2c->dev, "failed to read irq event\n");
		return IRQ_HANDLED;
	}

	evt |= BIT(SC6601_IRQ_HK);

	evt &= ~(info->irq_mask);

	for (i = 0; i < SC6601_IRQ_MAX; i++) {
		if(evt & BIT(i)) {
			handle_nested_irq(irq_find_mapping(info->irq_domain, i));
			handle = true;
		}
	}

	pm_relax(&info->i2c->dev);
	return handle ? IRQ_HANDLED : IRQ_NONE;
}

static int sc6601_add_irq_chip(struct sc6601_info *info)
{
	int ret = 0;
	int val;

	ret = regmap_bulk_read(info->regmap, SC6601_REG_HK_IRQ, &val, 1);
	if (ret < 0)
		return ret;

	info->irq_mask = 0xff;

	ret = regmap_bulk_write(info->regmap, SC6601_REG_HK_IRQ_MASK, &info->irq_mask, 1);

	info->irq_chip.name = dev_name(&info->i2c->dev);
	info->irq_chip.irq_disable = sc6601_irq_disable;
	info->irq_chip.irq_enable = sc6601_irq_enable;
	info->irq_chip.irq_bus_lock = sc6601_irq_lock;
	info->irq_chip.irq_bus_sync_unlock = sc6601_irq_sync_unlock;

	info->irq_domain = irq_domain_add_linear(info->i2c->dev.of_node,
						SC6601_IRQ_MAX, &sc6601_domain_ops, info);
	if (!info->irq_domain) {
		dev_err(&info->i2c->dev, "failed to create irq domain\n");
		return -ENOMEM;
	}

	ret = devm_request_threaded_irq(&info->i2c->dev, info->i2c->irq,
							NULL, sc6601_irq_thread,
							IRQF_TRIGGER_RISING | IRQF_ONESHOT, dev_name(&info->i2c->dev), info);
	if (ret) {
		dev_err(&info->i2c->dev, "failed to request irq %d for %s\n", info->i2c->irq, dev_name(&info->i2c->dev));
		irq_domain_remove(info->irq_domain);
		return ret;
	}

	return 0;
}

static int sc6601_check_did(struct sc6601_info *info)
{
	int ret;
	u8 did = 0;

	ret = regmap_bulk_read(info->regmap, SC6601_REG_HK_DID, &did, 1);
	if (ret)
		return ret;

	if (did == SC6601_DEVICE_ID) {
		dev_info(&info->i2c->dev, "SC6601 detected\n");
	} else if (did == SC6601_1P1_DEVICE_ID) {
		dev_info(&info->i2c->dev, "SC6601 1P1 detected\n");
	} else {
		return -ENODEV;
	}

	return 0;
}

static int sc6601_probe(struct i2c_client *i2c)
{
	struct sc6601_info *info;
	struct device *dev = &i2c->dev;
	int ret;

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->i2c = i2c;

	info->regmap = devm_regmap_init(dev, &sc6601_regmap_bus,
				  info, &sc6601_regmap_config);
	if (IS_ERR(info->regmap))
		return dev_err_probe(dev, PTR_ERR(info->regmap),
				     "Failed to init regmap\n");

	ret = sc6601_check_did(info);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to check chip did info\n");

	ret = sc6601_add_irq_chip(info);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to add irq chip\n");

	return devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO,
				    sc6601_devices, ARRAY_SIZE(sc6601_devices),
				    NULL, 0,
					info->irq_domain);
}

static const struct of_device_id sc6601_match_table[] = {
	{ .compatible = "southchip,sc6601" },
	{}
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
