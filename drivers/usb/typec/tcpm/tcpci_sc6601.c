// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025 Arseniy Velikanov <me@adomerle.pw>
 */

#include <linux/bits.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeirq.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/tcpci.h>
#include <linux/usb/tcpm.h>

#define BMCIO_RXDZEN	BIT(0)

#define SC6601_VCONN_CLIMITEN		0x95

#define SC6601_PHYCTRL1	0x80
#define SC6601_PHYCTRL2	0x81

#define SC6601_BMCIO_RXDZSEL		0x93
#define SC6601_BMCIO_RXDZSEL_MASK	BIT(5) | BIT(6) | BIT(7)

#define RT1711H_RTCTRL8		0x9B
/* Autoidle timeout = (tout * 2 + 1) * 6.4ms */
#define RT1711H_RTCTRL8_SET(ck300, ship_off, auto_idle, tout) \
			    (((ck300) << 7) | ((ship_off) << 5) | \
			    ((auto_idle) << 3) | ((tout) & 0x07))
#define RT1711H_AUTOIDLEEN	BIT(3)
#define RT1711H_ENEXTMSG	BIT(4)

#define RT1711H_RTCTRL11	0x9E

/* I2C timeout = (tout + 1) * 12.5ms */
#define RT1711H_RTCTRL11_SET(en, tout) \
			     (((en) << 7) | ((tout) & 0x0F))

#define RT1711H_RTCTRL13	0xA0
#define RT1711H_RTCTRL14	0xA1
#define RT1711H_RTCTRL15	0xA2
#define RT1711H_RTCTRL16	0xA3

#define SC6601_BMCIO_RXDZEN	0xAF

#define RT1711H_REG_RT_INT					(0x98)
#define RT1711H_REG_RT_MASK					(0x99)


struct sc6601_priv {
	struct device *dev;
	struct regulator *vbus;
	struct tcpci *tcpci;
	struct tcpci_data tcpci_data;
};

static int sc6601_read16(struct sc6601_priv *priv, unsigned int reg, u16 *val)
{
	return regmap_raw_read(priv->tcpci_data.regmap, reg, val, sizeof(u16));
}

static int sc6601_write16(struct sc6601_priv *priv, unsigned int reg, u16 val)
{
	return regmap_raw_write(priv->tcpci_data.regmap, reg, &val, sizeof(u16));
}

static int sc6601_read8(struct sc6601_priv *priv, unsigned int reg, u8 *val)
{
	return regmap_raw_read(priv->tcpci_data.regmap, reg, val, sizeof(u8));
}

static int sc6601_write8(struct sc6601_priv *priv, unsigned int reg, u8 val)
{
	return regmap_raw_write(priv->tcpci_data.regmap, reg, &val, sizeof(u8));
}

static int sc6601_tcpc_init(struct tcpci *tcpci, struct tcpci_data *data)
{
	struct sc6601_priv *priv = container_of(data, struct sc6601_priv,
						tcpci_data);
	int ret;
	/* CK 300K from 320K, shipping off, auto_idle enable, tout = 32ms */
	ret = sc6601_write8(priv, RT1711H_RTCTRL8,
			     RT1711H_RTCTRL8_SET(0, 1, 1, 2));
	if (ret < 0)
		return ret;

	/* I2C reset : (val + 1) * 12.5ms */
	ret = sc6601_write8(priv, RT1711H_RTCTRL11,
			     RT1711H_RTCTRL11_SET(1, 0x0F));
	if (ret < 0)
		return ret;

	/* tTCPCfilter : (26.7 * val) us */
	// 0x0a instead of 0x0f
	ret = sc6601_write8(priv, RT1711H_RTCTRL14, 0x0a);
	if (ret < 0)
		return ret;

	/*  tDRP : (51.2 + 6.4 * val) ms */
	ret = sc6601_write8(priv, RT1711H_RTCTRL15, 0x04);
	if (ret < 0)
		return ret;

	/* dcSRC.DRP : 33% */
	ret = sc6601_write16(priv, RT1711H_RTCTRL16, 330);
	if (ret < 0)
		return ret;

	/* VConn OC */
	ret = sc6601_write8(priv, SC6601_VCONN_CLIMITEN, 1);

	/* Enable phy discard retry, retry count 7, rx filter deglitch 100 us */
	ret = sc6601_write8(priv, SC6601_PHYCTRL1, 0xF1);
	if (ret < 0)
		return ret;

	/* Decrease wait time of BMC-encoded 1 bit from 2.67us to 2.55us */
	/* wait time : (val * .4167) us */
	return sc6601_write8(priv, SC6601_PHYCTRL2, 62);
}

static int sc6601_tcpc_set_vconn(struct tcpci *tcpci, struct tcpci_data *data,
				 bool enable)
{
	return regmap_update_bits(data->regmap, RT1711H_RTCTRL8,
				  RT1711H_AUTOIDLEEN, enable ? 0 : RT1711H_AUTOIDLEEN);
}

static int sc6601_tcpc_set_vbus(struct tcpci *tcpci, struct tcpci_data *data,
				bool source, bool sink)
{
	struct sc6601_priv *priv = container_of(data, struct sc6601_priv,
						tcpci_data);
	int ret;

	ret = regulator_is_enabled(priv->vbus);
	if (ret < 0)
		return ret;

	if (ret && !source)
		return regulator_disable(priv->vbus);

	if (!ret && source)
		return regulator_enable(priv->vbus);

	return 0;
}

static inline int sc6601_init_cc_params(struct sc6601_priv *priv, u8 status)
{
	int ret, cc1, cc2;
	u8 role = 0;
	u32 rxdz_en, rxdz_sel;

	ret = sc6601_read8(priv, TCPC_ROLE_CTRL, &role);
	if (ret < 0)
		return ret;

	cc1 = tcpci_to_typec_cc(FIELD_GET(TCPC_CC_STATUS_CC1, status),
				status & TCPC_CC_STATUS_TERM ||
				tcpc_presenting_rd(role, CC1));
	cc2 = tcpci_to_typec_cc(FIELD_GET(TCPC_CC_STATUS_CC2, status),
				status & TCPC_CC_STATUS_TERM ||
				tcpc_presenting_rd(role, CC2));

	if ((cc1 >= TYPEC_CC_RP_1_5 && cc2 < TYPEC_CC_RP_DEF) ||
	    (cc2 >= TYPEC_CC_RP_1_5 && cc1 < TYPEC_CC_RP_DEF)) {
		rxdz_en = BMCIO_RXDZEN;
		rxdz_sel = 0x80;
	} else {
		rxdz_en = 0;
		rxdz_sel = 0x81;
	}

	ret = regmap_update_bits(priv->tcpci_data.regmap, SC6601_BMCIO_RXDZEN,
				 BMCIO_RXDZEN, rxdz_en);
	if (ret < 0)
		return ret;

	return regmap_update_bits(priv->tcpci_data.regmap, SC6601_BMCIO_RXDZSEL,
				  SC6601_BMCIO_RXDZSEL_MASK, rxdz_sel);
}

static int sc6601_start_drp_toggling(struct tcpci *tcpci,
				      struct tcpci_data *data,
				      enum typec_cc_status cc)
{
	struct sc6601_priv *priv = container_of(data, struct sc6601_priv,
						tcpci_data);
	int ret;
	unsigned int reg = 0;

	switch (cc) {
	default:
	case TYPEC_CC_RP_DEF:
		reg |= FIELD_PREP(TCPC_ROLE_CTRL_RP_VAL,
				  TCPC_ROLE_CTRL_RP_VAL_DEF);
		break;
	case TYPEC_CC_RP_1_5:
		reg |= FIELD_PREP(TCPC_ROLE_CTRL_RP_VAL,
				  TCPC_ROLE_CTRL_RP_VAL_1_5);
		break;
	case TYPEC_CC_RP_3_0:
		reg |= FIELD_PREP(TCPC_ROLE_CTRL_RP_VAL,
				  TCPC_ROLE_CTRL_RP_VAL_3_0);
		break;
	}

	if (cc == TYPEC_CC_RD)
		reg |= (FIELD_PREP(TCPC_ROLE_CTRL_CC1, TCPC_ROLE_CTRL_CC_RD)
			| FIELD_PREP(TCPC_ROLE_CTRL_CC2, TCPC_ROLE_CTRL_CC_RD));
	else
		reg |= (FIELD_PREP(TCPC_ROLE_CTRL_CC1, TCPC_ROLE_CTRL_CC_RP)
			| FIELD_PREP(TCPC_ROLE_CTRL_CC2, TCPC_ROLE_CTRL_CC_RP));

	ret = sc6601_write8(priv, TCPC_ROLE_CTRL, reg);
	if (ret < 0)
		return ret;
	usleep_range(500, 1000);

	return 0;
}

static irqreturn_t sc6601_irq(int irq, void *dev_id)
{
	int ret;
	u16 alert;
	u8 status;
	struct sc6601_priv *priv = dev_id;

	if (!priv->tcpci)
		return IRQ_HANDLED;

	ret = sc6601_read16(priv, TCPC_ALERT, &alert);
	if (ret < 0)
		goto out;

	if (alert & TCPC_ALERT_FAULT) {
		sc6601_read8(priv, TCPC_FAULT_STATUS, &status);
		/* clear fault interrupt */
		sc6601_write8(priv, TCPC_FAULT_STATUS, status);
		/* clear alert interrupt */
		sc6601_write16(priv, TCPC_ALERT, TCPC_ALERT_FAULT);

		dev_err(priv->dev, "fault: 0x%02x\n", status);
	}

	if (alert & TCPC_ALERT_CC_STATUS) {
		ret = sc6601_read8(priv, TCPC_CC_STATUS, &status);
		if (ret < 0)
			goto out;
		/* Clear cc change event triggered by starting toggling */
		if (status & TCPC_CC_STATUS_TOGGLING)
			sc6601_write8(priv, TCPC_ALERT, TCPC_ALERT_CC_STATUS);
		else
			sc6601_init_cc_params(priv, status);
	}

out:
	return tcpci_irq(priv->tcpci);
}

static int sc6601_sw_reset(struct sc6601_priv *priv)
{
	int ret;

	ret = sc6601_write8(priv, RT1711H_RTCTRL13, 0x01);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);
	return 0;
}

static void sc6601_unregister_tcpci_port(void *tcpci)
{
	tcpci_unregister_port(tcpci);
}

static int sc6601_tcpc_probe(struct platform_device *pdev)
{
	struct sc6601_priv *priv;
	struct device *dev = &pdev->dev;
	const u16 alert_mask = TCPC_ALERT_TX_SUCCESS | TCPC_ALERT_TX_DISCARDED |
			       TCPC_ALERT_TX_FAILED | TCPC_ALERT_RX_HARD_RST |
			       TCPC_ALERT_RX_STATUS | TCPC_ALERT_POWER_STATUS |
			       TCPC_ALERT_CC_STATUS | TCPC_ALERT_RX_BUF_OVF |
			       TCPC_ALERT_FAULT;
	int irq, ret;

	dev_err(dev, "probing!");

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	priv->tcpci_data.regmap = dev_get_regmap(dev->parent, NULL);
	if (!priv->tcpci_data.regmap)
		return dev_err_probe(dev, -ENODEV, "Failed to init regmap\n");

	ret = sc6601_sw_reset(priv);
	if (ret < 0)
		return ret;

	/* Disable chip interrupts before requesting irq */
	sc6601_write16(priv, TCPC_ALERT_MASK, 0);
	sc6601_write8(priv, RT1711H_REG_RT_MASK, 0);
	sc6601_write8(priv, RT1711H_REG_RT_INT, 0xff);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/* Assign TCPCI feature and ops */
	priv->tcpci_data.init = sc6601_tcpc_init;
	priv->tcpci_data.set_vconn = sc6601_tcpc_set_vconn;
	priv->tcpci_data.start_drp_toggling = sc6601_start_drp_toggling;

	priv->vbus = devm_regulator_get_optional(dev, "vbus");
	if (!IS_ERR(priv->vbus))
		priv->tcpci_data.set_vbus = sc6601_tcpc_set_vbus;

	priv->tcpci = tcpci_register_port(dev, &priv->tcpci_data);
	if (IS_ERR(priv->tcpci))
		return dev_err_probe(dev, PTR_ERR(priv->tcpci),
				     "Failed to register tcpci port\n");

	ret = devm_add_action_or_reset(dev, sc6601_unregister_tcpci_port, priv->tcpci);
	if (ret)
		return ret;

	ret = devm_request_threaded_irq(dev, irq, NULL, sc6601_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW, dev_name(dev), priv);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to allocate irq\n");

	/* Enable alert interrupts */
	ret = sc6601_write16(priv, TCPC_ALERT_MASK, alert_mask);
	if (ret < 0)
		return ret;

	device_init_wakeup(dev, true);
	dev_pm_set_wake_irq(dev, irq);

	return 0;
}

static void sc6601_tcpc_remove(struct platform_device *pdev)
{
	dev_pm_clear_wake_irq(&pdev->dev);
	device_init_wakeup(&pdev->dev, false);
}

static const struct of_device_id sc6601_tcpc_devid_table[] = {
	{ .compatible = "southchip,sc6601-tcpc" },
	{}
};
MODULE_DEVICE_TABLE(of, sc6601_tcpc_devid_table);

static struct platform_driver sc6601_tcpc_driver = {
	.driver = {
		.name = "sc6601-tcpc",
		.of_match_table = sc6601_tcpc_devid_table,
	},
	.probe = sc6601_tcpc_probe,
	.remove = sc6601_tcpc_remove,
};
module_platform_driver(sc6601_tcpc_driver);

MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_DESCRIPTION("SC6601 USB Type-C Port Controller Interface Driver");
MODULE_LICENSE("GPL v2");
