/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2560-codec.c
**
** Description:
**     ALSA SoC driver for Texas Instruments TAS2560 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2560_CODEC

#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "tas2560.h"
#include "tas2560-core.h"
#include <dsp/smart_amp.h>
#include <dsp/q6afe-v2.h>
#define TAS2560_MDELAY 0xFFFFFFFE
#define KCONTROL_CODEC

#define SMARTAMP_SPEAKER_CALIBDATA_FILE     "/mnt/vendor/persist-lg/audio/smartamp_calib.bin"
#define MAX_CHANNEL_COUNT		1
#define MAX_STRING		200

static bool rdc_cal_done;

static unsigned int tas2560_codec_read(struct snd_soc_codec *codec,  unsigned int reg)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	unsigned int value = 0;
	int ret;

	ret = pTAS2560->read(pTAS2560, reg, &value);
	if (ret >= 0)
		return value;
	else
		return ret;
}

static int tas2560_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	return pTAS2560->write(pTAS2560, reg, value);
}

static int tas2560_codec_suspend(struct snd_soc_codec *codec)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2560->codec_lock);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	pTAS2560->runtime_suspend(pTAS2560);

	mutex_unlock(&pTAS2560->codec_lock);
	return ret;
}

static int tas2560_codec_resume(struct snd_soc_codec *codec)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	mutex_lock(&pTAS2560->codec_lock);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
	pTAS2560->runtime_resume(pTAS2560);

	mutex_unlock(&pTAS2560->codec_lock);
	return ret;
}

static int tas2560_AIF_post_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(pTAS2560->dev, "SND_SOC_DAPM_POST_PMU");
	break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(pTAS2560->dev, "SND_SOC_DAPM_POST_PMD");
	break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget tas2560_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN_E("ASI1", "ASI1 Playback", 0, SND_SOC_NOPM, 0, 0,
		tas2560_AIF_post_event, SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_OUT_DRV("ClassD", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("PLL", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route tas2560_audio_map[] = {
	{"DAC", NULL, "ASI1"},
	{"ClassD", NULL, "DAC"},
	{"OUT", NULL, "ClassD"},
	{"DAC", NULL, "PLL"},
};

static int tas2560_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static void tas2560_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);
}

static int tas2560_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&pTAS2560->codec_lock);
	dev_dbg(pTAS2560->dev, "%s, %d\n", __func__, mute);
	tas2560_enable(pTAS2560, !mute);
	mutex_unlock(&pTAS2560->codec_lock);
	return 0;
}

static int tas2560_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
			unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return ret;
}

static int tas2560_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static int tas2560_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static int tas2560_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(pTAS2560->dev, "%s, format=0x%x\n", __func__, fmt);

	return ret;
}

static struct snd_soc_dai_ops tas2560_dai_ops = {
	.startup = tas2560_startup,
	.shutdown = tas2560_shutdown,
	.digital_mute = tas2560_mute,
	.hw_params  = tas2560_hw_params,
	.prepare    = tas2560_prepare,
	.set_sysclk = tas2560_set_dai_sysclk,
	.set_fmt    = tas2560_set_dai_fmt,
};

#define TAS2560_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)
static struct snd_soc_dai_driver tas2560_dai_driver[] = {
	{
		.name = "tas2560 ASI1",
		.id = 0,
		.playback = {
			.stream_name    = "ASI1 Playback",
			.channels_min   = 2,
			.channels_max   = 2,
			.rates      = SNDRV_PCM_RATE_8000_192000,
			.formats    = TAS2560_FORMATS,
		},
		.capture = {
			.stream_name    = "ASI1 Capture",
			.channels_min   = 1,
			.channels_max   = 2,
			.rates      = SNDRV_PCM_RATE_8000_48000,
			.formats    = TAS2560_FORMATS,
		},
		.ops = &tas2560_dai_ops,
		.symmetric_rates = 1,
	},
};

static int tas2560_codec_probe(struct snd_soc_codec *codec)
{
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(pTAS2560->dev, "%s\n", __func__);

	return 0;
}

static int tas2560_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static int tas2560_get_load(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	pUcontrol->value.integer.value[0] = pTAS2560->mnLoad;

	return 0;
}

static int tas2560_set_load(struct snd_kcontrol *pKcontrol,
			struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	mutex_lock(&pTAS2560->codec_lock);
	pTAS2560->mnLoad = pUcontrol->value.integer.value[0];
	dev_dbg(pCodec->dev, "%s:load = 0x%x\n", __func__,
			pTAS2560->mnLoad);
	tas2560_setLoad(pTAS2560, pTAS2560->mnLoad);
	mutex_unlock(&pTAS2560->codec_lock);
	return 0;
}

static int tas2560_get_Sampling_Rate(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);

	pUcontrol->value.integer.value[0] = pTAS2560->mnSamplingRate;
	dev_dbg(pCodec->dev, "%s: %d\n", __func__,
			pTAS2560->mnSamplingRate);
	return 0;
}

static int tas2560_set_Sampling_Rate(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *pCodec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *pCodec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(pCodec);
	int sampleRate = pUcontrol->value.integer.value[0];

	mutex_lock(&pTAS2560->codec_lock);
	dev_dbg(pCodec->dev, "%s: %d\n", __func__, sampleRate);
	tas2560_set_SampleRate(pTAS2560, sampleRate);
	mutex_unlock(&pTAS2560->codec_lock);

	return 0;
}

static int tas2560_power_ctrl_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);

	pValue->value.integer.value[0] = pTAS2560->mbPowerUp;
	dev_dbg(codec->dev, "tas2560_power_ctrl_get = 0x%x\n",
					pTAS2560->mbPowerUp);

	return 0;
}

static int tas2560_power_ctrl_put(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
#ifdef KCONTROL_CODEC
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(pKcontrol);
#else
	struct snd_soc_codec *codec = snd_kcontrol_chip(pKcontrol);
#endif
	struct tas2560_priv *pTAS2560 = snd_soc_codec_get_drvdata(codec);
	int bPowerUp = pValue->value.integer.value[0];

	mutex_lock(&pTAS2560->codec_lock);
	tas2560_enable(pTAS2560, bPowerUp);
	mutex_unlock(&pTAS2560->codec_lock);

	return 0;
}

int smartamp_bypass = 0;

static const char *tas2560_smartamp_bypass_text[] = {
	"FALSE",
	"TRUE"
};

static const struct soc_enum tas2560_smartamp_bypass_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tas2560_smartamp_bypass_text), tas2560_smartamp_bypass_text),
};

static int tas2560_smartamp_bypass_set(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = pUcontrol->value.integer.value[0];

	if((user_data < 0) || (user_data > 1))   return ret;
	smartamp_bypass = user_data;
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_smartamp_bypass_text[user_data]);
	return ret;
}

static int tas2560_smartamp_bypass_get(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pUcontrol)
{
	int ret = 0;
	int user_data = smartamp_bypass;
	pUcontrol->value.integer.value[0] = user_data;
	pr_info("TI-SmartPA: %s: case %s", __func__, tas2560_smartamp_bypass_text[user_data]);
	return ret;
}

static int tas2560_fb_path_info(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
	int ret = 0;
	int enable = pValue->value.integer.value[0];
	if(enable)
	{
		pr_info("TI-SmartPA: %s: Setting Feedback Path Info for TAS\n",__func__);
		ret = afe_spk_prot_feed_back_cfg(TAS_TX_PORT_ID,TAS_RX_PORT_ID,1,0,1);
	}
	return ret;
}

/*Get Rdc value from the bin file*/
static int smartpa_get_calibrated_re(unsigned int channel_count_speaker, int32_t *Rdc_fix)
{
	struct file *file;
	char calib_data[MAX_STRING] = {0};
	char *val = NULL, *data = calib_data;
	int32_t i, ret;

	file = filp_open(SMARTAMP_SPEAKER_CALIBDATA_FILE, O_RDWR, 0);
	if (IS_ERR(file)) {
		pr_err("TI-SmartPA: %s: open calib failed\n", __func__);
		return -1;
	}

	ret = kernel_read(file, 0, calib_data, MAX_STRING);

	if((ret <= 0) || strlen(calib_data) <= 0)
	{
		pr_err("TI-SmartPA: %s: read calib failed\n", __func__);
		return -1;
	}

	for(i = 0; i < channel_count_speaker; i++)
	{
		val = strstr(data, ";");
		if(val)
		{
			*val = '\0';
			sscanf(data, "%d", &Rdc_fix[i]);
			data = val + 1;
			pr_info("TI-SmartPA: %s: Re[%d] = %d", __func__, i, Rdc_fix[i]);
		}
	}
	rdc_cal_done = true;

	return 0;
}

static int tas2560_set_parameters(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
	struct afe_smartamp_set_params_t port_config;
	struct soc_mixer_control *mc = (struct soc_mixer_control *)pKcontrol->private_value;
	int data = pValue->value.integer.value[0];
	int sz = sizeof(int);
	int cmd = mc->shift;
	int param_id = cmd;
	int module_id = AFE_SMARTAMP_MODULE;
	int ret = -1;

	if(smartamp_bypass)
	{
		pr_err("TI-SmartPA: %s: Set Param bypassed through master control", __func__);
		return 0;
	}
	switch(cmd)
	{
	   case CAPI_V2_TAS_TX_ENABLE :
	   	{
			pr_err("TI-SmartPA: %s: command_id = %x, data = %d\n",__func__,cmd,data);
			module_id = AFE_SMARTAMP_MODULE_TX;
			break;
		}
	    case CAPI_V2_TAS_RX_ENABLE :
	    case CAPI_V2_TAS_RX_CFG :
	   	{
			pr_info("TI-SmartPA: %s: command_id = %x, data = %d\n",__func__,cmd,data);
			break;
		}
	    case AFE_SA_SET_RE :
		{
			if (rdc_cal_done) {
				pr_debug("TI-SmartPA: %s: calibration already done\n",__func__);
				return 0;
			}
			if(smartpa_get_calibrated_re(1, &data))
			{
				pr_err("TI-SmartPA: %s: Failed to read Re value from bin file %s\n",__func__, SMARTAMP_SPEAKER_CALIBDATA_FILE);
				break;
			}
		}
	    case AFE_SA_SET_PROFILE :
	    case AFE_SA_CALIB_INIT :
	    case AFE_SA_CALIB_DEINIT :
	    case AFE_SA_F0_TEST_INIT :
	    case AFE_SA_F0_TEST_DEINIT :
	   	{
			pr_info("TI-SmartPA: %s: command_id = %d, data = %d\n",__func__,cmd,data);
			param_id |= (1 << 24) | (sz << 16);
			break;
		}
	    default :
	    	{
	    		pr_err("TI-SmartPA: %s: unsupported command ID %d (0x%x)\n",__func__,cmd,cmd);
	    		return ret;
	    	}
	}

	memcpy(port_config.payload,&data,sz);
	ret = afe_smartamp_set_calib_data(param_id,&port_config,sz,module_id);
	if(ret)
	{
		pr_err("TI-SmartPA: %s: cmd %d(0x%x) failed\n",__func__,cmd,cmd);
	}

	return ret;
}

static int tas2560_get_parameters(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
	struct afe_smartamp_get_calib port_config;
	struct soc_mixer_control *mc = (struct soc_mixer_control *)pKcontrol->private_value;
	int data = pValue->value.integer.value[0];
	int sz = sizeof(int);
	int cmd = mc->shift;
	int param_id = cmd;
	int module_id = AFE_SMARTAMP_MODULE;
	int ret = -1;
	unsigned int return_val = 0;
	switch(cmd)
	{
	   case CAPI_V2_TAS_TX_ENABLE :
	   	{
			pr_info("TI-SmartPA: %s: command_id = %x, data = %d\n",__func__,cmd,data);
			module_id = AFE_SMARTAMP_MODULE_TX;
			break;
		}
	    case CAPI_V2_TAS_RX_ENABLE :
	   	{
			pr_info("TI-SmartPA: %s: command_id = %x, data = %d\n",__func__,cmd,data);
			break;
		}
	    case AFE_SA_GET_RE :
	    case AFE_SA_GET_F0 :
	    case AFE_SA_GET_Q :
	    case AFE_SA_GET_TV :
	    case AFE_SA_SET_PROFILE :
	   	{
			pr_info("TI-SmartPA: %s: command_id = %d, data = %d\n",__func__,cmd,data);
			param_id |= (1 << 24) | (sz << 16);
			break;
		}
	    default :
	    	{
	    		pr_err("TI-SmartPA: %s: unsupported command ID %d (0x%x)\n",__func__,cmd,cmd);
	    		return ret;
	    	}
	}
	memset(port_config.res_cfg.payload,0x0,TAS_PAYLOAD_SIZE);
	ret = afe_smartamp_get_calib_data(&port_config,param_id,module_id);
	if(ret)
	{
		pr_err("TI-SmartPA: %s: cmd %d(0x%x) failed\n",__func__,cmd,cmd);
	}else{
		memcpy(&pValue->value.integer.value[0],port_config.res_cfg.payload,sz);
		pr_err("TI-SmartPA: %s: value = %ld",__func__,pValue->value.integer.value[0]);
	}
 	return_val = (unsigned int )pValue->value.integer.value[0];
 	pr_err("TI-SmartPA: %s: Ret_value = x%x",__func__,return_val);
	return return_val;
}

static int tas2560_no_impl(struct snd_kcontrol *pKcontrol,
				struct snd_ctl_elem_value *pValue)
{
	return 0;
}

static const char *load_text[] = {"8_Ohm", "6_Ohm", "4_Ohm"};

static const struct soc_enum load_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(load_text), load_text),
};

static const char *Sampling_Rate_text[] = {"48_khz", "44.1_khz", "16_khz", "8_khz"};

static const struct soc_enum Sampling_Rate_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(Sampling_Rate_text), Sampling_Rate_text),
};

/*
 * DAC digital volumes. From 0 to 15 dB in 1 dB steps
 */
static DECLARE_TLV_DB_SCALE(dac_tlv, 0, 100, 0);

static const struct snd_kcontrol_new tas2560_snd_controls[] = {
	SOC_SINGLE_TLV("DAC Playback Volume", TAS2560_SPK_CTRL_REG, 0, 0x0f, 0,
			dac_tlv),
	SOC_ENUM_EXT("TAS2560 Boost load", load_enum[0],
			tas2560_get_load, tas2560_set_load),
	SOC_ENUM_EXT("TAS2560 Sampling Rate", Sampling_Rate_enum[0],
			tas2560_get_Sampling_Rate, tas2560_set_Sampling_Rate),
	SOC_SINGLE_EXT("TAS2560 PowerCtrl", SND_SOC_NOPM, 0, 0x0001, 0,
			tas2560_power_ctrl_get, tas2560_power_ctrl_put),
	/*Controls to enable/disable and load parameters*/
	SOC_SINGLE_EXT("CAPI_V2_TAS_FEEDBACK_INFO", SND_SOC_NOPM, 0, 0x1, 0,
			tas2560_no_impl, tas2560_fb_path_info),
	SOC_SINGLE_EXT("TAS2560_ALGO_TX_ENABLE", SND_SOC_NOPM, CAPI_V2_TAS_TX_ENABLE, 0x1, 0,
			tas2560_no_impl, tas2560_set_parameters),
	SOC_SINGLE_EXT("TAS2560_ALGO_RX_ENABLE", SND_SOC_NOPM, CAPI_V2_TAS_RX_ENABLE, 0x1, 0,
			tas2560_no_impl, tas2560_set_parameters),
	SOC_SINGLE_EXT("TAS2560_ALGO_RX_CFG", SND_SOC_NOPM, CAPI_V2_TAS_RX_CFG, 0x1, 0,
			tas2560_no_impl, tas2560_set_parameters),
	SOC_ENUM_EXT("TAS2560_ALGO_BYPASS", tas2560_smartamp_bypass_enum[0],
			tas2560_smartamp_bypass_get, tas2560_smartamp_bypass_set),
	/*Controls to set Rdc and Profile ID*/
	SOC_SINGLE_EXT("TAS2560_ALGO_SET_RE", SND_SOC_NOPM, AFE_SA_SET_RE, 0x7fffffff, 0,
			tas2560_no_impl, tas2560_set_parameters),
	SOC_SINGLE_EXT("TAS2560_ALGO_SET_PROFILE", SND_SOC_NOPM, AFE_SA_SET_PROFILE, 0x5, 0,
			tas2560_get_parameters, tas2560_set_parameters),
	/*Controls for calibration*/
	SOC_SINGLE_EXT("TAS2560_ALGO_CALIB_INIT", SND_SOC_NOPM, AFE_SA_CALIB_INIT, 0x1, 0,
			tas2560_no_impl, tas2560_set_parameters),
	SOC_SINGLE_EXT("TAS2560_ALGO_CALIB_DEINIT", SND_SOC_NOPM, AFE_SA_CALIB_DEINIT, 0x1, 0,
			tas2560_no_impl, tas2560_set_parameters),
	SOC_SINGLE_EXT("TAS2560_ALGO_F0_TEST_INIT", SND_SOC_NOPM, AFE_SA_F0_TEST_INIT, 0x1, 0,
			tas2560_no_impl, tas2560_set_parameters),
	SOC_SINGLE_EXT("TAS2560_ALGO_F0_TEST_DEINIT", SND_SOC_NOPM, AFE_SA_F0_TEST_DEINIT, 0x1, 0,
			tas2560_no_impl, tas2560_set_parameters),
        /*Controls for reading calibration parameters*/
        SOC_SINGLE_EXT("TAS2560_ALGO_GET_RE", SND_SOC_NOPM, AFE_SA_GET_RE, 0x7fffffff, 0,
			tas2560_get_parameters, tas2560_no_impl),
 	SOC_SINGLE_EXT("TAS2560_ALGO_GET_F0", SND_SOC_NOPM, AFE_SA_GET_F0, 0x7fffffff, 0,
			tas2560_get_parameters, tas2560_no_impl),
	 SOC_SINGLE_EXT("TAS2560_ALGO_GET_Q", SND_SOC_NOPM, AFE_SA_GET_Q, 0x7fffffff, 0,
			tas2560_get_parameters, tas2560_no_impl),
	SOC_SINGLE_EXT("TAS2560_ALGO_GET_TEMP", SND_SOC_NOPM, AFE_SA_GET_TV, 0x7fffffff, 0,
			tas2560_get_parameters, tas2560_no_impl),
};

static struct snd_soc_codec_driver soc_codec_driver_tas2560 = {
	.probe			= tas2560_codec_probe,
	.remove			= tas2560_codec_remove,
	.read			= tas2560_codec_read,
	.write			= tas2560_codec_write,
	.suspend		= tas2560_codec_suspend,
	.resume			= tas2560_codec_resume,
	.component_driver = {
		.controls		= tas2560_snd_controls,
		.num_controls		= ARRAY_SIZE(tas2560_snd_controls),
		.dapm_widgets		= tas2560_dapm_widgets,
		.num_dapm_widgets	= ARRAY_SIZE(tas2560_dapm_widgets),
		.dapm_routes		= tas2560_audio_map,
		.num_dapm_routes	= ARRAY_SIZE(tas2560_audio_map),
	},
};

int tas2560_register_codec(struct tas2560_priv *pTAS2560)
{
	int nResult = 0;

	dev_info(pTAS2560->dev, "%s, enter\n", __func__);
	nResult = snd_soc_register_codec(pTAS2560->dev,
		&soc_codec_driver_tas2560,
		tas2560_dai_driver, ARRAY_SIZE(tas2560_dai_driver));

	return nResult;
}
EXPORT_SYMBOL(tas2560_register_codec);

int tas2560_deregister_codec(struct tas2560_priv *pTAS2560)
{
	snd_soc_unregister_codec(pTAS2560->dev);

	return 0;
}
EXPORT_SYMBOL(tas2560_deregister_codec);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2560 ALSA SOC Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif /* CONFIG_TAS2560_CODEC */
