// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Copyright (c) 2025 Arseniy Velikanov <me@adomerle.pw>
 */
#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"

#include <dt-bindings/clock/mt6765-clk.h>

static DEFINE_SPINLOCK(mt6765_topck_lock);

/* clk cfg update */
#define CLK_CFG_0		0x40
#define CLK_CFG_0_SET		0x44
#define CLK_CFG_0_CLR		0x48
#define CLK_CFG_1		0x50
#define CLK_CFG_1_SET		0x54
#define CLK_CFG_1_CLR		0x58
#define CLK_CFG_2		0x60
#define CLK_CFG_2_SET		0x64
#define CLK_CFG_2_CLR		0x68
#define CLK_CFG_3		0x70
#define CLK_CFG_3_SET		0x74
#define CLK_CFG_3_CLR		0x78
#define CLK_CFG_4		0x80
#define CLK_CFG_4_SET		0x84
#define CLK_CFG_4_CLR		0x88
#define CLK_CFG_5		0x90
#define CLK_CFG_5_SET		0x94
#define CLK_CFG_5_CLR		0x98
#define CLK_CFG_6		0xa0
#define CLK_CFG_6_SET		0xa4
#define CLK_CFG_6_CLR		0xa8
#define CLK_CFG_7		0xb0
#define CLK_CFG_7_SET		0xb4
#define CLK_CFG_7_CLR		0xb8
#define CLK_CFG_8		0xc0
#define CLK_CFG_8_SET		0xc4
#define CLK_CFG_8_CLR		0xc8
#define CLK_CFG_9		0xd0
#define CLK_CFG_9_SET		0xd4
#define CLK_CFG_9_CLR		0xd8
#define CLK_CFG_10		0xe0
#define CLK_CFG_10_SET		0xe4
#define CLK_CFG_10_CLR		0xe8
#define CLK_CFG_UPDATE		0x004

static const struct mtk_fixed_clk fixed_clks[] = {
	FIXED_CLK(CLK_TOP_DMPLL, "dmpll_ck", NULL, 466000000),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_SYSPLL_D2, "syspll_d2", "mainpll", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "syspll_d2", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "syspll_d2", 1, 4),
	FACTOR(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "syspll_d2", 1, 8),
	FACTOR(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "syspll_d2", 1, 16),
	FACTOR(CLK_TOP_SYSPLL_D3, "syspll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "syspll_d3", 1, 2),
	FACTOR(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "syspll_d3", 1, 4),
	FACTOR(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "syspll_d3", 1, 8),
	FACTOR(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "syspll_d5", 1, 2),
	FACTOR(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "syspll_d5", 1, 4),
	FACTOR(CLK_TOP_SYSPLL_D7, "syspll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "syspll_d7", 1, 2),
	FACTOR(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "syspll_d7", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL, "univpll", "univ2pll", 1, 2),
	FACTOR(CLK_TOP_USB20_192M, "usb20_192m_ck", "univpll", 2, 13),
	FACTOR(CLK_TOP_USB20_192M_D4, "usb20_192m_d4", "usb20_192m_ck", 1, 4),
	FACTOR(CLK_TOP_USB20_192M_D8, "usb20_192m_d8", "usb20_192m_ck", 1, 8),
	FACTOR(CLK_TOP_USB20_192M_D16, "usb20_192m_d16", "usb20_192m_ck", 1, 16),
	FACTOR(CLK_TOP_USB20_192M_D32, "usb20_192m_d32", "usb20_192m_ck", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll_d2", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll_d2", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll_d3", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll_d3", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll_d3", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL2_D32, "univpll2_d32", "univpll_d3", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll_d5", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll_d5", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1, 2),
	FACTOR(CLK_TOP_DA_MPLL_104M_DIV, "mpll_104m_div", "mpll", 1, 2),
	FACTOR(CLK_TOP_DA_MPLL_52M_DIV, "mpll_52m_div", "mpll", 1, 4),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "apll1", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1_D2, "ulposc1_d2", "ulposc1", 1, 2),
	FACTOR(CLK_TOP_ULPOSC1_D4, "ulposc1_d4", "ulposc1", 1, 4),
	FACTOR(CLK_TOP_ULPOSC1_D8, "ulposc1_d8", "ulposc1", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1_D16, "ulposc1_d16", "ulposc1", 1, 16),
	FACTOR(CLK_TOP_ULPOSC1_D32, "ulposc1_d32", "ulposc1", 1, 32),
};

static const char * const axi_parents[] = {
	"clk26m",
	"syspll_d7",
	"syspll1_d4",
	"syspll3_d2"
};

static const char * const mem_parents[] = {
	"clk26m",
	"dmpll_ck",
	"apll1"
};

static const char * const mm_parents[] = {
	"clk26m",
	"mmpll",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll_d5",
	"univpll1_d2",
	"mmpll_d2"
};

static const char * const scp_parents[] = {
	"clk26m",
	"syspll4_d2",
	"univpll2_d2",
	"syspll1_d2",
	"univpll1_d2",
	"syspll_d3",
	"univpll_d3"
};

static const char * const mfg_parents[] = {
	"clk26m",
	"mfgpll",
	"syspll_d3",
	"univpll_d3"
};

static const char * const atb_parents[] = {
	"clk26m",
	"syspll1_d4",
	"syspll1_d2"
};

static const char * const camtg_parents[] = {
	"clk26m",
	"usb20_192m_d8",
	"univpll2_d8",
	"usb20_192m_d4",
	"univpll2_d32",
	"usb20_192m_d16",
	"usb20_192m_d32"
};

static const char * const uart_parents[] = {
	"clk26m",
	"univpll2_d8"
};

static const char * const spi_parents[] = {
	"clk26m",
	"syspll3_d2",
	"syspll4_d2",
	"syspll2_d4"
};

static const char * const msdc5hclk_parents[] = {
	"clk26m",
	"syspll1_d2",
	"univpll1_d4",
	"syspll2_d2"
};

static const char * const msdc50_0_parents[] = {
	"clk26m",
	"msdcpll",
	"syspll2_d2",
	"syspll4_d2",
	"univpll1_d2",
	"syspll1_d2",
	"univpll_d5",
	"univpll1_d4"
};

static const char * const msdc30_1_parents[] = {
	"clk26m",
	"msdcpll_d2",
	"univpll2_d2",
	"syspll2_d2",
	"syspll1_d4",
	"univpll1_d4",
	"usb20_192m_d4",
	"syspll2_d4"
};

static const char * const audio_parents[] = {
	"clk26m",
	"syspll3_d4",
	"syspll4_d4",
	"syspll1_d16"
};

static const char * const aud_intbus_parents[] = {
	"clk26m",
	"syspll1_d4",
	"syspll4_d2"
};

static const char * const aud_1_parents[] = {
	"clk26m",
	"apll1"
};

static const char * const aud_engen1_parents[] = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const disp_pwm_parents[] = {
	"clk26m",
	"univpll2_d4",
	"ulposc1_d2",
	"ulposc1_d8"
};

static const char * const sspm_parents[] = {
	"clk26m",
	"syspll1_d2",
	"syspll_d3"
};

static const char * const dxcc_parents[] = {
	"clk26m",
	"syspll1_d2",
	"syspll1_d4",
	"syspll1_d8"
};

static const char * const usb_top_parents[] = {
	"clk26m",
	"univpll3_d4"
};

static const char * const spm_parents[] = {
	"clk26m",
	"syspll1_d8"
};

static const char * const i2c_parents[] = {
	"clk26m",
	"univpll3_d4",
	"univpll3_d2",
	"syspll1_d8",
	"syspll2_d8"
};

static const char * const pwm_parents[] = {
	"clk26m",
	"univpll3_d4",
	"syspll1_d8"
};

static const char * const seninf_parents[] = {
	"clk26m",
	"univpll1_d4",
	"univpll1_d2",
	"univpll2_d2"
};

static const char * const aes_fde_parents[] = {
	"clk26m",
	"msdcpll",
	"univpll_d3",
	"univpll2_d2",
	"univpll1_d2",
	"syspll1_d2"
};

static const char * const ulposc_parents[] = {
	"clk26m",
	"ulposc1_d4",
	"ulposc1_d8",
	"ulposc1_d16",
	"ulposc1_d32"
};

static const char * const camtm_parents[] = {
	"clk26m",
	"univpll1_d4",
	"univpll1_d2",
	"univpll2_d2"
};

static const struct mtk_mux top_muxes[] = {
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel", axi_parents,
			CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
			0, 2, 7, CLK_CFG_UPDATE, 0,
			CLK_IS_CRITICAL | CLK_SET_RATE_PARENT),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_MEM_SEL, "mem_sel", mem_parents,
			CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
			8, 2, 15, CLK_CFG_UPDATE, 1,
			CLK_IS_CRITICAL | CLK_SET_RATE_PARENT),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel", mm_parents, CLK_CFG_0,
			CLK_CFG_0_SET, CLK_CFG_0_CLR, 16, 3, 23,
			CLK_CFG_UPDATE, 2),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCP_SEL, "scp_sel", scp_parents, CLK_CFG_0,
			CLK_CFG_0_SET, CLK_CFG_0_CLR, 24, 3, 31,
			CLK_CFG_UPDATE, 3),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents, CLK_CFG_1,
			CLK_CFG_1_SET, CLK_CFG_1_CLR, 0, 2, 7,
			CLK_CFG_UPDATE, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ATB_SEL, "atb_sel", atb_parents, CLK_CFG_1,
			CLK_CFG_1_SET, CLK_CFG_1_CLR, 8, 2, 15,
			CLK_CFG_UPDATE, 5),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel",
			camtg_parents, CLK_CFG_1, CLK_CFG_1_SET,
			CLK_CFG_1_CLR, 16, 3, 23, CLK_CFG_UPDATE, 6),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG1_SEL, "camtg1_sel", camtg_parents,
			CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
			24, 3, 31, CLK_CFG_UPDATE, 7),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL, "camtg2_sel",
			camtg_parents, CLK_CFG_2, CLK_CFG_2_SET,
			CLK_CFG_2_CLR, 0, 3, 7, CLK_CFG_UPDATE, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL, "camtg3_sel", camtg_parents,
			CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
			8, 3, 15, CLK_CFG_UPDATE, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel", uart_parents,
			CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, 16, 1, 23,
			CLK_CFG_UPDATE, 10),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel", spi_parents, CLK_CFG_2,
			CLK_CFG_2_SET, CLK_CFG_2_CLR, 24, 2, 31,
			CLK_CFG_UPDATE, 11),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_MSDC50_0_HCLK_SEL, "msdc5hclk",
			msdc5hclk_parents, CLK_CFG_3, CLK_CFG_3_SET,
			CLK_CFG_3_CLR, 0, 2, 7, CLK_CFG_UPDATE, 12, 0),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
			msdc50_0_parents, CLK_CFG_3, CLK_CFG_3_SET,
			CLK_CFG_3_CLR, 8, 3, 15, CLK_CFG_UPDATE, 13, 0),
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
			msdc30_1_parents, CLK_CFG_3, CLK_CFG_3_SET,
			CLK_CFG_3_CLR, 16, 3, 23, CLK_CFG_UPDATE, 14, 0),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel", audio_parents,
			CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
			24, 2, 31, CLK_CFG_UPDATE, 15),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
			aud_intbus_parents, CLK_CFG_4, CLK_CFG_4_SET,
			CLK_CFG_4_CLR, 0, 2, 7, CLK_CFG_UPDATE, 16),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel", aud_1_parents,
			CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
			8, 1, 15, CLK_CFG_UPDATE, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
			aud_engen1_parents, CLK_CFG_4, CLK_CFG_4_SET,
			CLK_CFG_4_CLR, 16, 2, 23, CLK_CFG_UPDATE, 18),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
			disp_pwm_parents, CLK_CFG_4, CLK_CFG_4_SET,
			CLK_CFG_4_CLR, 24, 2, 31, CLK_CFG_UPDATE, 19),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSPM_SEL, "sspm_sel", sspm_parents,
			CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, 0, 2, 7,
			CLK_CFG_UPDATE, 20),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL, "dxcc_sel", dxcc_parents,
			CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, 8, 2, 15,
			CLK_CFG_UPDATE, 21),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL, "usb_top_sel",
			usb_top_parents, CLK_CFG_5, CLK_CFG_5_SET,
			CLK_CFG_5_CLR, 16, 1, 23, CLK_CFG_UPDATE, 22),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPM_SEL, "spm_sel", spm_parents, CLK_CFG_5,
			CLK_CFG_5_SET, CLK_CFG_5_CLR, 24, 1, 31,
			CLK_CFG_UPDATE, 23),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents, CLK_CFG_6,
			CLK_CFG_6_SET, CLK_CFG_6_CLR, 0, 3, 7, CLK_CFG_UPDATE,
			24),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents, CLK_CFG_6,
			CLK_CFG_6_SET, CLK_CFG_6_CLR, 8, 2, 15, CLK_CFG_UPDATE,
			25),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL, "seninf_sel", seninf_parents,
			CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, 16, 2, 23,
			CLK_CFG_UPDATE, 26),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_FDE_SEL, "aes_fde_sel",
			aes_fde_parents, CLK_CFG_6, CLK_CFG_6_SET,
			CLK_CFG_6_CLR, 24, 3, 31, CLK_CFG_UPDATE, 27),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD_FLAGS(CLK_TOP_PWRAP_ULPOSC_SEL, "ulposc_sel",
			ulposc_parents, CLK_CFG_7, CLK_CFG_7_SET,
			CLK_CFG_7_CLR, 0, 3, 7, CLK_CFG_UPDATE, 28,
			CLK_IS_CRITICAL | CLK_SET_RATE_PARENT),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL, "camtm_sel", camtm_parents,
			CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 8, 2, 15,
			CLK_CFG_UPDATE, 29),
};

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x104,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs top2_cg_regs = {
	.set_ofs = 0x320,
	.clr_ofs = 0x320,
	.sta_ofs = 0x320,
};

#define GATE_TOP0(_id, _name, _parent, _shift)				\
	GATE_MTK(_id, _name, _parent, &top0_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr)

#define GATE_TOP1(_id, _name, _parent, _shift)				\
	GATE_MTK(_id, _name, _parent, &top1_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

#define GATE_TOP2(_id, _name, _parent, _shift)				\
	GATE_MTK(_id, _name, _parent, &top2_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr)

static const struct mtk_gate top_clks[] = {
	/* TOP0 */
	GATE_TOP0(CLK_TOP_MD_32K, "md_32k", "clk32k", 8),
	GATE_TOP0(CLK_TOP_MD_26M, "md_26m", "clk26m", 9),
	GATE_TOP0(CLK_TOP_MD2_32K, "md2_32k", "clk32k", 10),
	GATE_TOP0(CLK_TOP_MD2_26M, "md2_26m", "clk26m", 11),
	/* TOP1 */
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL0_EN, "arm_div_pll0_en", "syspll_d2", 3),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL1_EN, "arm_div_pll1_en", "mainpll", 4),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL2_EN, "arm_div_pll2_en", "univpll_d2", 5),
	GATE_TOP1(CLK_TOP_FMEM_OCC_DRC_EN, "drc_en", "univpll2_d2", 6),
	GATE_TOP1(CLK_TOP_USB20_48M_EN, "usb20_48m_en", "usb20_192m_d4", 8),
	GATE_TOP1(CLK_TOP_UNIVPLL_48M_EN, "univpll_48m_en", "usb20_192m_d4", 9),
	GATE_TOP1(CLK_TOP_F_UFS_MP_SAP_CFG_EN, "ufs_sap", "clk26m", 12),
	GATE_TOP1(CLK_TOP_F_BIST2FPC_EN, "bist2fpc", "univpll2_d2", 16),
	/* TOP2 */
	GATE_TOP2(CLK_TOP_APLL12_DIV0, "apll12_div0", "aud_1_sel", 2),
	GATE_TOP2(CLK_TOP_APLL12_DIV1, "apll12_div1", "aud_1_sel", 3),
	GATE_TOP2(CLK_TOP_APLL12_DIV2, "apll12_div2", "aud_1_sel", 4),
	GATE_TOP2(CLK_TOP_APLL12_DIV3, "apll12_div3", "aud_1_sel", 5),
};

static const struct mtk_clk_desc topck_desc = {
	.fixed_clks = fixed_clks,
	.num_fixed_clks = ARRAY_SIZE(fixed_clks),
	.factor_clks = top_divs,
	.num_factor_clks = ARRAY_SIZE(top_divs),
	.mux_clks = top_muxes,
	.num_mux_clks = ARRAY_SIZE(top_muxes),
	.clks = top_clks,
	.num_clks = ARRAY_SIZE(top_clks),
	.clk_lock = &mt6765_topck_lock,
};

static const struct of_device_id of_match_clk_mt6765_topckgen[] = {
	{ .compatible = "mediatek,mt6765-topckgen", .data = &topck_desc },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6765_topckgen);

static struct platform_driver clk_mt6765_topckgen_drv = {
	.probe = mtk_clk_simple_probe,
	.remove = mtk_clk_simple_remove,
	.driver = {
		.name = "clk-mt6765-topckgen",
		.of_match_table = of_match_clk_mt6765_topckgen,
	},
};
module_platform_driver(clk_mt6765_topckgen_drv);

MODULE_DESCRIPTION("MediaTek MT6765 top clock generators driver");
MODULE_LICENSE("GPL");