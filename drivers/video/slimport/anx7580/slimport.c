/*
* Copyright(c) 2012-2013, Analogix Semiconductor All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#define pr_fmt(fmt) "%s %s: " fmt, "anx7580", __func__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/slimport7580.h>
#include <linux/async.h>
#include <linux/of_platform.h>

#include "slimport_tx_drv.h"

#include <video/msm_dba.h>
#include "../../fbdev/msm/msm_dba/msm_dba_internal.h"

struct anx7580_data {
	struct i2c_client *client;
	struct anx7580_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	struct wake_lock slimport_lock;
	struct msm_dba_device_info dev_info;
	int gpio_p_dwn;
	int gpio_reset;
	//int gpio_int;
	//int gpio_cbl_det;
	const char *vdd10_name;
	const char *vdd18_name;
	const char *avdd33_name;
	struct regulator *avdd_reg;
	struct regulator *vdd18_reg;
	struct regulator *vdd_reg;
//struct platform_device *hdmi_pdev;
//	struct msm_hdmi_sp_ops *hdmi_sp_ops;
	bool update_chg_type;
};

// Since 0x54 is used in ANX7625, we must use I2C_ADDR_SEL as 1
#define I2C_ADDR_SEL 1

// Register ADDR definition
#if I2C_ADDR_SEL == 1
#define CHICAGO_SLAVE_ID_ADDR   0x8E
#define CHICAGO_OFFSET_ADDR             0x86
#elif I2C_ADDR_SEL == 0
#define CHICAGO_SLAVE_ID_ADDR   0x54
#define CHICAGO_OFFSET_ADDR             0x52
#endif

#define VALUE_FAILURE      -1
#define VALUE_SUCCESS      0

int ANX7580_POWER_STATE = 0;
EXPORT_SYMBOL(ANX7580_POWER_STATE);
struct anx7580_data *anx_chip2;
/*
#ifdef HDCP_EN
static bool hdcp_enable2 = 1;
#else
static bool hdcp_enable2;
#endif
*/
struct completion init_aux_ch_completion2;
//static uint32_t sp_tx_chg_current_ma = NORMAL_CHG_I_MA;

static int notify_control = 0;

int EDID_ready2 = 0;
EXPORT_SYMBOL(EDID_ready2);

extern BYTE bEDID_twoblock2[256];

void anx7580_notify_clients(struct msm_dba_device_info *dev,
		enum msm_dba_callback_event event)
{
	struct msm_dba_client_info *c;
	struct list_head *pos = NULL;

	pr_info("%s++\n", __func__);

	if (!dev) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	list_for_each(pos, &dev->client_list) {
		c = list_entry(pos, struct msm_dba_client_info, list);

		pr_info("%s: notifying event %d to client %s\n", __func__,
			event, c->client_name);

		if (c && c->cb)
			c->cb(c->cb_data, event);
	}

	pr_info("%s--\n", __func__);
}
EXPORT_SYMBOL(anx7580_notify_clients);

static int anx7580_avdd_3p3_power(struct anx7580_data *chip, int on)
{
	static int on_state;
	int ret = 0;

	if (on_state == on) {
		pr_info("avdd 3.3V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->avdd_reg) {
		chip->avdd_reg = regulator_get(&chip->client->dev, chip->avdd33_name);
		if (IS_ERR(chip->avdd_reg)) {
			ret = PTR_ERR(chip->avdd_reg);
			pr_err("regulator_get %s failed. rc = %d\n",
			       chip->avdd33_name, ret);
			chip->avdd_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->avdd_reg);
		if (ret) {
			pr_err("avdd_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->avdd_reg);
		if (ret) {
			pr_err("avdd_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->avdd_reg);
	chip->avdd_reg = NULL;
out:
	return ret;
}

static int anx7580_vdd_1p8_power(struct anx7580_data *chip, int on)
{
	static int on_state;
	int ret = 0;

	if (on_state == on) {
		pr_info("vdd 1.8V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->vdd18_reg) {
		chip->vdd18_reg = regulator_get(&chip->client->dev, chip->vdd18_name);
		if (IS_ERR(chip->vdd18_reg)) {
			ret = PTR_ERR(chip->vdd18_reg);
			pr_err("regulator_get %s failed. ret = %d\n",
			       chip->vdd18_name, ret);
			chip->vdd18_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->vdd18_reg);
		if (ret) {
			pr_err("vdd18_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->vdd18_reg);
		if (ret) {
			pr_err("vdd18_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->vdd18_reg);
	chip->vdd18_reg = NULL;
out:
	return ret;
}

static int anx7580_vdd_1p0_power(struct anx7580_data *chip, int on)
{
	static int on_state;
	int ret = 0;

	if (on_state == on) {
		pr_info("vdd 1.0V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->vdd_reg) {
		chip->vdd_reg = regulator_get(&chip->client->dev, chip->vdd10_name);
		if (IS_ERR(chip->vdd_reg)) {
			ret = PTR_ERR(chip->vdd_reg);
			pr_err("regulator_get %s failed. ret = %d\n",
			       chip->vdd10_name, ret);
			chip->vdd_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->vdd_reg);
		if (ret) {
			pr_err("vdd_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->vdd_reg);
		if (ret) {
			pr_err("vdd_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->vdd_reg);
	chip->vdd_reg = NULL;
out:
	return ret;
}

int sp_read_reg_anx7580(uint8_t slave_id, uint8_t offset_addr, uint8_t *p_data)
{
	/*
	int ret = 0;

	if (!anx_chip2)
		return -EINVAL;

	anx_chip2->client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx_chip2->client, offset);
	if (ret < 0) {
		pr_err("failed to read i2c addr=%x\n", slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	pr_err("[ANX7580] I2C Read(%x, %x): [%x]\n", slave_addr, offset, ret);

	return 0;*/
	int ret = 0;

	//pr_info("%s: I2C read id=%02X offset=%04X\n",LOG_TAG,slave_id,offset_addr);

	if (!anx_chip2)
		return -EINVAL;

	if((((slave_id & 0x0F)!=0)&&((offset_addr&0xFF00)!=0))||((offset_addr&0xF000)!=0))
	{
		pr_info("%s %s: I2C slave_id or offset_addr ERROR!! %02x %04x\n"
				,LOG_TAG,__func__,slave_id,offset_addr);
		// CHIC-447, I2C read interface returned the value was "SUCCESS" even it was "FAILURE".
		*p_data = 0;
		return VALUE_FAILURE;
	}

	anx_chip2->client->addr = (CHICAGO_SLAVE_ID_ADDR >> 1);
	ret = i2c_smbus_write_byte_data(anx_chip2->client, 0x00, (slave_id |
				(u8)((offset_addr&0x0F00)>>8)));
	if (ret < 0) {
		pr_err("%s %s: failed to write i2c addr=%x\n", LOG_TAG,__func__,
				anx_chip2->client->addr);
		// CHIC-447, I2C read interface returned the value was "SUCCESS" even it was "FAILURE".
		*p_data = 0;
		return VALUE_FAILURE;
	}else{
		anx_chip2->client->addr = (CHICAGO_OFFSET_ADDR >> 1);
		ret = i2c_smbus_read_byte_data(anx_chip2->client,
				(u8)(offset_addr&0x00FF));
		if (ret < 0) {
			pr_err("%s %s: failed to read i2c addr=%x\n",
					LOG_TAG,__func__, anx_chip2->client->addr);
			// CHIC-447, I2C read interface returned the value was "SUCCESS" even it was "FAILURE".
			*p_data = 0;
			return VALUE_FAILURE;
		}
		*p_data = (u8)ret;
	}
	delay_ms(10);
	return VALUE_SUCCESS;
}

int sp_write_reg_anx7580(uint8_t slave_id, uint8_t offset_addr, uint8_t data)
{
	int ret = 0;

	if (!anx_chip2)
		return -EINVAL;

	//pr_info("%s: I2C write id=%02X offset=%04X data=%02X\n",LOG_TAG,slave_id,offset_addr,data);

	if((((slave_id & 0x0F)!=0)&&((offset_addr&0xFF00)!=0))||((offset_addr&0xF000)!=0))
	{
		pr_err("%s %s: I2C slave_id or offset_addr ERROR!! %02x	%04x\n",LOG_TAG,__func__,slave_id,offset_addr);
		return -1;
	}

	anx_chip2->client->addr = (CHICAGO_SLAVE_ID_ADDR >> 1);
	ret = i2c_smbus_write_byte_data(anx_chip2->client, 0x00, (slave_id |
				(u8)((offset_addr&0x0F00)>>8)));
	if (ret < 0) {
		pr_err("%s %s: failed to write i2c addr=%x\n", LOG_TAG,__func__,
				anx_chip2->client->addr);
		// CHIC-447, I2C read interface returned the value was "SUCCESS" even it was "FAILURE".
			return -1;
	} else {
		anx_chip2->client->addr = (CHICAGO_OFFSET_ADDR >> 1);
		ret = i2c_smbus_write_byte_data(anx_chip2->client,
				(u8)(offset_addr&0x00FF), data);
		if (ret < 0) {
			pr_err("%s %s: failed to write i2c addr=%x\n",
					LOG_TAG,__func__, anx_chip2->client->addr);
			// CHIC-447, I2C read interface returned the value was "SUCCESS" even it was "FAILURE".
				return -1;
		}
	}
	return 0;
}

void sp_tx_hardware_poweron2(void)
{
	if (!anx_chip2)
		return;

	gpio_direction_output(anx_chip2->gpio_reset, 0);
	msleep(1);
	gpio_direction_output(anx_chip2->gpio_p_dwn, 0);
	msleep(2);
	anx7580_vdd_1p0_power(anx_chip2, 1);
	msleep(5);
	gpio_direction_output(anx_chip2->gpio_reset, 1);

	pr_err("anx7580 power on\n");
}

void sp_tx_hardware_powerdown2(void)
{
//int status = 0;

	if (!anx_chip2)
		return;

	gpio_direction_output(anx_chip2->gpio_reset, 0);
	msleep(1);
	anx7580_vdd_1p0_power(anx_chip2, 0);
	msleep(2);
	gpio_direction_output(anx_chip2->gpio_p_dwn, 1);
	msleep(1);

	/* turn off hpd */
	/*
	if (anx_chip2->hdmi_sp_ops->set_upstream_hpd) {
	status = anx_chip2->hdmi_sp_ops->set_upstream_hpd(
	anx_chip2->hdmi_pdev, 0);
	if (status)
	pr_err("failed to turn off hpd");
	}
	*/
	pr_info("anx7580 power down\n");
}

/*
static void sp_tx_power_down_and_init(void)
{
	vbus_power_ctrl2();
	sp_tx_power_down(SP_TX_PWR_REG);
	sp_tx_power_down(SP_TX_PWR_TOTAL);
	sp_tx_hardware_powerdown2();
	sp_tx_pd_mode2 = 1;
	sp_tx_link_config_done = 0;
	sp_tx_hw_lt_enable = 0;
	sp_tx_hw_lt_done = 0;
	sp_tx_rx_type2 = RX_NULL;
	sp_tx_rx_type2_backup = RX_NULL;
	sp_tx_set_sys_state(STATE_CABLE_PLUG);
}

*/



int slimport_read_edid_block2(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bEDID_firstblock2, sizeof(bEDID_firstblock2));
	} else if (block == 1) {
		memcpy(edid_buf, bEDID_extblock2, sizeof(bEDID_extblock2));
	} else {
		pr_err("%s: block number %d is invalid\n", __func__, block);
		return -EINVAL;
	}

	return 0;
}

EXPORT_SYMBOL(slimport_read_edid_block2);

int update_audio_format_setting2(unsigned char  bAudio_Fs, unsigned char bAudio_word_len, int Channel_Num, I2SLayOut layout)
{
	//pr_info("bAudio_Fs = %d, bAudio_word_len = %d, Channel_Num = %d, layout = %d\n", bAudio_Fs, bAudio_word_len, Channel_Num, layout); //liu
	SP_CTRL_AUDIO_FORMAT_Set2(AUDIO_I2S,bAudio_Fs ,bAudio_word_len);
	SP_CTRL_I2S_CONFIG_Set2(Channel_Num , layout);
	audio_format_change2=1;
	
	return 0;
}
EXPORT_SYMBOL(update_audio_format_setting2);

int hdcp_eanble_setting2(bool on)
{
	hdcp_enable2=on;
	return 0;
}
EXPORT_SYMBOL(hdcp_eanble_setting2);

unchar sp_get_rx_bw2(void)
{
	return sp_rx_bw2;
}
EXPORT_SYMBOL(sp_get_rx_bw2);

#if 0
static int anx7580_mipi_timing_setting(void *client, bool on,
		struct msm_dba_video_cfg *cfg, u32 flags)
{
	if (!cfg) {
		pr_err("%s: invalid input\n", __func__);
		return 1;
	}

	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_HTOTAL = cfg->h_active + cfg->h_front_porch +
	      cfg->h_pulse_width + cfg->h_back_porch;
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_VTOTAL= cfg->v_active + cfg->v_front_porch +
	      cfg->v_pulse_width + cfg->v_back_porch;

	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_HActive = cfg->h_active;
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_H_Sync_Width= cfg->h_pulse_width;
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_H_Front_Porch= cfg->h_front_porch;
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_H_Back_Porch= cfg->h_back_porch;

	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_VActive = cfg->v_active;
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_V_Sync_Width= cfg->v_pulse_width;
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_V_Front_Porch= cfg->v_front_porch;
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_V_Back_Porch= cfg->v_back_porch;

	mipi_lane_count2 = cfg->num_of_input_lanes;
	//mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_pixel_frequency=(unsigned int)(cfg->pclk2_khz/1000);
	mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_pixel_frequency =
		mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_HTOTAL *
		mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_VTOTAL * 60 / 1000;

	pr_info("cfg->pclk2_khz = %d\n", cfg->pclk2_khz);

	pr_info("h_total = %d, h_active = %d, hfp = %d, hpw = %d, hbp = %d\n",
		mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_HTOTAL, cfg->h_active, cfg->h_front_porch,
		cfg->h_pulse_width, cfg->h_back_porch);

	pr_info("v_total = %d, v_active = %d, vfp = %d, vpw = %d, vbp = %d\n",
		mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_VTOTAL, cfg->v_active, cfg->v_front_porch,
		cfg->v_pulse_width, cfg->v_back_porch);

	pr_info("pixel clock = %lu, lane count = %d, \n", mipi_video_timing_table2[bMIPIFormatIndex2].MIPI_pixel_frequency, cfg->num_of_input_lanes);

	video_format_change2 = 1;
	SP_CTRL_Set_System_State2(SP_TX_CONFIG_VIDEO_INPUT);

	return 0;
}
#endif
bool slimport_is_connected3(void)
{
	bool result = false;

	if (!anx_chip2)
		return false;
/*
	if (gpio_get_value_cansleep(anx_chip2->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(anx_chip2->gpio_cbl_det)) {
			pr_info("slimport cable is detected\n");
			result = true;
		}
	}
*/
	return result;
}
EXPORT_SYMBOL(slimport_is_connected3);


static void anx7580_free_gpio(struct anx7580_data *anx7580)
{
	//gpio_free(anx7580->gpio_cbl_det);
	//gpio_free(anx7580->gpio_int);
	gpio_free(anx7580->gpio_reset);
	gpio_free(anx7580->gpio_p_dwn);
}

static int anx7580_pinctrl_configure(struct pinctrl *key_pinctrl, bool active)
{
	struct pinctrl_state *set_state;
	int retval;

	if (active) {
		set_state = pinctrl_lookup_state(key_pinctrl, "pmx_anx_int_active");
		if (IS_ERR(set_state)) {
			pr_err("%s: cannot get anx7580 pinctrl active state\n", __func__);
			return PTR_ERR(set_state);
		}
	}
	else {
		/* suspend setting here */
	}
	retval = pinctrl_select_state(key_pinctrl, set_state);
	if (retval) {
		pr_err("%s: cannot set anx7580 pinctrl state\n", __func__);
		return retval;
	}

	pr_info("%s: configure pinctrl success\n", __func__);
	return 0;
}


static int anx7580_init_gpio(struct anx7580_data *anx7580)
{
	int ret = 0;
	struct pinctrl *key_pinctrl;

	/* Get pinctrl if target uses pinctrl */
	key_pinctrl = devm_pinctrl_get(&anx7580->client->dev);
	if (IS_ERR(key_pinctrl)) {
		if (PTR_ERR(key_pinctrl) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		pr_debug("Target does not use pinctrl\n");
		key_pinctrl = NULL;
	}
	if (key_pinctrl) {
		pr_debug("Target uses pinctrl\n");
		ret = anx7580_pinctrl_configure(key_pinctrl, true);
		if (ret)
			pr_err("%s: cannot configure anx_int pinctrl\n", __func__);
	}

	ret = gpio_request_one(anx7580->gpio_p_dwn,
	                       GPIOF_OUT_INIT_HIGH, "anx_p_dwn_ctl");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7580->gpio_p_dwn);
		goto out;
	}

	ret = gpio_request_one(anx7580->gpio_reset,
	                       GPIOF_OUT_INIT_LOW, "anx7580_reset_n");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7580->gpio_reset);
		goto err0;
	}
/*
	ret = gpio_request_one(anx7580->gpio_int,
	                       GPIOF_IN, "anx7580_int_n");

	if (ret) {
		pr_err("failed to request gpio %d\n", anx7580->gpio_int);
		goto err1;
	}

	ret = gpio_request_one(anx7580->gpio_cbl_det,
	                       GPIOF_IN, "anx7580_cbl_det");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7580->gpio_cbl_det);
		goto err2;
	}*/
	//gpio_direction_input(anx7580->gpio_cbl_det);
	gpio_direction_output(anx7580->gpio_reset, 0);
	gpio_direction_output(anx7580->gpio_p_dwn, 1);

	goto out;	
//err2:
//	gpio_free(anx7580->gpio_int);
//err1:
//	gpio_free(anx7580->gpio_reset);
err0:
	gpio_free(anx7580->gpio_p_dwn);
out:
	return ret;
}
#if 0
static int anx7580_system_init(void)
{
	int ret = 0;

	ret = SP_CTRL_Chip_Detect2();
	if (ret == 0) {
		pr_err("failed to detect anx7580\n");
		return -ENODEV;
	}

	SP_CTRL_Chip_Initial2();
	return 0;
}
#endif
#if 0
static irqreturn_t anx7580_cbl_det_isr(int irq, void *data)
{
	struct anx7580_data *anx7580 = data;
	int status;

	if (gpio_get_value(anx7580->gpio_cbl_det)) {
		wake_lock(&anx7580->slimport_lock);
		pr_info("detect cable insertion\n");
		queue_delayed_work(anx7580->workqueue, &anx7580->work, 0);
	} else {		
		/* check HPD state again after 5 ms to see if it is HPD irq event */
		#ifdef Standard_DP
		mdelay(5);

		if (!gpio_get_value(anx7580->gpio_cbl_det)) { // if it is one IRQ, should not destroy ANX7580 work queue
		#endif
			
		pr_info("detect cable removal\n");
		status = cancel_delayed_work_sync(&anx7580->work);
		if (status == 0)
			flush_workqueue(anx7580->workqueue);
		//when HPD low, power down ANX7580
		if(sp_tx_pd_mode2==0)
		{
			SP_CTRL_Set_System_State2(SP_TX_WAIT_SLIMPORT_PLUGIN);
			system_power_ctrl2(0);
		}
		
		wake_unlock(&anx7580->slimport_lock);
		wake_lock_timeout(&anx7580->slimport_lock, 2*HZ);



			/* Notify DBA framework disconnect event */
			anx7580_notify_clients(&anx7580->dev_info,
				MSM_DBA_CB_HPD_DISCONNECT);

			/* clear notify_control */
			notify_control = 0;

			/* clear EDID_ready2 */
			EDID_ready2 = 0;
		#ifdef Standard_DP
		}
		#endif
	}
	return IRQ_HANDLED;
}
#endif
static void anx7580_work_func(struct work_struct *work)
{
#ifndef EYE_TEST
	struct anx7580_data *td = container_of(work, struct anx7580_data,
	                                       work.work);
	int workqueu_timer = 0;
	if(get_system_state2() >= SP_TX_PLAY_BACK)
		workqueu_timer = 500;
	else
		workqueu_timer = 100;

	SP_CTRL_Main_Procss2();
	queue_delayed_work(td->workqueue, &td->work,
	                   msecs_to_jiffies(workqueu_timer));

	if (!notify_control && EDID_ready2 && slimport_is_connected3()) {
		/* Notify DBA framework connect event */
		anx7580_notify_clients(&td->dev_info,
				MSM_DBA_CB_HPD_CONNECT);

		notify_control = 1;
	}
#endif
}

static int anx7580_parse_dt(struct device_node *node,
                            struct anx7580_data *anx7580)
{
	int ret = 0;
//struct platform_device *hdmi_pdev = NULL;
	struct device_node *hdmi_tx_node = NULL;

	anx7580->gpio_p_dwn =
	    of_get_named_gpio(node, "analogix,p-dwn-gpio", 0);
	if (anx7580->gpio_p_dwn < 0) {
		pr_err("failed to get analogix,p-dwn-gpio.\n");
		ret = anx7580->gpio_p_dwn;
		goto out;
	}

	anx7580->gpio_reset =
	    of_get_named_gpio(node, "analogix,reset-gpio", 0);
	if (anx7580->gpio_reset < 0) {
		pr_err("failed to get analogix,reset-gpio.\n");
		ret = anx7580->gpio_reset;
		goto out;
	}
/*
	anx7580->gpio_int =
	    of_get_named_gpio(node, "analogix,irq-gpio", 0);
	if (anx7580->gpio_int < 0) {
		pr_err("failed to get analogix,irq-gpio.\n");
		ret = anx7580->gpio_int;
		goto out;
	}

	anx7580->gpio_cbl_det =
	    of_get_named_gpio(node, "analogix,cbl-det-gpio", 0);
	if (anx7580->gpio_cbl_det < 0) {
		pr_err("failed to get analogix,cbl-det-gpio.\n");
		ret = anx7580->gpio_cbl_det;
		goto out;
	}
*/
	ret = of_property_read_string(node, "analogix,vdd10-name",
	                              &anx7580->vdd10_name);
	if (ret) {
		pr_err("failed to get vdd10-name.\n");
		goto out;
	}

	ret = of_property_read_string(node, "analogix,vdd18-name",
	                              &anx7580->vdd18_name);
	if (ret) {
		pr_err("failed to get vdd18-name.\n");
		goto out;
	}
	
	ret = of_property_read_string(node, "analogix,avdd33-name",
	                              &anx7580->avdd33_name);
	if (ret) {
		pr_err("failed to get avdd33-name.\n");
		goto out;
	}

	/* parse phandle for hdmi tx handle */
	hdmi_tx_node = of_parse_phandle(node, "analogix,hdmi-tx-map", 0);
	if (!hdmi_tx_node) {
		pr_err("can't find hdmi phandle\n");
//		ret = -EINVAL;
//		goto out;
	}
	/*
	hdmi_pdev = of_find_device_by_node(hdmi_tx_node);
	if (!hdmi_pdev) {
	pr_err("can't find the deivce by node\n");
	ret = -EINVAL;
	goto out;
	}
	anx7580->hdmi_pdev = hdmi_pdev;
	*/
out:
	return ret;
}

#if 0
static int anx7580_get_raw_edid(void *client,
	u32 size, char *buf, u32 flags)
{
	struct anx7580_data *pdata =
		anx7580_get_platform_data(client);

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		goto end;
	}

	mutex_lock(&pdata->lock);

	pr_info("%s: size=%d\n", __func__, size);
	size = min_t(u32, size, sizeof(bEDID_twoblock2));

	pr_info("%s: memcpy EDID block, size=%d\n", __func__, size);
	memcpy(buf, bEDID_twoblock2, size);
:q

	mutex_unlock(&pdata->lock);
end:
	return 0;
}


static int anx7580_register_dba(struct anx7580_data *pdata)
{
	struct msm_dba_ops *client_ops;
	struct msm_dba_device_ops *dev_ops;

	if (!pdata)
		return -EINVAL;

	client_ops = &pdata->dev_info.client_ops;
	dev_ops = &pdata->dev_info.dev_ops;

	client_ops->power_on        = NULL;
	client_ops->video_on        = anx7580_mipi_timing_setting;
	client_ops->configure_audio = NULL;
	client_ops->hdcp_enable2     = NULL;
	client_ops->hdmi_cec_on     = NULL;
	client_ops->hdmi_cec_write  = NULL;
	client_ops->hdmi_cec_read   = NULL;
	client_ops->get_edid_size   = NULL;
	client_ops->get_raw_edid    = anx7580_get_raw_edid;
	client_ops->check_hpd	    = NULL;

	strlcpy(pdata->dev_info.chip_name, "anx7580",
		sizeof(pdata->dev_info.chip_name));

	pdata->dev_info.instance_id = 0;

	mutex_init(&pdata->dev_info.dev_mutex);

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	return msm_dba_add_probed_device(&pdata->dev_info);
}
#endif
int anx7580_power_control(int on_off)
{
	int ret = 0;

	pr_err("%s: Start\n", __func__);


	if (!anx_chip2)
		return -EINVAL;


	if(ANX7580_POWER_STATE == on_off)
	{
		pr_err("%s : an7850 power [%d] control already Done!!\n" , __func__ , on_off);
		goto exit;
	}

	ret = anx7580_avdd_3p3_power(anx_chip2, on_off);
	if (ret)
	{
		pr_err("%s : anx7580 avdd 3.3v power [%d] control failed!!\n" , __func__ , on_off);
		goto err1;
	}

	ret = anx7580_vdd_1p8_power(anx_chip2, on_off);
	if (ret)
	{
		pr_err("%s : anx7580 avdd 1.8v power [%d] control failed!!\n" , __func__ , on_off);
		goto err2;
	}

	msleep(1);
	ret = anx7580_vdd_1p0_power(anx_chip2, on_off);
	if (ret)
	{
		pr_err("%s : anx7580 avdd 1.0v power [%d] control failed!!\n" , __func__ , on_off);
		goto err3;
	}

	goto exit;
err3:
	if (!anx_chip2->vdd18_reg)
		regulator_put(anx_chip2->vdd18_reg);
err2:
	if (!anx_chip2->avdd_reg)
		regulator_put(anx_chip2->avdd_reg);
err1:
	pr_err("%s : anx7580 avdd 3.3v power [%d] control failed!!\n" , __func__ , on_off);
	destroy_workqueue(anx_chip2->workqueue);
exit:
	ANX7580_POWER_STATE = on_off;
	pr_err("%s : anx7580 avdd all  power [%d] control successed!!\n" , __func__ , on_off);
	ret = gpio_direction_output(102, on_off);
	if(ret)
	{
		pr_err("%s : fail to configure GPIO 102 failed!!\n" , __func__);
	}
	delay_ms(20);
	ret = gpio_direction_output(101, on_off);
	if(ret)
	{
		pr_err("%s : fail to configure GPIO 101 failed!!\n" , __func__);
	}
	return ret;
}
EXPORT_SYMBOL(anx7580_power_control);

static int anx7580_i2c_probe(struct i2c_client *client,
                             const struct i2c_device_id *id)
{
	struct anx7580_data *anx7580;
	struct anx7580_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	//struct msm_hdmi_sp_ops *hdmi_sp_ops = NULL;
	int ret = 0;

	pr_info("%s++\n", __func__);

	if (!i2c_check_functionality(client->adapter,
	                             I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("i2c bus does not support anx7580\n");
		ret = -ENODEV;
		goto exit;
	}

	pr_info("%s: i2c device name=%s, addr=0x%x, adapter nr=%d\n", __func__,
			client->name, client->addr, client->adapter->nr);

	anx7580 = kzalloc(sizeof(struct anx7580_data), GFP_KERNEL);
	if (!anx7580) {
		pr_err("failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit;
	}

	anx7580->client = client;
	i2c_set_clientdata(client, anx7580);

	if (dev_node) {
		ret = anx7580_parse_dt(dev_node, anx7580);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto err0;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			pr_err("no platform data.\n");
			goto err0;
		}

		anx7580->gpio_p_dwn = pdata->gpio_p_dwn;
		anx7580->gpio_reset = pdata->gpio_reset;
		//anx7580->gpio_int = pdata->gpio_int;
		//anx7580->gpio_cbl_det = pdata->gpio_cbl_det;
		anx7580->vdd10_name = pdata->vdd10_name;
		anx7580->vdd18_name = pdata->vdd18_name;
		anx7580->avdd33_name = pdata->avdd33_name;		
	}

	/* initialize hdmi_sp_ops */
	/*
	hdmi_sp_ops = devm_kzalloc(&client->dev,
	                           sizeof(struct msm_hdmi_sp_ops),
	                           GFP_KERNEL);
	if (!hdmi_sp_ops) {
		pr_err("alloc hdmi sp ops failed\n");
		goto err0;
	}
	
	if (anx7580->hdmi_pdev) {
	ret = msm_hdmi_register_sp(anx7580->hdmi_pdev,
	hdmi_sp_ops);
	if (ret) {
	pr_err("register with hdmi_failed\n");
	goto err0;
	}
	}
	
	anx7580->hdmi_sp_ops = hdmi_sp_ops;
*/
	anx_chip2 = anx7580;

	mutex_init(&anx7580->lock);
	init_completion(&init_aux_ch_completion2);
	ret = anx7580_init_gpio(anx7580);
	if (ret) {
		pr_err("failed to initialize gpio\n");
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7580->work, anx7580_work_func);

	anx7580->workqueue = create_singlethread_workqueue("anx7580_work");
	if (!anx7580->workqueue) {
		pr_err("failed to create work queue\n");
		ret = -ENOMEM;
		goto err1;
	}
/*	ret = gpio_direction_output(47, 1);
	if (ret){
		pr_err("fail to configure GPIO 47\n");
	}*/
	//anx7580_set_mux_path(ANX7580_WIFI);
	pr_err("%s succeed!\n", __func__);

	goto exit;
#if 0
err7:
	free_irq(client->irq, anx7580);
err6:
	wake_lock_destroy(&anx7580->slimport_lock);
#endif
err1:
	anx7580_free_gpio(anx7580);
err0:
	anx_chip2 = NULL;
	kfree(anx7580);
exit:
	pr_err("%s--\n", __func__);
	return ret;
}

static int anx7580_i2c_remove(struct i2c_client *client)
{
	struct anx7580_data *anx7580 = i2c_get_clientdata(client);

	free_irq(client->irq, anx7580);
	wake_lock_destroy(&anx7580->slimport_lock);
	if (!anx7580->vdd_reg)
		regulator_put(anx7580->vdd_reg);
	if (!anx7580->vdd18_reg)
		regulator_put(anx7580->vdd18_reg);
	if (!anx7580->avdd_reg)
		regulator_put(anx7580->avdd_reg);
	destroy_workqueue(anx7580->workqueue);
	anx7580_free_gpio(anx7580);
	anx_chip2 = NULL;
	kfree(anx7580);
	return 0;
}

static const struct i2c_device_id anx7580_id[] = {
	{ "anx7580", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, anx7580_id);

static struct of_device_id anx_match_table[] = {
	{ .compatible = "analogix,anx7580",},
	{ },
};

static struct i2c_driver anx7580_driver = {
	.driver = {
		.name = "anx7580",
		.owner = THIS_MODULE,
		.of_match_table = anx_match_table,
	},
	.probe = anx7580_i2c_probe,
	.remove = anx7580_i2c_remove,
	.id_table = anx7580_id,
};

static void __init anx7580_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = i2c_add_driver(&anx7580_driver);
	if (ret)
		pr_err("%s: failed to register anx7580 driver\n", __func__);
}

static int __init anx7580_init(void)
{
	pr_info("%s\n", __func__);
	async_schedule(anx7580_init_async, NULL);
	return 0;
}

static void __exit anx7580_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&anx7580_driver);
}

module_init(anx7580_init);
module_exit(anx7580_exit);

MODULE_DESCRIPTION("Slimport transmitter ANX7580 driver");
MODULE_AUTHOR("swang@analogixsemi.com");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");
