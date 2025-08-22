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

enum {
	SC6601_TCPC_I2C = 0,
	SC6601_CHARGER_I2C,
	SC6601_MAX_I2C
};

struct sc6601_info {
	struct i2c_client *i2c[SC6601_MAX_I2C];
	struct regmap_irq_chip_data *irq_data;
};

#endif // __MFD_SC6601_H__
