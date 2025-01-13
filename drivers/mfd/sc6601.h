#ifndef __MFD_SC6601_H__
#define __MFD_SC6601_H__

/* IRQ definitions */
#define SC6601_IRQ_CHARGER		0
#define SC6601_IRQ_DVCHG		1
#define SC6601_IRQ_LED			2
#define SC6601_IRQ_DPDM			3
#define SC6601_IRQ_UFCS			5
#define SC6601_IRQ_HK			6
#define SC6601_IRQ_CID			7
#define SC6601_IRQ_MAX			8

#define SC6601_REG_HK_DID			0x00
#define SC6601_REG_HK_IRQ			0x02
#define SC6601_REG_HK_IRQ_MASK		0x03

#define SC6601_DEVICE_ID			0x66
#define SC6601_1P1_DEVICE_ID		0x61

struct sc6601_info {
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct irq_chip irq_chip;
	struct irq_domain *irq_domain;
	struct mutex irq_lock;
	uint8_t irq_mask;
};

#endif // __MFD_SC6601_H__
