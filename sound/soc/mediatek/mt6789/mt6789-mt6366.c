// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6789-mt6358.c  --  mt6789 mt6358 ALSA SoC machine driver
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Yujie Xiao <yujie.xiao@mediatek.com>
 */

#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include "../common/mtk-soc-card.h"
#include "../common/mtk-soundcard-driver.h"

static int mt6789_mt6366_startup(struct snd_pcm_substream *substream)
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

static const struct snd_soc_ops mt6789_mt6366_ops = {
	.startup = mt6789_mt6366_startup,
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
SND_SOC_DAILINK_DEFS(adda,
	DAILINK_COMP_ARRAY(COMP_CPU("ADDA")),
	DAILINK_COMP_ARRAY(COMP_CODEC("mt6358-sound",
				      "mt6358-snd-codec-aif1")),
	DAILINK_COMP_ARRAY(COMP_EMPTY()));

static struct snd_soc_dai_link mt6789_mt6366_dai_links[] = {
	/* FE */
	{
		.name = "Playback_1",
		.stream_name = "Playback_1",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		SND_SOC_DAILINK_REG(playback1),
	},
	{
		.name = "Playback_2",
		.stream_name = "Playback_2",
		.trigger = {SND_SOC_DPCM_TRIGGER_PRE,
			    SND_SOC_DPCM_TRIGGER_PRE},
		.dynamic = 1,
		SND_SOC_DAILINK_REG(playback2),
	},
	/* BE */
	{
		.name = "Primary Codec",
		.no_pcm = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adda),
	},
};

static struct snd_soc_card mt6789_mt6366_soc_card = {
	.name = "mt6789-mt6366",
	.owner = THIS_MODULE,
	.dai_link = mt6789_mt6366_dai_links,
	.num_links = ARRAY_SIZE(mt6789_mt6366_dai_links),
};

static int mt6789_mt6366_dev_probe(struct mtk_soc_card_data *soc_card_data, bool legacy)
{
	struct mtk_platform_card_data *card_data = soc_card_data->card_data;
	struct snd_soc_card *card = card_data->card;
	struct device *dev = card->dev;
	int ret;

	card->dev = dev;
	ret = parse_dai_link_info(card);
	if (ret)
		goto err;

	snd_soc_card_set_drvdata(card, soc_card_data);

	return 0;

err:
	clean_card_reference(card);
	return ret;
}

static const struct mtk_soundcard_pdata mt6789_mt6366_card = {
	.card_name = "mt6789_mt6366",
	.card_data = &(struct mtk_platform_card_data) {
		.card = &mt6789_mt6366_soc_card,
	},
	.soc_probe = mt6789_mt6366_dev_probe
};

static const struct of_device_id mt6789_mt6366_dt_match[] = {
	{ .compatible = "mediatek,mt6789-mt6366", .data = &mt6789_mt6366_card },
	{ /* sentinel */ }
};

static struct platform_driver mt6789_mt6366_driver = {
	.driver = {
		   .name = "mt6789_mt6366",
		   .of_match_table = mt6789_mt6366_dt_match,
		   .pm = &snd_soc_pm_ops,
	},
	.probe = mtk_soundcard_common_probe,
};

module_platform_driver(mt6789_mt6366_driver);

/* Module information */
MODULE_DESCRIPTION("MT6789-MT6366 ALSA SoC machine driver");
MODULE_AUTHOR("Arseniy Velikanov <me@adomerle.pw>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mt6789_mt6366 soc card");