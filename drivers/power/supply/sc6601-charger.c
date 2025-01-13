// SPDX-License-Identifier: GPL-2.0-only

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/devm-helpers.h>
#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/linear_range.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/workqueue.h>

enum sc6601_chg_reg_field {
	F_VAC_OVP,
	F_VBUS_OVP,
	F_ACDRV_MANUAL_PRE,
	F_ACDRV_EN,
	F_ACDRV_MANUAL_EN,
	F_WD_TIME_RST,
	F_WD_TIMER,
	F_REG_RST,
	F_VBUS_PD,
	F_VAC_PD,
	F_CID_EN,
	F_ADC_EN,
	F_ADC_FREEZE,

	F_ICO_EN,

	F_VBOOST,
	F_IBOOST,

	F_CHG_EN,
	F_BOOST_EN,
	F_QB_EN,
	F_ICHG_CC,
	F_VRECHG,
	F_IBAT_OCP_DIS,
	F_BATFET_RST_EN,
	F_VSYS_MIN,
	F_VBAT,
	F_BATSNS_EN,
	F_AUTO_INDET_EN,
	F_HVDCP_EN,
	F_CONV_OCP_DIS,
	F_RECHG_DG,
	F_VPMID_OVP_OTG_DIS,
	F_VBAT_OVP_BUCK_DIS,
	F_IBATOCP,
	F_JEITA_COOL_TEMP,
	F_JEITA_WARM_TEMP,
	F_MAX
};

enum sc6601_irq {
	SC6601_IRQ_CHARGER,
	SC6601_IRQ_HK,
	SC6601_IRQ_MAX
};

struct sc6601_priv {
	struct device *dev;
	struct iio_channel *iio_adcs;
	struct mutex attach_lock;
	struct power_supply *psy;
	struct regmap *regmap;
	struct regmap_field *rmap_fields[F_MAX];
	struct regulator_dev *rdev;
	struct workqueue_struct *wq;
	struct work_struct bc12_work;
	unsigned int irq[SC6601_IRQ_MAX];

	bool online;
};

/*
enum mt6370_usb_status {
	MT6370_USB_STAT_NO_VBUS = 0,
	MT6370_USB_STAT_VBUS_FLOW_IS_UNDER_GOING,
	MT6370_USB_STAT_SDP,
	MT6370_USB_STAT_SDP_NSTD,
	MT6370_USB_STAT_DCP,
	MT6370_USB_STAT_CDP,
	MT6370_USB_STAT_MAX
};*/

struct sc6601_chg_field {
	const char *name;
	const struct linear_range *range;
	struct reg_field field;
};

/*
enum {
	MT6370_RANGE_F_IAICR = 0,
	MT6370_RANGE_F_VOREG,
	MT6370_RANGE_F_VMIVR,
	MT6370_RANGE_F_ICHG,
	MT6370_RANGE_F_IPREC,
	MT6370_RANGE_F_IEOC,
	MT6370_RANGE_F_MAX
};

static const struct linear_range mt6370_chg_ranges[MT6370_RANGE_F_MAX] = {
	LINEAR_RANGE_IDX(MT6370_RANGE_F_IAICR, 100000, 0x0, 0x3F, 50000),
	LINEAR_RANGE_IDX(MT6370_RANGE_F_VOREG, 3900000, 0x0, 0x51, 10000),
	LINEAR_RANGE_IDX(MT6370_RANGE_F_VMIVR, 3900000, 0x0, 0x5F, 100000),
	LINEAR_RANGE_IDX(MT6370_RANGE_F_ICHG, 900000, 0x08, 0x31, 100000),
	LINEAR_RANGE_IDX(MT6370_RANGE_F_IPREC, 100000, 0x0, 0x0F, 50000),
	LINEAR_RANGE_IDX(MT6370_RANGE_F_IEOC, 100000, 0x0, 0x0F, 50000),
};*/

#define SC6601_CHG_FIELD(_fd, _reg, _lsb, _msb)				\
[_fd] = {								\
	.name = #_fd,							\
	.range = NULL,							\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
}

/*
#define SC6601_CHG_FIELD_RANGE(_fd, _reg, _lsb, _msb)			\
[_fd] = {								\
	.name = #_fd,							\
	.range = &mt6370_chg_ranges[MT6370_RANGE_##_fd],		\
	.field = REG_FIELD(_reg, _lsb, _msb),				\
}
*/


#define SC6601_REG_VAC_VBUS_OVP		0x04
#define SC6601_REG_HK_CTRL			0x07
#define SC6601_REG_HK_CTRL1			0x08
#define SC6601_REG_HK_INT_STAT		0x09
#define SC6601_REG_HK_INT_FLG		0x0a
#define SC6601_REG_HK_INT_MASK		0x0b
#define SC6601_REG_HK_FLT_STAT		0x0c
#define SC6601_REG_HK_FLT_FLG		0x0d
#define SC6601_REG_HK_FLT_MASK		0x0e
#define	SC6601_REG_HK_ADC_CTRL		0x0f
#define SC6601_REG_HK_ADC			0x11
#define SC6601_REG_VSYS_MIN			0x30
#define SC6601_REG_VBAT				0x31
#define SC6601_REG_ICHG_CC			0x32
#define SC6601_REG_ICO_CTRL			0x35
#define SC6601_REG_RECHARGE_CTRL	0x38
#define SC6601_REG_VBOOST_CTRL		0x39
#define SC6601_REG_PROTECTION_DIS	0x3a
#define SC6601_REG_RESET_CTRL		0x3b
#define SC6601_REG_CHG_CTRL			0x3c
#define SC6601_REG_CHG_CTRL1		0x3d
#define SC6601_REG_CHG_CTRL4		0x40
#define SC6601_REG_CHG_INT_STAT		0x41
#define SC6601_REG_CHG_INT_STAT1	0x42
#define SC6601_REG_CHG_INT_FLG		0x44
#define SC6601_REG_CHG_INT_MASK		0x47
#define SC6601_REG_CHG_FLT_STAT		0x50
#define SC6601_REG_CHG_FLT_FLG		0x52
#define SC6601_REG_CHG_FLT_MASK		0x54
#define SC6601_REG_JEITA_TEMP		0x56
#define SC6601_REG_DPDM_EN			0x90
#define SC6601_REG_DPDM_CTRL		0x91
#define SC6601_REG_DPDM_QC_CTRL		0x92
#define SC6601_REG_DPDM_TFCP_CTRL	0x93
#define SC6601_REG_DPDM_INT_FLAG	0x94
#define SC6601_REG_DPDM_INT_MASK	0x95
#define SC6601_REG_QC3_INT_FLAG		0x96
#define SC6601_REG_QC3_INT_MASK		0x97
#define SC6601_REG_DP_STAT			0x98
#define SC6601_REG_DM_STAT			0x99
#define Sc6601_REG_DPDM_INTERNAL	0x9a
#define SC6601_REG_DPDM_CTRL2		0x9d
#define SC6601_REG_DPDM_NONSTD_STAT	0x9e

static const struct sc6601_chg_field sc6601_chg_fields[F_MAX] = {
	SC6601_CHG_FIELD(F_VAC_OVP, SC6601_REG_VAC_VBUS_OVP, 4, 7),
	SC6601_CHG_FIELD(F_VBUS_OVP, SC6601_REG_VAC_VBUS_OVP, 0, 2),
	SC6601_CHG_FIELD(F_ACDRV_MANUAL_PRE, SC6601_REG_HK_CTRL, 7, 7),
	SC6601_CHG_FIELD(F_ACDRV_EN, SC6601_REG_HK_CTRL, 5, 5),
	SC6601_CHG_FIELD(F_ACDRV_MANUAL_EN, SC6601_REG_HK_CTRL, 4, 4),
	SC6601_CHG_FIELD(F_WD_TIME_RST, SC6601_REG_HK_CTRL, 3, 3),
	SC6601_CHG_FIELD(F_WD_TIMER, SC6601_REG_HK_CTRL, 0, 2),
	SC6601_CHG_FIELD(F_REG_RST, SC6601_REG_HK_CTRL1, 7, 7),
	SC6601_CHG_FIELD(F_VBUS_PD, SC6601_REG_HK_CTRL1, 6, 6),
	SC6601_CHG_FIELD(F_VAC_PD, SC6601_REG_HK_CTRL1, 5, 5),
	SC6601_CHG_FIELD(F_CID_EN, SC6601_REG_HK_CTRL1, 3, 3),
	SC6601_CHG_FIELD(F_ADC_EN, SC6601_REG_HK_ADC_CTRL, 7, 7),
	SC6601_CHG_FIELD(F_ADC_FREEZE, SC6601_REG_HK_ADC_CTRL, 5, 5),

	SC6601_CHG_FIELD(F_CHG_EN, SC6601_REG_CHG_CTRL, 0, 0),
	SC6601_CHG_FIELD(F_BOOST_EN, SC6601_REG_CHG_CTRL, 1, 1),
	SC6601_CHG_FIELD(F_QB_EN, SC6601_REG_CHG_CTRL, 2, 2),

	SC6601_CHG_FIELD(F_IBATOCP, SC6601_REG_CHG_CTRL1, 3, 4),

	SC6601_CHG_FIELD(F_VSYS_MIN, SC6601_REG_VSYS_MIN, 0, 2),
	SC6601_CHG_FIELD(F_VBAT, SC6601_REG_VBAT, 0, 6),
	SC6601_CHG_FIELD(F_BATSNS_EN, SC6601_REG_VBAT, 7, 7),
	SC6601_CHG_FIELD(F_ICHG_CC, SC6601_REG_ICHG_CC, 0, 6),

	SC6601_CHG_FIELD(F_ICO_EN, SC6601_REG_ICO_CTRL, 6, 6),


	SC6601_CHG_FIELD(F_VBOOST, SC6601_REG_VBOOST_CTRL, 3, 7),
	SC6601_CHG_FIELD(F_IBOOST, SC6601_REG_VBOOST_CTRL, 0, 2),

	SC6601_CHG_FIELD(F_RECHG_DG, SC6601_REG_RECHARGE_CTRL, 2, 3),
	SC6601_CHG_FIELD(F_VRECHG, SC6601_REG_RECHARGE_CTRL, 0, 1),
	SC6601_CHG_FIELD(F_IBAT_OCP_DIS, SC6601_REG_PROTECTION_DIS, 2, 2),
	SC6601_CHG_FIELD(F_CONV_OCP_DIS, SC6601_REG_PROTECTION_DIS, 4, 4),
	SC6601_CHG_FIELD(F_VPMID_OVP_OTG_DIS, SC6601_REG_PROTECTION_DIS, 1, 1),
	SC6601_CHG_FIELD(F_VBAT_OVP_BUCK_DIS, SC6601_REG_PROTECTION_DIS, 0, 0),
	SC6601_CHG_FIELD(F_BATFET_RST_EN, SC6601_REG_RESET_CTRL, 3, 3),

	SC6601_CHG_FIELD(F_JEITA_COOL_TEMP, SC6601_REG_JEITA_TEMP, 6, 7),
	SC6601_CHG_FIELD(F_JEITA_WARM_TEMP, SC6601_REG_JEITA_TEMP, 4, 5),

	SC6601_CHG_FIELD(F_AUTO_INDET_EN, SC6601_REG_DPDM_EN, 6, 6),
	SC6601_CHG_FIELD(F_HVDCP_EN, SC6601_REG_DPDM_EN, 5, 5),
};

static inline int sc6601_chg_field_get(struct sc6601_priv *priv,
				       enum sc6601_chg_reg_field fd,
				       unsigned int *val)
{
	int ret;
	unsigned int reg_val;

	ret = regmap_field_read(priv->rmap_fields[fd], &reg_val);
	if (ret)
		return ret;

	if (sc6601_chg_fields[fd].range)
		return linear_range_get_value(sc6601_chg_fields[fd].range,
					       reg_val, val);

	*val = reg_val;
	return 0;
}

static inline int sc6601_chg_field_set(struct sc6601_priv *priv,
				       enum sc6601_chg_reg_field fd,
				       unsigned int val)
{
	int ret;
	bool f;
	const struct linear_range *r;

	if (sc6601_chg_fields[fd].range) {
		r = sc6601_chg_fields[fd].range;

		linear_range_get_selector_within(r, val, &val);
	}

	return regmap_field_write(priv->rmap_fields[fd], val);
}

/*
enum {
	MT6370_CHG_STAT_READY = 0,
	MT6370_CHG_STAT_CHARGE_IN_PROGRESS,
	MT6370_CHG_STAT_DONE,
	MT6370_CHG_STAT_FAULT,
	MT6370_CHG_STAT_MAX
};

enum {
	MT6370_ATTACH_STAT_DETACH = 0,
	MT6370_ATTACH_STAT_ATTACH_WAIT_FOR_BC12,
	MT6370_ATTACH_STAT_ATTACH_BC12_DONE,
	MT6370_ATTACH_STAT_ATTACH_MAX
};*/

#define SC6601_ADC_CHAN_VBUS	1
#define SC6601_ADC_CHAN_IBUS	0
#define SC6601_ADC_CHAN_VBAT	3
#define SC6601_ADC_CHAN_IBAT	5

static int sc6601_chg_adc_read(struct sc6601_priv *priv, int addr)
{
	__be16 be_val;
	int ret;
	unsigned int adc_data;

	u32 reg = SC6601_REG_HK_ADC + addr * 2;

	ret = regmap_bulk_read(priv->regmap, reg, &be_val, sizeof(be_val));
	if (ret) {
		dev_err(priv->dev, "Failed to read adc data!\n");
		return ret;
	}

	adc_data = be16_to_cpu(be_val);

	switch (addr) {
		case SC6601_ADC_CHAN_VBAT:
			return adc_data * 1250;
		case SC6601_ADC_CHAN_VBUS:
			return adc_data * 3750;
		case SC6601_ADC_CHAN_IBAT:
			return adc_data * 1220;
		case SC6601_ADC_CHAN_IBUS:
			return adc_data * 2500;
		default:
			return -EINVAL;
	}
}

static int sc6601_chg_get_online(struct sc6601_priv *priv,
				 union power_supply_propval *val)
{
	mutex_lock(&priv->attach_lock);
	val->intval = priv->online;
	mutex_unlock(&priv->attach_lock);

	return 0;
}

static int sc6601_chg_get_status(struct sc6601_priv *priv,
				 union power_supply_propval *val)
{
	int ret;
	unsigned int chg_stat;
	union power_supply_propval online;

	ret = power_supply_get_property(priv->psy, POWER_SUPPLY_PROP_ONLINE,
					&online);
	if (ret) {
		dev_err(priv->dev, "Failed to get online status\n");
		return ret;
	}

	if (!online.intval) {
		val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		return 0;
	}

	/*ret = mt6370_chg_field_get(priv, F_CHG_STAT, &chg_stat);
	if (ret)
		return ret;

	switch (chg_stat) {
	case MT6370_CHG_STAT_READY:
	case MT6370_CHG_STAT_FAULT:
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		return ret;
	case MT6370_CHG_STAT_CHARGE_IN_PROGRESS:
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
		return ret;
	case MT6370_CHG_STAT_DONE:
		val->intval = POWER_SUPPLY_STATUS_FULL;
		return ret;
	default:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		return ret;
	}*/
	// deleteme
	return ret;
}

static int sc6601_chg_get_charge_type(struct sc6601_priv *priv,
				      union power_supply_propval *val)
{
	int type, ret;
	unsigned int chg_stat, vbat_lvl;

	/*ret = mt6370_chg_field_get(priv, F_CHG_STAT, &chg_stat);
	if (ret)
		return ret;

	ret = mt6370_chg_field_get(priv, F_VBAT_LVL, &vbat_lvl);
	if (ret)
		return ret;

	switch (chg_stat) {
	case MT6370_CHG_STAT_CHARGE_IN_PROGRESS:
		if (vbat_lvl)
			type = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else
			type = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case MT6370_CHG_STAT_READY:
	case MT6370_CHG_STAT_DONE:
	case MT6370_CHG_STAT_FAULT:
	default:
		type = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	}

	val->intval = type;*/

	return 0;
}

static int sc6601_chg_get_chg_status(struct sc6601_priv *priv)
{
	int ret;
	u8 val;
	ret = regmap_bulk_read(priv->regmap, SC6601_REG_CHG_INT_STAT1, &val, 1);
	if (ret < 0)
		return ret;
	val >>= 5;
	val &= 0x7;
	dev_err(priv->dev, "chg status: 0x%x\n", val);
	return val;
}

static int sc6601_chg_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	struct sc6601_priv *priv = power_supply_get_drvdata(psy);
	int adc_data, ret;

	u8 chg = sc6601_chg_get_chg_status(priv);
	bool online = sc6601_chg_get_online(priv, val);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return online;
	case POWER_SUPPLY_PROP_STATUS:
		if (chg == 5)
			return POWER_SUPPLY_STATUS_FULL;
		else if (chg == 1 || chg == 4 || chg == 1)
			return POWER_SUPPLY_STATUS_CHARGING;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (chg == 3 || chg == 4)
			return POWER_SUPPLY_CHARGE_TYPE_FAST;
		else if (chg == 1)
			return POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		else
			return POWER_SUPPLY_CHARGE_TYPE_NONE;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = sc6601_chg_adc_read(priv, SC6601_ADC_CHAN_VBUS);
		return 0;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = sc6601_chg_adc_read(priv, SC6601_ADC_CHAN_IBUS);
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		val->intval = sc6601_chg_adc_read(priv, SC6601_ADC_CHAN_VBAT);
		return 0;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = sc6601_chg_adc_read(priv, SC6601_ADC_CHAN_IBAT);
		return 0;
	case POWER_SUPPLY_PROP_USB_TYPE:
		//val->intval = priv->psy_usb_type;
		return 0;
	default:
		return -EINVAL;
	}
}

static int sc6601_chg_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	struct sc6601_priv *priv = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		//return mt6370_chg_set_online(priv, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		//return mt6370_chg_field_set(priv, F_ICHG, val->intval);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		//return mt6370_chg_field_set(priv, F_VOREG, val->intval);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		//return mt6370_chg_field_set(priv, F_IAICR, val->intval);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		//return mt6370_chg_field_set(priv, F_VMIVR, val->intval);
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		//return mt6370_chg_field_set(priv, F_IPREC, val->intval);
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		//return mt6370_chg_field_set(priv, F_IEOC, val->intval);
	default:
		return -EINVAL;
	}
}

static int sc6601_chg_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property sc6601_chg_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
};

static const struct power_supply_desc sc6601_chg_psy_desc = {
	.name = "sc6601-charger",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = sc6601_chg_properties,
	.num_properties = ARRAY_SIZE(sc6601_chg_properties),
	.get_property = sc6601_chg_get_property,
	.set_property = sc6601_chg_set_property,
	.property_is_writeable = sc6601_chg_property_is_writeable,
	.usb_types = BIT(POWER_SUPPLY_USB_TYPE_SDP) |
		     BIT(POWER_SUPPLY_USB_TYPE_CDP) |
		     BIT(POWER_SUPPLY_USB_TYPE_DCP) |
		     BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN),
};

#define SC6601_OTG_VBOOST_MIN	3900000
#define SC6601_OTG_VBOOST_MAX	5800000
#define SC6601_OTG_VBOOST_STEP	100
#define SC6601_OTG_IBOOST_MIN	500000
#define SC6601_OTG_IBOOST_MAX	3250000

static const int sc6601_boost_curr_range[] = {
	500000, 900000, 1300000, 1500000, 2100000, 2500000, 2900000, 3250000,
};

static int sc6601_vboost_ctrl(struct sc6601_priv *priv, bool en)
{
	int ret, retries;
	u8 boost_state;

	sc6601_chg_field_set(priv, F_QB_EN, en);
	sc6601_chg_field_set(priv, F_CHG_EN, !en);

	retries = 0;

	do {
		boost_state = 0;
		ret = sc6601_chg_field_set(priv, F_BOOST_EN, en);
		if (ret < 0) {
			sc6601_chg_field_set(priv, F_CHG_EN, 1);
			return ret;
		}
		mdelay(30);
		ret = regmap_bulk_read(priv->regmap, SC6601_REG_CHG_INT_STAT1, &boost_state, 1);
		if (ret < 0) {
			sc6601_chg_field_set(priv, F_CHG_EN, 1);
			return ret;
		}
		if (retries++ > 3) {
			sc6601_chg_field_set(priv, F_CHG_EN, 1);
			return -EIO;
		}
	} while ( (en && !(boost_state & BIT(4))) || (!en && (boost_state & BIT(4))) );

	return 0;
}

static int sc6601_vbus_enable(struct regulator_dev *rdev)
{
	struct sc6601_priv *priv = rdev_get_drvdata(rdev);

	dev_err(priv->dev, "vbus enable!\n");

	return sc6601_vboost_ctrl(priv, 1);
}

static int sc6601_vbus_disable(struct regulator_dev *rdev)
{
	struct sc6601_priv *priv = rdev_get_drvdata(rdev);

	dev_err(priv->dev, "vbus disable!\n");

	return sc6601_vboost_ctrl(priv, 0);
}

static int sc6601_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct sc6601_priv *priv = rdev_get_drvdata(rdev);
	int ret, vboost;

	ret = sc6601_chg_field_get(priv, F_BOOST_EN, &vboost);
	if (ret)
		dev_err(priv->dev, "failed to disable vbus\n");

	return vboost;
}

static int sc6601_vbus_get_voltage(struct regulator_dev *rdev)
{
	struct sc6601_priv *priv = rdev_get_drvdata(rdev);
	int ret, vboost;

	ret = sc6601_chg_field_get(priv, F_VBOOST, &vboost);
	if (ret) {
		dev_err(priv->dev, "failed to get vboost voltage!");
		return ret;
	}

	return (vboost * SC6601_OTG_VBOOST_STEP) * 1000;
}

static int sc6601_vbus_set_voltage(struct regulator_dev *rdev,
					int min_uV, int max_uV, unsigned *selector)
{
	struct sc6601_priv *priv = rdev_get_drvdata(rdev);

	int ret;
	int uv = max_uV;
	if (max_uV > SC6601_OTG_VBOOST_MAX)
		uv = SC6601_OTG_VBOOST_MAX;

	dev_err(priv->dev, "vbus set %d uV!", uv);

	uv = (uv / 1000) / SC6601_OTG_VBOOST_STEP;

	ret = sc6601_chg_field_set(priv, F_VBOOST, uv);
	if (ret)
		dev_err(priv->dev, "failed to set vboost voltage!");
	return ret;
}

static int sc6601_vbus_get_current(struct regulator_dev *rdev)
{
	struct sc6601_priv *priv = rdev_get_drvdata(rdev);
	int ret, iboost;

	ret = sc6601_chg_field_get(priv, F_IBOOST, &iboost);
	if (ret) {
		dev_err(priv->dev, "failed to get iboost voltage!");
		return ret;
	}

	return sc6601_boost_curr_range[iboost];
}

static int sc6601_vbus_set_current(struct regulator_dev *rdev,
								   int min_uA, int max_uA)
{
	struct sc6601_priv *priv = rdev_get_drvdata(rdev);

	int ret, ua;
	int i;

	if (max_uA < sc6601_boost_curr_range[0]) {
		ua = sc6601_boost_curr_range[0];
	} else if (max_uA > sc6601_boost_curr_range[ARRAY_SIZE(sc6601_boost_curr_range) - 1]) {
		ua = sc6601_boost_curr_range[ARRAY_SIZE(sc6601_boost_curr_range) - 1];
	} else {
		ua = max_uA;
	}

	for (i = 0; i <= ARRAY_SIZE(sc6601_boost_curr_range) - 1; i++) {
		if (ua < sc6601_boost_curr_range[i])
			break;
	}

	dev_err(priv->dev, "vbus set %d [%d] uA!", ua, i);

	ret = sc6601_chg_field_set(priv, F_IBOOST, i);
	if (ret)
		dev_err(priv->dev, "failed to set iboost current!");
	return ret;
}

static const struct regulator_ops sc6601_chg_otg_ops = {
	.enable = sc6601_vbus_enable,
	.disable = sc6601_vbus_disable,
	.is_enabled = sc6601_vbus_is_enabled,
	.get_voltage = sc6601_vbus_get_voltage,
	.set_voltage = sc6601_vbus_set_voltage,
	.set_current_limit = sc6601_vbus_set_current,
	.get_current_limit = sc6601_vbus_get_current
};

static const struct regulator_desc sc6601_chg_otg_rdesc = {
	.of_match = "usb-otg-vbus-regulator",
	.name = "sc6601-usb-otg-vbus",
	.ops = &sc6601_chg_otg_ops,
	.owner = THIS_MODULE,
	.type = REGULATOR_VOLTAGE,
};

static int sc6601_chg_init_rmap_fields(struct sc6601_priv *priv)
{
	int i;
	const struct sc6601_chg_field *fds = sc6601_chg_fields;

	for (i = 0; i < F_MAX; i++) {
		priv->rmap_fields[i] = devm_regmap_field_alloc(priv->dev,
							       priv->regmap,
							       fds[i].field);
		if (IS_ERR(priv->rmap_fields[i]))
			return dev_err_probe(priv->dev,
					PTR_ERR(priv->rmap_fields[i]),
					"Failed to allocate regmapfield[%s]\n",
					fds[i].name);
	}

	return 0;
}

static const int sc6601_wd_range[] = {
	0, 500, 1000, 2000, 20000, 40000, 80000, 160000,
};

static int sc6601_chg_set_wd_timeout(struct sc6601_priv *priv, int ms)
{
	int i = 0;
	if (ms < sc6601_wd_range[0]) {
		ms = sc6601_wd_range[0];
	}

	if (ms > sc6601_wd_range[ARRAY_SIZE(sc6601_wd_range) - 1]) {
		ms = sc6601_wd_range[ARRAY_SIZE(sc6601_wd_range) - 1];
	}

	for (i = 0; i < ARRAY_SIZE(sc6601_wd_range); i++) {
		if (ms <= sc6601_wd_range[i])
			break;
	}
	return sc6601_chg_field_set(priv, F_WD_TIMER, i);
}

static int sc6601_chg_acdrv_ctrl(struct sc6601_priv *priv, bool en)
{
	int ret;
	int cnt = 0;
	int from_ic;

	ret = sc6601_chg_field_get(priv, F_ACDRV_EN, &from_ic);

	do {
		if (cnt++ > 3) {
			return -EIO;
		}

		ret = sc6601_chg_field_set(priv, F_ACDRV_EN, en);
		if (ret < 0)
			continue;

		ret = sc6601_chg_field_get(priv, F_ACDRV_EN, &from_ic);
		if (ret < 0)
			continue;
	} while (en != from_ic);

	return 0;
}

#define SC6601_BUCK_VBAT_OFFSET	3840
#define SC6601_BUCK_VBAT_STEP	8
#define SC6601_BUCK_ICHG_STEP	50
#define SC6601_BUCK_ICHG_MAX	3600

static int sc6601_chg_init_setting(struct sc6601_priv *priv)
{
	struct power_supply_battery_info *bat;
	int ret, bat_mv, bat_ma, ibat_ocp;
	u8 val;

	ret = power_supply_get_battery_info(priv->psy, &bat);
	if (ret) {
		dev_err(priv->dev, "Failed to get battery info!\n");
		return ret;
	}

	sc6601_chg_field_set(priv, F_REG_RST, 1);
	sc6601_chg_field_set(priv, F_CHG_EN, 0);

	// set buck freq = 1M , boost freq = 1M
	val = 0x8a;
	regmap_bulk_write(priv->regmap, SC6601_REG_CHG_CTRL4, &val, 1);

	sc6601_chg_field_set(priv, F_CHG_EN, 1);
	sc6601_chg_set_wd_timeout(priv, 0);

	// 14000 mV
	sc6601_chg_field_set(priv, F_VAC_OVP, 4);

	sc6601_chg_field_set(priv, F_ICO_EN, 0);
	sc6601_chg_acdrv_ctrl(priv, true);
	sc6601_chg_field_set(priv, F_BATFET_RST_EN, 0);

	// 3500 mV
	sc6601_chg_field_set(priv, F_VSYS_MIN, 4);

	sc6601_chg_field_set(priv, F_AUTO_INDET_EN, 0);

	//val = 0xff;
	//regmap_bulk_write(priv->regmap, SC6601_REG_QC3_INT_MASK, &val, 1);

	sc6601_chg_field_set(priv, F_BATSNS_EN, 0);
	bat_mv = bat->voltage_max_design_uv / 1000;
	bat_mv -= SC6601_BUCK_VBAT_OFFSET;
	bat_mv /= SC6601_BUCK_VBAT_STEP;
	sc6601_chg_field_set(priv, F_VBAT, bat_mv);
	bat_ma = bat->constant_charge_current_max_ua / 1000;
	if (bat_ma > 12000)
		ibat_ocp = 1;
	else
		ibat_ocp = 0;
	if (bat_ma > SC6601_BUCK_ICHG_MAX)
		bat_ma = SC6601_BUCK_ICHG_MAX;
	bat_ma /= SC6601_BUCK_VBAT_STEP;
	sc6601_chg_field_set(priv, F_ICHG_CC, bat_ma);

	// recharge after 100mV
	sc6601_chg_field_set(priv, F_RECHG_DG, 0);
	sc6601_chg_field_set(priv, F_VRECHG, 0);

	// protections
	sc6601_chg_field_set(priv, F_CONV_OCP_DIS, 0);
	sc6601_chg_field_set(priv, F_IBAT_OCP_DIS, 0);
	sc6601_chg_field_set(priv, F_VPMID_OVP_OTG_DIS, 0);
	sc6601_chg_field_set(priv, F_VBAT_OVP_BUCK_DIS, 0);
	// 0: 12A, 1: 16A
	sc6601_chg_field_set(priv, F_IBATOCP, ibat_ocp);

	sc6601_chg_field_set(priv, F_ACDRV_MANUAL_EN, 1);
	sc6601_chg_field_set(priv, F_ACDRV_MANUAL_PRE, 1);
	sc6601_chg_field_set(priv, F_HVDCP_EN, 0);

	/* set jeita, cool -> 5, warm -> 54.5 */
	sc6601_chg_field_set(priv, F_JEITA_COOL_TEMP, 0);
	sc6601_chg_field_set(priv, F_JEITA_WARM_TEMP, 3);

	sc6601_chg_field_set(priv, F_ADC_EN, 1);

	val = 0xb0;
	regmap_bulk_write(priv->regmap, 0x60, &val, 1);

	return 0;
}

static int sc6601_chg_init_otg_regulator(struct sc6601_priv *priv)
{
	struct regulator_config rcfg = {
		.dev = priv->dev,
		.regmap = priv->regmap,
		.driver_data = priv,
	};

	priv->rdev = devm_regulator_register(priv->dev, &sc6601_chg_otg_rdesc,
					     &rcfg);

	return PTR_ERR_OR_ZERO(priv->rdev);
}

static int sc6601_chg_init_psy(struct sc6601_priv *priv)
{
	struct power_supply_config cfg = {
		.drv_data = priv,
		.of_node = dev_of_node(priv->dev),
	};

	priv->psy = devm_power_supply_register(priv->dev, &sc6601_chg_psy_desc,
					       &cfg);

	return PTR_ERR_OR_ZERO(priv->psy);
}

static void sc6601_chg_destroy_attach_lock(void *data)
{
	struct mutex *attach_lock = data;

	mutex_destroy(attach_lock);
}

static void sc6601_chg_destroy_wq(void *data)
{
	struct workqueue_struct *wq = data;

	flush_workqueue(wq);
	destroy_workqueue(wq);
}

static irqreturn_t sc6601_chg_irq_charger(int irq, void *data)
{
	struct sc6601_priv *priv = data;
	u8 val[3];
	u32 flt, state;
	int ret;

	dev_err(priv->dev, "charger irq happen!\n");
	ret = regmap_bulk_read(priv->regmap, SC6601_REG_CHG_FLT_FLG, &val, 2);
	flt = val[0] + (val[1] << 8);
	if (flt != 0) {
		dev_err(priv->dev, "charger FAULT: 0x%x:\n", flt);
	}
	ret = regmap_bulk_read(priv->regmap, SC6601_REG_CHG_INT_FLG, &val, 3);
	state = val[0] + (val[1] << 8) + (val[2] << 16);

	if (state & BIT(12)) {
		dev_err(priv->dev, "boost good");
	}

	return IRQ_HANDLED;
}

static int sc6601_chg_check_hk(struct sc6601_priv *priv)
{
	int ret;
	u8 val;
	ret = regmap_bulk_read(priv->regmap, SC6601_REG_HK_INT_STAT, &val, 1);
	if ((val & BIT(0)) && (val & BIT(1))) {
		priv->online = true;
	} else {
		priv->online = false;
	}

	power_supply_changed(priv->psy);
	return ret;
}

static irqreturn_t sc6601_chg_irq_hk(int irq, void *data)
{
	struct sc6601_priv *priv = data;
	u8 val;
	int ret;

	dev_err(priv->dev, "hourse_keeping irq happen!\n");
	ret = regmap_bulk_read(priv->regmap, SC6601_REG_HK_FLT_FLG, &val, 1);
	if (val != 0) {
		dev_err(priv->dev, "hourse_keeping FAULT: 0x%x:\n", val);
	}

	ret = sc6601_chg_check_hk(priv);

	return IRQ_HANDLED;
}

static int sc6601_chg_init_irq(struct sc6601_priv *priv)
{
	int ret;

	ret = platform_get_irq_byname(to_platform_device(priv->dev), "charger");
	if (ret < 0) {
		dev_err(priv->dev, "failed to get charger irq\n");
		return ret;
	}

	priv->irq[0] = ret;

	ret = devm_request_threaded_irq(priv->dev, ret, NULL,
					sc6601_chg_irq_charger, IRQF_ONESHOT,
					dev_name(priv->dev), priv);
	if (ret < 0) {
		dev_err(priv->dev, "failed to request charger irq\n");
		return ret;
	}

	ret = platform_get_irq_byname(to_platform_device(priv->dev), "hourse_keeping");
	if (ret < 0) {
		dev_err(priv->dev, "failed to get charger irq\n");
		return ret;
	}

	priv->irq[1] = ret;

	ret = devm_request_threaded_irq(priv->dev, ret, NULL,
								sc6601_chg_irq_hk, IRQF_ONESHOT,
								dev_name(priv->dev), priv);
	if (ret < 0) {
		dev_err(priv->dev, "failed to request hourse keeping irq\n");
		return ret;
	}

	return 0;
}

static int sc6601_chg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sc6601_priv *priv;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &pdev->dev;
	priv->online = false;

	priv->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!priv->regmap)
		return dev_err_probe(dev, -ENODEV, "Failed to get regmap\n");

	ret = sc6601_chg_init_rmap_fields(priv);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init regmap fields\n");

	platform_set_drvdata(pdev, priv);

	ret = sc6601_chg_init_psy(priv);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init psy\n");
	/*
	mutex_init(&priv->attach_lock);
	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_attach_lock,
				       &priv->attach_lock);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init attach lock\n");

	priv->attach = SC6601_ATTACH_STAT_DETACH;

	priv->wq = create_singlethread_workqueue(dev_name(priv->dev));
	if (!priv->wq)
		return dev_err_probe(dev, -ENOMEM,
				     "Failed to create workqueue\n");

	ret = devm_add_action_or_reset(dev, mt6370_chg_destroy_wq, priv->wq);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init wq\n");

	ret = devm_work_autocancel(dev, &priv->bc12_work, mt6370_chg_bc12_work_func);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init bc12 work\n");*/

	ret = sc6601_chg_init_setting(priv);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to init sc6601 charger setting\n");

	ret = sc6601_chg_init_irq(priv);
	if (ret)
		return ret;

	ret = sc6601_chg_init_otg_regulator(priv);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to init OTG regulator\n");

	sc6601_chg_check_hk(priv);

	return 0;
}

static const struct of_device_id sc6601_chg_of_match[] = {
	{ .compatible = "southchip,sc6601-charger", },
	{}
};
MODULE_DEVICE_TABLE(of, sc6601_chg_of_match);

static struct platform_driver sc6601_chg_driver = {
	.probe = sc6601_chg_probe,
	.driver = {
		.name = "sc6601-charger",
		.of_match_table = sc6601_chg_of_match,
	},
};
module_platform_driver(sc6601_chg_driver);

MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_DESCRIPTION("SouthChip SC6601 Charger Driver");
MODULE_LICENSE("GPL v2");
