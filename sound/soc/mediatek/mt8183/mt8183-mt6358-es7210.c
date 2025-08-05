// SPDX-License-Identifier: GPL-2.0
//
// MT8183-MT6358-ES7210 ALSA SoC machine driver
//
// Copyright (c) 2025 Arseniy Velikanov <me@adomerle.pw>

#include <linux/module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <sound/jack.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../common/mtk-soc-card.h"
#include "../common/mtk-soundcard-driver.h"
#include "../common/mtk-afe-platform-driver.h"
#include "mt8183-afe-common.h"

static int mt8183_mt6358_i2s_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	unsigned int rate = params_rate(params);
	unsigned int mclk_fs_ratio = 128;
	unsigned int mclk_fs = rate * mclk_fs_ratio;

	return snd_soc_dai_set_sysclk(snd_soc_rtd_to_cpu(rtd, 0),
				      0, mclk_fs, SND_SOC_CLOCK_OUT);
}

static const struct snd_soc_ops mt8183_mt6358_i2s_ops = {
	.hw_params = mt8183_mt6358_i2s_hw_params,
};

static int mt8183_i2s_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	dev_dbg(rtd->dev, "%s(), fix format to S32_LE\n", __func__);

	/* fix BE i2s format to S32_LE, clean param mask first */
	snd_mask_reset_range(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			     0, (__force unsigned int)SNDRV_PCM_FORMAT_LAST);

	params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
	return 0;
}

static int mt8183_mt6358_startup(struct snd_pcm_substream *substream)
{
	static const unsigned int rates[] = {
		48000,
	};
	static const struct snd_pcm_hw_constraint_list constraints_rates = {
		.count = ARRAY_SIZE(rates),
		.list  = rates,
		.mask = 0,
	};
	static const unsigned int channels[] = {
		2,
	};
	static const struct snd_pcm_hw_constraint_list constraints_channels = {
		.count = ARRAY_SIZE(channels),
		.list = channels,
		.mask = 0,
	};

	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_pcm_hw_constraint_list(runtime, 0,
				   SNDRV_PCM_HW_PARAM_RATE, &constraints_rates);
	runtime->hw.channels_max = 2;
	snd_pcm_hw_constraint_list(runtime, 0,
				   SNDRV_PCM_HW_PARAM_CHANNELS,
				   &constraints_channels);

	runtime->hw.formats = SNDRV_PCM_FMTBIT_S16_LE;
	snd_pcm_hw_constraint_msbits(runtime, 0, 16, 16);

	return 0;
}

static const struct snd_soc_ops mt8183_mt6358_ops = {
	.startup = mt8183_mt6358_startup,
};

/* FE */
SND_SOC_DAILINK_DEFS(playback1,
	DAILINK_COMP_ARRAY(COMP_CPU("DL1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(playback2,
	DAILINK_COMP_ARRAY(COMP_CPU("DL2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

/* BE */
SND_SOC_DAILINK_DEFS(primary_codec,
	DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
	DAILINK_COMP_ARRAY(COMP_CODEC("mt6358-sound", "mt6358-snd-codec-aif1")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(pcm1,
	DAILINK_COMP_ARRAY(COMP_CPU("PCM 1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(pcm2,
	DAILINK_COMP_ARRAY(COMP_CPU("PCM 2")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s0,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S0")),
	DAILINK_COMP_ARRAY(COMP_CODEC("bt-sco", "bt-sco-pcm-wb")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

SND_SOC_DAILINK_DEFS(i2s1,
	DAILINK_COMP_ARRAY(COMP_CPU("I2S1")),
	DAILINK_COMP_ARRAY(COMP_DUMMY()),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link mt8183_mt6358_es7210_dai_links[] = {
	/* FE */
	{
		.name = "Playback_1",
		.stream_name = "Playback_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.playback_only = 1,
		.ops = &mt8183_mt6358_ops,
		SND_SOC_DAILINK_REG(playback1),
	},
	{
		.name = "Playback_2",
		.stream_name = "Playback_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		.playback_only = 1,
		SND_SOC_DAILINK_REG(playback2),
	},

	/* BE */
	{
		.name = "Primary Codec",
		.no_pcm = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(primary_codec),
	},
	{
		.name = "PCM 1",
		.no_pcm = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pcm1),
	},
	{
		.name = "PCM 2",
		.no_pcm = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(pcm2),
	},
	{
		.name = "I2S1",
		.no_pcm = 1,
		.playback_only = 1,
		.ignore_suspend = 1,
		.be_hw_params_fixup = mt8183_i2s_hw_params_fixup,
		.ops = &mt8183_mt6358_i2s_ops,
		SND_SOC_DAILINK_REG(i2s1),
	},
};

static struct snd_soc_card mt8183_mt6358_es7210_soc_card = {
	.name = "mt8183-mt6358-es7210",
	.owner = THIS_MODULE,
	.dai_link = mt8183_mt6358_es7210_dai_links,
	.num_links = ARRAY_SIZE(mt8183_mt6358_es7210_dai_links),
};

static int mt8183_mt6358_es7210_dev_probe(struct mtk_soc_card_data *soc_card_data, bool legacy)
{
	struct mtk_platform_card_data *card_data = soc_card_data->card_data;
	struct snd_soc_card *card = card_data->card;
	struct device *dev = card->dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	int ret;

	card->dev = dev;
	ret = parse_dai_link_info(card);
	if (ret)
		goto err;

	snd_soc_card_set_drvdata(card, soc_card_data);
	
	pinctrl = devm_pinctrl_get(card->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		return dev_err_probe(card->dev, ret,
				    "Failed to get pinctrl\n");
	}

	pins_default = pinctrl_lookup_state(pinctrl, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		dev_err_probe(dev, ret, "Failed to get default pin state\n");
	}
	ret = pinctrl_select_state(pinctrl, pins_default);
	if (ret) {
		dev_err_probe(card->dev, ret,
				  "Failed to apply pin settings\n");
		return ret;
	}

	return 0;

err:
	clean_card_reference(card);
	return ret;
}

static const struct mtk_soundcard_pdata mt8183_mt6358_es7210_card = {
	.card_name = "mt8183-mt6358-es7210",
	.card_data = &(struct mtk_platform_card_data) {
		.card = &mt8183_mt6358_es7210_soc_card,
	},
	.soc_probe = mt8183_mt6358_es7210_dev_probe
};

static const struct of_device_id mt8183_mt6358_es7120_dt_match[] = {
	{ .compatible = "mediatek,mt8183-mt6358-es7210", .data = &mt8183_mt6358_es7210_card },
	{ /* sentinel */ }
};

static struct platform_driver mt8183_mt6358_es7210_driver = {
	.driver = {
		   .name = "mt8183_mt6358_es7210",
		   .of_match_table = mt8183_mt6358_es7120_dt_match,
		   .pm = &snd_soc_pm_ops,
	},
	.probe = mtk_soundcard_common_probe,
};

module_platform_driver(mt8183_mt6358_es7210_driver);

/* Module information */
MODULE_DESCRIPTION("MT8183-MT6358-ES7210 ALSA SoC machine driver");
MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mt8183_mt6358_es7210 soc card");
