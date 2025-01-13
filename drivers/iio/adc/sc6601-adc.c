// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/iio/iio.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sysfs.h>
#include <linux/units.h>

#include <dt-bindings/iio/adc/southchip,sc6601_adc.h>

#define SC6601_REG_HK_ADC		0x11

struct sc6601_adc_data {
	struct device *dev;
	struct regmap *regmap;
	/*
	 * This mutex lock is for preventing the different ADC channels
	 * from being read at the same time.
	 */
	struct mutex adc_lock;
};

static int sc6601_adc_get_ibus_scale(struct sc6601_adc_data *priv)
{
	return 2500;
}

static int sc6601_adc_get_ibat_scale(struct sc6601_adc_data *priv)
{
	return 1220;
}

static int sc6601_adc_get_vbus_scale(struct sc6601_adc_data *priv)
{
	return 3750;
}

static int sc6601_adc_get_vac_scale(struct sc6601_adc_data *priv)
{
	return 5000;
}

static int sc6601_adc_get_vbat_scale(struct sc6601_adc_data *priv)
{
	return 1250;
}

static int sc6601_adc_get_vsys_scale(struct sc6601_adc_data *priv)
{
	return 1250;
}

static int sc6601_adc_read_raw(struct iio_dev *iio_dev,
			       const struct iio_chan_spec *chan,
			       int *val, int *val2, long mask)
{
	struct sc6601_adc_data *priv = iio_priv(iio_dev);
	__be16 be_val;
	int ret;
	unsigned int adc_data;

	mutex_lock(&priv->adc_lock);

	u32 reg = SC6601_REG_HK_ADC + chan->address * 2;

	ret = regmap_bulk_read(priv->regmap, reg, &be_val, sizeof(be_val));
	if (ret) {
		dev_err(priv->dev, "Failed to read adc data!\n");
		goto adc_unlock;
	}

	adc_data = be16_to_cpu(be_val);

	switch (chan->channel) {
	case SC6601_ADC_CHAN_VAC:
		adc_data *= sc6601_adc_get_vac_scale(priv);
	case SC6601_ADC_CHAN_VBAT:
		adc_data *= sc6601_adc_get_vbat_scale(priv);
	case SC6601_ADC_CHAN_VSYS:
		adc_data *= sc6601_adc_get_vsys_scale(priv);
	case SC6601_ADC_CHAN_VBUS:
		adc_data *= sc6601_adc_get_vbus_scale(priv);
	case SC6601_ADC_CHAN_IBAT:
		adc_data *= sc6601_adc_get_ibat_scale(priv);
	case SC6601_ADC_CHAN_IBUS:
		adc_data *= sc6601_adc_get_ibus_scale(priv);
	default:
		ret = -EINVAL;
		goto adc_unlock;
	}

	*val = adc_data;
	ret = IIO_VAL_INT;

adc_unlock:
	mutex_unlock(&priv->adc_lock);

	return ret;
}

static const char * const sc6601_channel_labels[SC6601_ADC_CHAN_MAX] = {
	[SC6601_ADC_CHAN_VAC] = "vac",
	[SC6601_ADC_CHAN_VBAT] = "vbat",
	[SC6601_ADC_CHAN_VSYS] = "vsys",
	[SC6601_ADC_CHAN_VBUS] = "vbus",
	[SC6601_ADC_CHAN_IBAT] = "ibat",
	[SC6601_ADC_CHAN_IBUS] = "ibus",
};

static int sc6601_adc_read_label(struct iio_dev *iio_dev,
				 struct iio_chan_spec const *chan, char *label)
{
	return sysfs_emit(label, "%s\n", sc6601_channel_labels[chan->channel]);
}

static const struct iio_info sc6601_adc_iio_info = {
	.read_raw = sc6601_adc_read_raw,
	.read_label = sc6601_adc_read_label,
};

#define SC6601_ADC_CHAN(_idx, _type, _addr, _extra_info) {	\
	.type = _type,						\
	.channel = SC6601_ADC_CHAN_##_idx,				\
	.address = _addr,					\
	.scan_index = SC6601_ADC_CHAN_##_idx,			\
	.indexed = 1,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			      BIT(IIO_CHAN_INFO_SCALE) |	\
			      _extra_info,			\
}

static const struct iio_chan_spec sc6601_adc_channels[] = {
	SC6601_ADC_CHAN(VAC, IIO_VOLTAGE, 2, 0),
	SC6601_ADC_CHAN(VBAT, IIO_VOLTAGE, 3, 0),
	SC6601_ADC_CHAN(VSYS, IIO_VOLTAGE, 6, 0),
	SC6601_ADC_CHAN(VBUS, IIO_VOLTAGE, 1, 0),
	SC6601_ADC_CHAN(IBAT, IIO_CURRENT, 5, 0),
	SC6601_ADC_CHAN(IBUS, IIO_CURRENT, 0, 0),
};

static int sc6601_adc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sc6601_adc_data *priv;
	struct iio_dev *indio_dev;
	struct regmap *regmap;
	int ret;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap)
		return dev_err_probe(dev, -ENODEV, "Failed to get regmap\n");

	indio_dev = devm_iio_device_alloc(dev, sizeof(*priv));
	if (!indio_dev)
		return -ENOMEM;

	priv = iio_priv(indio_dev);
	priv->dev = dev;
	priv->regmap = regmap;
	mutex_init(&priv->adc_lock);

	indio_dev->name = "sc6601-adc";
	indio_dev->info = &sc6601_adc_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = sc6601_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(sc6601_adc_channels);

	dev_err(dev, "sc6601 adc registered!\n");

	return devm_iio_device_register(dev, indio_dev);
}

static const struct of_device_id sc6601_adc_of_id[] = {
	{ .compatible = "southchip,sc6601-adc", },
	{}
};
MODULE_DEVICE_TABLE(of, sc6601_adc_of_id);

static struct platform_driver sc6601_adc_driver = {
	.driver = {
		.name = "sc6601-adc",
		.of_match_table = sc6601_adc_of_id,
	},
	.probe = sc6601_adc_probe,
};
module_platform_driver(sc6601_adc_driver);

MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_DESCRIPTION("SC6601 ADC Driver");
MODULE_LICENSE("GPL v2");
