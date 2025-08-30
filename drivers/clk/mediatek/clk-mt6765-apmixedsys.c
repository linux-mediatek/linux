// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Copyright (c) 2025 Arseniy Velikanov <me@adomerle.pw>
 */
#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-pll.h"

#include <dt-bindings/clock/mt6765-clk.h>

#define MT6765_PLL_FMAX		(3800UL * MHZ)
#define MT6765_PLL_FMIN		(1500UL * MHZ)

/* additional CCF control for mipi26M race condition(disp/camera) */
static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x14,
	.sta_ofs = 0x14,
};

#define GATE_APMIXED(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &apmixed_cg_regs, _shift, &mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate apmixed_clks[] = {
	/* AUDIO0 */
	GATE_APMIXED(CLK_APMIXED_SSUSB26M, "apmixed_ssusb26m", "clk26m",
		     4),
	GATE_APMIXED(CLK_APMIXED_APPLL26M, "apmixed_appll26m", "clk26m",
		     5),
	GATE_APMIXED(CLK_APMIXED_MIPIC0_26M, "apmixed_mipic026m", "clk26m",
		     6),
	GATE_APMIXED(CLK_APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "clk26m",
		     7),
	GATE_APMIXED(CLK_APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "clk26m",
		     8),
	GATE_APMIXED(CLK_APMIXED_UFS26M, "apmixed_ufs26m", "clk26m",
		     9),
	GATE_APMIXED(CLK_APMIXED_MIPIC1_26M, "apmixed_mipic126m", "clk26m",
		     11),
	GATE_APMIXED(CLK_APMIXED_MEMPLL26M, "apmixed_mempll26m", "clk26m",
		     13),
	GATE_APMIXED(CLK_APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m",
		     "clk26m", 14),
	GATE_APMIXED(CLK_APMIXED_MIPID0_26M, "apmixed_mipid026m", "clk26m",
		     16),
};

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
		_pcwibits, _pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,\
		_tuner_en_bit, _pcw_reg, _pcw_shift, _div_table) {\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = BIT(23),				\
		.fmax = MT6765_PLL_FMAX,				\
		.fmin = MT6765_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = _pcwibits,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pcwibits, _pd_reg, _pd_shift, _tuner_reg,	\
			_tuner_en_reg, _tuner_en_bit, _pcw_reg,	\
			_pcw_shift)	\
		PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_pcwbits, _pcwibits, _pd_reg, _pd_shift,	\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, NULL)	\

static const struct mtk_pll_data plls[] = {
	PLL(CLK_APMIXED_ARMPLL_L, "armpll_l", 0x021C, 0x0228, 0,
	    PLL_AO, 22, 8, 0x0220, 24, 0, 0, 0, 0x0220, 0),
	PLL(CLK_APMIXED_ARMPLL, "armpll", 0x020C, 0x0218, 0,
	    PLL_AO, 22, 8, 0x0210, 24, 0, 0, 0, 0x0210, 0),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", 0x022C, 0x0238, 0,
	    PLL_AO, 22, 8, 0x0230, 24, 0, 0, 0, 0x0230, 0),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x023C, 0x0248, 0,
	    (HAVE_RST_BAR | PLL_AO), 22, 8, 0x0240, 24, 0, 0, 0, 0x0240,
	    0),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x024C, 0x0258, 0,
	    0, 22, 8, 0x0250, 24, 0, 0, 0, 0x0250, 0),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x025C, 0x0268, 0,
	    0, 22, 8, 0x0260, 24, 0, 0, 0, 0x0260, 0),
	PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", 0x026C, 0x0278, 0,
	    HAVE_RST_BAR, 22, 8, 0x0270, 24, 0, 0, 0, 0x0270, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x027C, 0x0288, 0,
	    0, 22, 8, 0x0280, 24, 0, 0, 0, 0x0280, 0),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x028C, 0x029C, 0,
	    0, 32, 8, 0x0290, 24, 0x0040, 0x000C, 0, 0x0294, 0),
	PLL(CLK_APMIXED_MPLL, "mpll", 0x02A0, 0x02AC, 0,
	    PLL_AO, 22, 8, 0x02A4, 24, 0, 0, 0, 0x02A4, 0),
};

static const struct of_device_id of_match_clk_mt6765_apmixed[] = {
	{ .compatible = "mediatek,mt6765-apmixedsys" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, of_match_clk_mt6789_apmixed);

static int clk_mt6765_apmixed_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int r;

	clk_data = mtk_alloc_clk_data(ARRAY_SIZE(plls));
	if (!clk_data)
		return -ENOMEM;

	r = mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
	if (r)
		goto free_apmixed_data;

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (r)
		goto unregister_plls;

	return r;

unregister_plls:
	mtk_clk_unregister_plls(plls, ARRAY_SIZE(plls), clk_data);
free_apmixed_data:
	mtk_free_clk_data(clk_data);
	return r;
}

static void clk_mt6765_apmixed_remove(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data = platform_get_drvdata(pdev);

	mtk_clk_unregister_plls(plls, ARRAY_SIZE(plls), clk_data);
	mtk_free_clk_data(clk_data);
}

static struct platform_driver clk_mt6765_apmixed_drv = {
	.probe = clk_mt6765_apmixed_probe,
	.remove = clk_mt6765_apmixed_remove,
	.driver = {
		.name = "clk-mt6765-apmixed",
		.of_match_table = of_match_clk_mt6765_apmixed,
	},
};

module_platform_driver(clk_mt6765_apmixed_drv);

MODULE_DESCRIPTION("MediaTek MT6765 apmixedsys clocks driver");
MODULE_LICENSE("GPL");