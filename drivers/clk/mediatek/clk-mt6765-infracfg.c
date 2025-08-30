// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Copyright (c) 2025 Arseniy Velikanov <me@adomerle.pw>
 */
#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6765-clk.h>

static const struct mtk_gate_regs ifr2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifr3_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifr4_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs ifr5_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

#define GATE_IFR2(_id, _name, _parent, _shift)				\
	GATE_MTK(_id, _name, _parent, &ifr2_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

#define GATE_IFR3(_id, _name, _parent, _shift)				\
	GATE_MTK(_id, _name, _parent, &ifr3_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

#define GATE_IFR4(_id, _name, _parent, _shift)				\
	GATE_MTK(_id, _name, _parent, &ifr4_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

#define GATE_IFR5(_id, _name, _parent, _shift)				\
	GATE_MTK(_id, _name, _parent, &ifr5_cg_regs, _shift, &mtk_clk_gate_ops_setclr)

static const struct mtk_gate infra_clks[] = {
	/* INFRA_TOPAXI */
	/* INFRA PERI */
	/* INFRA mode 0 */
	GATE_IFR2(CLK_IFR_PMIC_AP, "ifr_pmic_ap", "axi_sel", 1),
	GATE_IFR2(CLK_IFR_ICUSB, "ifr_icusb", "axi_sel", 8),
	GATE_IFR2(CLK_IFR_GCE, "ifr_gce", "axi_sel", 9),
	GATE_IFR2(CLK_IFR_THERM, "ifr_therm", "axi_sel", 10),
	GATE_IFR2(CLK_IFR_I2C_AP, "ifr_i2c_ap", "i2c_sel", 11),
	GATE_IFR2(CLK_IFR_I2C_CCU, "ifr_i2c_ccu", "i2c_sel", 12),
	GATE_IFR2(CLK_IFR_I2C_SSPM, "ifr_i2c_sspm", "i2c_sel", 13),
	GATE_IFR2(CLK_IFR_I2C_RSV, "ifr_i2c_rsv", "i2c_sel", 14),
	GATE_IFR2(CLK_IFR_PWM_HCLK, "ifr_pwm_hclk", "axi_sel", 15),
	GATE_IFR2(CLK_IFR_PWM1, "ifr_pwm1", "pwm_sel", 16),
	GATE_IFR2(CLK_IFR_PWM2, "ifr_pwm2", "pwm_sel", 17),
	GATE_IFR2(CLK_IFR_PWM3, "ifr_pwm3", "pwm_sel", 18),
	GATE_IFR2(CLK_IFR_PWM4, "ifr_pwm4", "pwm_sel", 19),
	GATE_IFR2(CLK_IFR_PWM5, "ifr_pwm5", "pwm_sel", 20),
	GATE_IFR2(CLK_IFR_PWM, "ifr_pwm", "pwm_sel", 21),
	GATE_IFR2(CLK_IFR_UART0, "ifr_uart0", "uart_sel", 22),
	GATE_IFR2(CLK_IFR_UART1, "ifr_uart1", "uart_sel", 23),
	GATE_IFR2(CLK_IFR_GCE_26M, "ifr_gce_26m", "clk26m", 27),
	GATE_IFR2(CLK_IFR_CQ_DMA_FPC, "ifr_dma", "axi_sel", 28),
	GATE_IFR2(CLK_IFR_BTIF, "ifr_btif", "axi_sel", 31),
	/* INFRA mode 1 */
	GATE_IFR3(CLK_IFR_SPI0, "ifr_spi0", "spi_sel", 1),
	GATE_IFR3(CLK_IFR_MSDC0, "ifr_msdc0", "msdc5hclk", 2),
	GATE_IFR3(CLK_IFR_MSDC1, "ifr_msdc1", "axi_sel", 4),
	GATE_IFR3(CLK_IFR_TRNG, "ifr_trng", "axi_sel", 9),
	GATE_IFR3(CLK_IFR_AUXADC, "ifr_auxadc", "clk26m", 10),
	GATE_IFR3(CLK_IFR_CCIF1_AP, "ifr_ccif1_ap", "axi_sel", 12),
	GATE_IFR3(CLK_IFR_CCIF1_MD, "ifr_ccif1_md", "axi_sel", 13),
	GATE_IFR3(CLK_IFR_AUXADC_MD, "ifr_auxadc_md", "clk26m", 14),
	GATE_IFR3(CLK_IFR_AP_DMA, "ifr_ap_dma", "axi_sel", 18),
	GATE_IFR3(CLK_IFR_DEVICE_APC, "ifr_dapc", "axi_sel", 20),
	GATE_IFR3(CLK_IFR_CCIF_AP, "ifr_ccif_ap", "axi_sel", 23),
	GATE_IFR3(CLK_IFR_AUDIO, "ifr_audio", "axi_sel", 25),
	GATE_IFR3(CLK_IFR_CCIF_MD, "ifr_ccif_md", "axi_sel", 26),
	/* INFRA mode 2 */
	GATE_IFR4(CLK_IFR_RG_PWM_FBCLK6, "ifr_pwmfb", "clk26m", 0),
	GATE_IFR4(CLK_IFR_DISP_PWM, "ifr_disp_pwm", "disp_pwm_sel", 2),
	GATE_IFR4(CLK_IFR_CLDMA_BCLK, "ifr_cldmabclk", "axi_sel", 3),
	GATE_IFR4(CLK_IFR_AUDIO_26M_BCLK, "ifr_audio26m", "clk26m", 4),
	GATE_IFR4(CLK_IFR_SPI1, "ifr_spi1", "spi_sel", 6),
	GATE_IFR4(CLK_IFR_I2C4, "ifr_i2c4", "i2c_sel", 7),
	GATE_IFR4(CLK_IFR_SPI2, "ifr_spi2", "spi_sel", 9),
	GATE_IFR4(CLK_IFR_SPI3, "ifr_spi3", "spi_sel", 10),
	GATE_IFR4(CLK_IFR_I2C5, "ifr_i2c5", "i2c_sel", 18),
	GATE_IFR4(CLK_IFR_I2C5_ARBITER, "ifr_i2c5a", "i2c_sel", 19),
	GATE_IFR4(CLK_IFR_I2C5_IMM, "ifr_i2c5_imm", "i2c_sel", 20),
	GATE_IFR4(CLK_IFR_I2C1_ARBITER, "ifr_i2c1a", "i2c_sel", 21),
	GATE_IFR4(CLK_IFR_I2C1_IMM, "ifr_i2c1_imm", "i2c_sel", 22),
	GATE_IFR4(CLK_IFR_I2C2_ARBITER, "ifr_i2c2a", "i2c_sel", 23),
	GATE_IFR4(CLK_IFR_I2C2_IMM, "ifr_i2c2_imm", "i2c_sel", 24),
	GATE_IFR4(CLK_IFR_SPI4, "ifr_spi4", "spi_sel", 25),
	GATE_IFR4(CLK_IFR_SPI5, "ifr_spi5", "spi_sel", 26),
	GATE_IFR4(CLK_IFR_CQ_DMA, "ifr_cq_dma", "axi_sel", 27),
	GATE_IFR4(CLK_IFR_FAES_FDE, "ifr_faes_fde_ck", "aes_fde_sel", 29),
	/* INFRA mode 3 */
	GATE_IFR5(CLK_IFR_MSDC0_SELF, "ifr_msdc0sf", "msdc50_0_sel", 0),
	GATE_IFR5(CLK_IFR_MSDC1_SELF, "ifr_msdc1sf", "msdc50_0_sel", 1),
	GATE_IFR5(CLK_IFR_I2C6, "ifr_i2c6", "i2c_sel", 6),
	GATE_IFR5(CLK_IFR_AP_MSDC0, "ifr_ap_msdc0", "msdc50_0_sel", 7),
	GATE_IFR5(CLK_IFR_MD_MSDC0, "ifr_md_msdc0", "msdc50_0_sel", 8),
	GATE_IFR5(CLK_IFR_MSDC0_SRC, "ifr_msdc0_clk", "msdc50_0_sel", 9),
	GATE_IFR5(CLK_IFR_MSDC1_SRC, "ifr_msdc1_clk", "msdc30_1_sel", 10),
	GATE_IFR5(CLK_IFR_MCU_PM_BCLK, "ifr_mcu_pm_bclk", "axi_sel", 17),
	GATE_IFR5(CLK_IFR_CCIF2_AP, "ifr_ccif2_ap", "axi_sel", 18),
	GATE_IFR5(CLK_IFR_CCIF2_MD, "ifr_ccif2_md", "axi_sel", 19),
	GATE_IFR5(CLK_IFR_CCIF3_AP, "ifr_ccif3_ap", "axi_sel", 20),
	GATE_IFR5(CLK_IFR_CCIF3_MD, "ifr_ccif3_md", "axi_sel", 21),
};

static const struct mtk_clk_desc infra_desc = {
	.clks = infra_clks,
	.num_clks = ARRAY_SIZE(infra_clks),
};

static const struct of_device_id of_match_clk_mt6765_infracfg[] = {
	{ .compatible = "mediatek,mt6765-infracfg", .data = &infra_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6765_infracfg);

static struct platform_driver clk_mt6765_infracfg_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6765-infracfg",
		.of_match_table = of_match_clk_mt6765_infracfg,
	},
};

module_platform_driver(clk_mt6765_infracfg_drv);

MODULE_DESCRIPTION("MediaTek MT6765 infracfg clocks driver");
MODULE_LICENSE("GPL");