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

#define pr_fmt(fmt) "%s %s: " fmt, "anx7625", __func__

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
#include <linux/slimport7625.h>
#include <linux/async.h>
#include <linux/of_platform.h>

#include "slimport_tx_drv.h"

#include <video/msm_dba.h>
#include "../../fbdev/msm/msm_dba/msm_dba_internal.h"

struct anx7625_data {
	struct i2c_client *client;
	struct anx7625_platform_data *pdata;
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
#ifdef CONFIG_MACH_SDM450_WIDEPD
	const char *i2c_vdd18_name;
	const char *level_shifter_name;
	struct regulator *i2c_vdd18_reg;
	struct regulator *level_shifter_reg;
#endif
//struct platform_device *hdmi_pdev;
//	struct msm_hdmi_sp_ops *hdmi_sp_ops;
	bool update_chg_type;
#if defined(CONFIG_MACH_SDM450_DD14) || defined(CONFIG_MACH_SDM450_WIDEPD)
	int gpio_mux_sw_sel;
	int gpio_mux_sw_oe;
	int gpio_lcd_3v3_buck;
#endif
};

#define ANX7625_WIFI 0
#define ANX7625_USB 1
int ANX7625_POWER_STATE = 0;
EXPORT_SYMBOL(ANX7625_POWER_STATE);
struct anx7625_data *anx_chip3;
/*
#ifdef HDCP_EN
static bool hdcp_enable3 = 1;
#else
static bool hdcp_enable3;
#endif
*/
struct completion init_aux_ch_completion3;
//static uint32_t sp_tx_chg_current_ma = NORMAL_CHG_I_MA;

static int notify_control = 0;

static uint8_t last_access_DevAddr = 0xff;

int EDID_ready3 = 0;
EXPORT_SYMBOL(EDID_ready3);
#ifdef CONFIG_MACH_SDM450_WIDEPD
int WIFI_PATH = 1;//WIFI_PATH 0 means USB mode
EXPORT_SYMBOL(WIFI_PATH);
#endif

extern BYTE bEDID_twoblock3[256];
extern BYTE ATE_pattern7625(void);
extern BYTE ATE_pattern7580(void);
extern int anx7580_power_control(int on_off);
#ifdef CONFIG_MACH_SDM450_DS3CM
extern void set_ATE_pattern7625_prog(int enable);
#endif

#if 0
static struct anx7625_data *anx7625_get_platform_data(void *client)
{
	struct anx7625_data *pdata = NULL;
	struct msm_dba_device_info *dev;
	struct msm_dba_client_info *cinfo =
		(struct msm_dba_client_info *)client;

	if (!cinfo) {
		pr_err("%s: invalid client data\n", __func__);
		goto end;
	}

	dev = cinfo->dev;
	if (!dev) {
		pr_err("%s: invalid device data\n", __func__);
		goto end;
	}

	pdata = container_of(dev, struct anx7625_data, dev_info);
	if (!pdata)
		pr_err("%s: invalid platform data\n", __func__);

end:
	return pdata;
}
#endif

void anx7625_notify_clients(struct msm_dba_device_info *dev,
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
EXPORT_SYMBOL(anx7625_notify_clients);

static int anx7625_avdd_3p3_power(struct anx7625_data *chip, int on)
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

static int anx7625_vdd_1p8_power(struct anx7625_data *chip, int on)
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

static int anx7625_vdd_1p0_power(struct anx7625_data *chip, int on)
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

#ifdef CONFIG_MACH_SDM450_WIDEPD
static int anx7625_i2c_vdd_1p8_power(struct anx7625_data *chip, int on)
{
	static int on_state;
	int ret = 0;

	if (on_state == on) {
		pr_info("i2c_vdd 1.8V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->i2c_vdd18_reg) {
		chip->i2c_vdd18_reg = regulator_get(&chip->client->dev, chip->i2c_vdd18_name);
		if (IS_ERR(chip->i2c_vdd18_reg)) {
			ret = PTR_ERR(chip->i2c_vdd18_reg);
			pr_err("regulator_get %s failed. ret = %d\n",
			       chip->i2c_vdd18_name, ret);
			chip->i2c_vdd18_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->i2c_vdd18_reg);
		if (ret) {
			pr_err("i2c_vdd18_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->i2c_vdd18_reg);
		if (ret) {
			pr_err("i2c_vdd18_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->i2c_vdd18_reg);
	chip->i2c_vdd18_reg = NULL;
out:
	return ret;
}

static int anx7625_level_shifter_power(struct anx7625_data *chip, int on)
{
	static int on_state;
	int ret = 0;

	if (on_state == on) {
		pr_info("level_shifter 3.3V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->level_shifter_reg) {
		chip->level_shifter_reg = regulator_get(&chip->client->dev, chip->level_shifter_name);
		if (IS_ERR(chip->level_shifter_reg)) {
			ret = PTR_ERR(chip->level_shifter_reg);
			pr_err("regulator_get %s failed. ret = %d\n",
			       chip->level_shifter_name, ret);
			chip->level_shifter_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->level_shifter_reg);
		if (ret) {
			pr_err("level_shifter_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->level_shifter_reg);
		if (ret) {
			pr_err("level_shifter_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->level_shifter_reg);
	chip->level_shifter_reg = NULL;
out:
	return ret;
}
#endif

//static int first_boot_skip_anx7625_powerctrl = 1;
int anx7625_power_control(int on_off)
{
	int ret = 0;
/*
	if(first_boot_skip_anx7625_powerctrl)
	{
		first_boot_skip_anx7625_powerctrl = 0;
		pr_err("%s : anx7625_power_control skip ... first booting!!!\n" , __func__);
		return 0;
	}
*/
#ifdef CONFIG_MACH_SDM450_DS3CM
	if(!on_off){
		pr_err("%s:Reset flags\n", __func__);
		set_ATE_pattern7625_prog(0);
	}
#endif

	if(ANX7625_POWER_STATE == on_off)
	{
		pr_err("%s : anx7625 power [%d] control already Done!!\n" , __func__ , on_off);
		goto exit;
	}

	ret = anx7625_avdd_3p3_power(anx_chip3, on_off);
	if (ret)
		goto err1;
#if defined(CONFIG_MACH_SDM450_DD14)
	if (on_off)
#endif
	ret = anx7625_vdd_1p8_power(anx_chip3, on_off);
	if (ret)
		goto err2;

	ret = anx7625_vdd_1p0_power(anx_chip3, on_off);
	if (ret)
		goto err3;

#ifdef CONFIG_MACH_SDM450_WIDEPD
	ret = anx7625_i2c_vdd_1p8_power(anx_chip3, on_off);
	if (ret)
		goto err4;

	ret = anx7625_level_shifter_power(anx_chip3, on_off);
	if (ret)
		goto err5;
#endif

	goto exit;

err1://avdd 3.3v
	if (!anx_chip3->avdd_reg)
		regulator_put(anx_chip3->avdd_reg);
	pr_err("%s : anx7625 avdd 3.3v power [%d] control failed!!\n" , __func__ , on_off);
	return ret;
err2://avdd 1.8v
	if (!anx_chip3->vdd18_reg)
		regulator_put(anx_chip3->vdd18_reg);
	pr_err("%s : anx7625 avdd 1.8v power [%d] control failed!!\n" , __func__ , on_off);
	return ret;
err3://avdd 1.0v
	if (!anx_chip3->vdd_reg)
		regulator_put(anx_chip3->vdd_reg);
	pr_err("%s : anx7625 avdd 1.0v power [%d] control failed!!\n" , __func__ , on_off);
	return ret;
#ifdef CONFIG_MACH_SDM450_WIDEPD
err4://i2c_avdd 1.8v
	if (!anx_chip3->i2c_vdd18_reg)
		regulator_put(anx_chip3->i2c_vdd18_reg);
	pr_err("%s : anx7625 i2c_avdd 1.8v power [%d] control failed!!\n" , __func__ , on_off);
	return ret;
err5://level_shifter 3.3v
	if (!anx_chip3->level_shifter_reg)
		regulator_put(anx_chip3->level_shifter_reg);
	pr_err("%s : anx7625 level_shifter 3.3v power [%d] control failed!!\n" , __func__ , on_off);
	return ret;
#endif
exit:
	ANX7625_POWER_STATE = on_off;
	pr_err("%s : anx7625 all power [%d] control successed!!\n" , __func__ , on_off);
/****************************************************************************************
GPIO 62 : ANX2 EN
GPIO 47 : ANX2 RST
****************************************************************************************/
#if defined(CONFIG_MACH_SDM450_DD14)
	ret = gpio_direction_output(62, on_off);
	if (ret){
		pr_err("%s : fail to configure GPIO 62(ANX2_EN)\n",__func__);
	}
#else
	ret = gpio_direction_output(16, on_off);
	if (ret){
		pr_err("%s : fail to configure GPIO 16(ANX2_EN)\n",__func__);
	}
#endif
	ret = gpio_direction_output(47, on_off);
	if (ret){
		pr_err("%s : fail to configure GPIO 47(ANX2_RST)\n",__func__);
	}
	return ret;
}
EXPORT_SYMBOL(anx7625_power_control);
static void Reg_Access_Conflict_Workaround(uint8_t slave_addr)
{
	uint8_t offset;
	int ret = 0;

	if (slave_addr != last_access_DevAddr) {
		switch (slave_addr) {
		case  0x54:
		case  0x72:
		default:
			offset = 0x00;
			break;

		case  0x58:
			offset = 0x00;
			break;

		case  0x70:
			offset = 0xD1;
			break;

		case  0x7A:
			offset = 0x60;
			break;

		case  0x7E:
			offset = 0x39;
			break;

		case  0x84:
			offset = 0x7F;
			break;
		}


		anx_chip3->client->addr = (slave_addr >> 1);
		ret = i2c_smbus_write_byte_data(anx_chip3->client, offset, 0x00);
		if (ret < 0) {
			pr_err("%s %s: failed to write i2c addr=%x , %x\n:",
				LOG_TAG, __func__, slave_addr, offset);

		}
		last_access_DevAddr = slave_addr;
	}

}

int sp_read_reg_anx7625(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	Reg_Access_Conflict_Workaround(slave_addr);

	if (!anx_chip3)
		return -EINVAL;

	anx_chip3->client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx_chip3->client, offset);
	if (ret < 0) {
		pr_err("failed to read i2c addr=%x\n", slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	pr_err("[ANX7625] I2C Read(%x, %x): [%x]\n", slave_addr, offset, ret);

	return 0;
}

int sp_write_reg_anx7625(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	Reg_Access_Conflict_Workaround(slave_addr);

	if (!anx_chip3)
		return -EINVAL;

	anx_chip3->client->addr = (slave_addr >> 1);
	//pr_err("[ANX7625] I2C Write(%x, %x, %x)\n", slave_addr, offset, value);
	ret = i2c_smbus_write_byte_data(anx_chip3->client, offset, value);
	if (ret < 0) {
		pr_err("failed to write i2c addr=%x\n", slave_addr);
	}
	return ret;
}

void sp_tx_hardware_poweron3(void)
{
	if (!anx_chip3)
		return;
	ANX7625_POWER_STATE = 1;

	gpio_direction_output(anx_chip3->gpio_p_dwn, 1);
	msleep(50);
	gpio_direction_output(anx_chip3->gpio_reset, 1);
	msleep(20);

	pr_info("anx7625 power on\n");
}

void sp_tx_hardware_powerdown3(void)
{
//int status = 0;

	if (!anx_chip3)
		return;

	gpio_direction_output(anx_chip3->gpio_reset, 0);
	msleep(10);
	gpio_direction_output(anx_chip3->gpio_p_dwn, 0);
	msleep(10);

	/* turn off hpd */
	/*
	if (anx_chip3->hdmi_sp_ops->set_upstream_hpd) {
	status = anx_chip3->hdmi_sp_ops->set_upstream_hpd(
	anx_chip3->hdmi_pdev, 0);
	if (status)
	pr_err("failed to turn off hpd");
	}
	*/
	pr_info("anx7625 power down\n");
}

/*
static void sp_tx_power_down_and_init(void)
{
	vbus_power_ctrl3();
	sp_tx_power_down(SP_TX_PWR_REG);
	sp_tx_power_down(SP_TX_PWR_TOTAL);
	sp_tx_hardware_powerdown3();
	sp_tx_pd_mode3 = 1;
	sp_tx_link_config_done = 0;
	sp_tx_hw_lt_enable = 0;
	sp_tx_hw_lt_done = 0;
	sp_tx_rx_type3 = RX_NULL;
	sp_tx_rx_type3_backup = RX_NULL;
	sp_tx_set_sys_state(STATE_CABLE_PLUG);
}

*/



int slimport_read_edid_block3(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bEDID_firstblock3, sizeof(bEDID_firstblock3));
	} else if (block == 1) {
		memcpy(edid_buf, bEDID_extblock3, sizeof(bEDID_extblock3));
	} else {
		pr_err("%s: block number %d is invalid\n", __func__, block);
		return -EINVAL;
	}

	return 0;
}

EXPORT_SYMBOL(slimport_read_edid_block3);

int update_audio_format_setting3(unsigned char  bAudio_Fs, unsigned char bAudio_word_len, int Channel_Num, I2SLayOut layout)
{
	//pr_info("bAudio_Fs = %d, bAudio_word_len = %d, Channel_Num = %d, layout = %d\n", bAudio_Fs, bAudio_word_len, Channel_Num, layout); //liu
	SP_CTRL_AUDIO_FORMAT_Set3(AUDIO_I2S,bAudio_Fs ,bAudio_word_len);
	SP_CTRL_I2S_CONFIG_Set3(Channel_Num , layout);
	audio_format_change3=1;
	
	return 0;
}
EXPORT_SYMBOL(update_audio_format_setting3);

int hdcp_eanble_setting3(bool on)
{
	hdcp_enable3=on;
	return 0;
}
EXPORT_SYMBOL(hdcp_eanble_setting3);

unchar sp_get_rx_bw3(void)
{
	return sp_rx_bw3;
}
EXPORT_SYMBOL(sp_get_rx_bw3);

#if 0
static int anx7625_mipi_timing_setting(void *client, bool on,
		struct msm_dba_video_cfg *cfg, u32 flags)
{
	if (!cfg) {
		pr_err("%s: invalid input\n", __func__);
		return 1;
	}

	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_HTOTAL = cfg->h_active + cfg->h_front_porch +
	      cfg->h_pulse_width + cfg->h_back_porch;
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_VTOTAL= cfg->v_active + cfg->v_front_porch +
	      cfg->v_pulse_width + cfg->v_back_porch;

	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_HActive = cfg->h_active;
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_H_Sync_Width= cfg->h_pulse_width;
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_H_Front_Porch= cfg->h_front_porch;
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_H_Back_Porch= cfg->h_back_porch;

	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_VActive = cfg->v_active;
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_V_Sync_Width= cfg->v_pulse_width;
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_V_Front_Porch= cfg->v_front_porch;
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_V_Back_Porch= cfg->v_back_porch;

	mipi_lane_count3 = cfg->num_of_input_lanes;
	//mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_pixel_frequency=(unsigned int)(cfg->pclk3_khz/1000);
	mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_pixel_frequency =
		mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_HTOTAL *
		mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_VTOTAL * 60 / 1000;

	pr_info("cfg->pclk3_khz = %d\n", cfg->pclk3_khz);

	pr_info("h_total = %d, h_active = %d, hfp = %d, hpw = %d, hbp = %d\n",
		mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_HTOTAL, cfg->h_active, cfg->h_front_porch,
		cfg->h_pulse_width, cfg->h_back_porch);

	pr_info("v_total = %d, v_active = %d, vfp = %d, vpw = %d, vbp = %d\n",
		mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_VTOTAL, cfg->v_active, cfg->v_front_porch,
		cfg->v_pulse_width, cfg->v_back_porch);

	pr_info("pixel clock = %lu, lane count = %d, \n", mipi_video_timing_table3[bMIPIFormatIndex3].MIPI_pixel_frequency, cfg->num_of_input_lanes);

	video_format_change3 = 1;
	SP_CTRL_Set_System_State3(SP_TX_CONFIG_VIDEO_INPUT);

	return 0;
}
#endif
bool slimport_is_connected2(void)
{
	bool result = false;

	if (!anx_chip3)
		return false;
/*
	if (gpio_get_value_cansleep(anx_chip3->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(anx_chip3->gpio_cbl_det)) {
			pr_info("slimport cable is detected\n");
			result = true;
		}
	}
*/
	return result;
}
EXPORT_SYMBOL(slimport_is_connected2);


static void anx7625_free_gpio(struct anx7625_data *anx7625)
{
	//gpio_free(anx7625->gpio_cbl_det);
	//gpio_free(anx7625->gpio_int);
	gpio_free(anx7625->gpio_reset);
	gpio_free(anx7625->gpio_p_dwn);
#if defined(CONFIG_MACH_SDM450_DD14) || defined(CONFIG_MACH_SDM450_WIDEPD)
	gpio_free(anx7625->gpio_mux_sw_sel);
	gpio_free(anx7625->gpio_mux_sw_oe);
	gpio_free(anx7625->gpio_lcd_3v3_buck);
#endif
}

static int anx7625_pinctrl_configure(struct pinctrl *key_pinctrl, bool active)
{
	struct pinctrl_state *set_state;
	int retval;

	if (active) {
		set_state = pinctrl_lookup_state(key_pinctrl, "pmx_anx_int_active");
		if (IS_ERR(set_state)) {
			pr_err("%s: cannot get anx7625 pinctrl active state\n", __func__);
			return PTR_ERR(set_state);
		}
	}
	else {
		/* suspend setting here */
	}
	retval = pinctrl_select_state(key_pinctrl, set_state);
	if (retval) {
		pr_err("%s: cannot set anx7625 pinctrl state\n", __func__);
		return retval;
	}

	pr_info("%s: configure pinctrl success\n", __func__);
	return 0;
}
#if defined(CONFIG_MACH_SDM450_DD14) || defined(CONFIG_MACH_SDM450_WIDEPD)
/****************************************************************************************
2020.02.14
Added func : anx7625_backlight_control
Issue : When USB DP cable is inserted or removed , the screen is broken bacasue SD450 lose
       display control from ANX7580.
fix : During mipi switching time , turn off the backlight and then restore it.
****************************************************************************************/
static int is_first_boot_skip_backlightctrl = 1;
void anx7625_backlight_control(int onoff)
{
	int ret = 0;
	pr_err("anx7625_backlight_control is set  to [%d]\n", onoff);

	ret = gpio_direction_output(0, onoff);
	if (ret){
		pr_err("%s : fail to configure GPIO 62(LED_EN)\n",__func__);
	}
	ret = gpio_direction_output(62, onoff);
	if (ret){
		pr_err("%s : fail to configure GPIO 62(LED_BOOST_EN)\n",__func__);
	}
}
/****************************************************************************************
  SEL     LOW     HIGH    
  OE      LOW     LOW  
  		===============
  		  USB     WIFI
 *****************************************************************************************/
int anx7625_set_mux_path(int path)
{
	int ret = 0;
	if(!anx_chip3)
	{
		pr_err("anx_chip3 is null , failed to set mux_path!!\n");
		return -1;
	}

	pr_err("anx7625_set_mux_path is called and call ATE_pattern7625!!!\n");

	if(!is_first_boot_skip_backlightctrl)
		anx7625_backlight_control(GPIOF_OUT_INIT_LOW);

	if(path == ANX7625_WIFI)
	{
		pr_err("anx7625 mipi path is set to  wifi path\n");
		WIFI_PATH = 1;
		gpio_direction_output(anx_chip3->gpio_mux_sw_sel, GPIOF_OUT_INIT_HIGH);
		gpio_direction_output(anx_chip3->gpio_mux_sw_oe, GPIOF_OUT_INIT_LOW);
		anx7580_power_control(1);
		msleep(100);
		ATE_pattern7580();
		ATE_pattern7625();
		msleep(10);
	}
	else	//USB
	{
		pr_err("anx7625 mipi path is set to USB DP path\n");
		WIFI_PATH = 0;
		ret = gpio_direction_output(102, 0);
		if (ret){
			pr_err("%s : fail to configure GPIO 102(ANX7580_EN)\n",__func__);
		}
		ret = gpio_direction_output(101, 0);
		if (ret){
			pr_err("%s : fail to configure GPIO 101(ANX7580_RST_N)\n",__func__);
		}
		gpio_direction_output(anx_chip3->gpio_mux_sw_sel, GPIOF_OUT_INIT_LOW);
		gpio_direction_output(anx_chip3->gpio_mux_sw_oe, GPIOF_OUT_INIT_LOW);
		anx7580_power_control(1);
		msleep(100);
		ATE_pattern7580();
		ATE_pattern7625();
		msleep(10);

	}

	if(!is_first_boot_skip_backlightctrl)
		anx7625_backlight_control(GPIOF_OUT_INIT_HIGH);

	is_first_boot_skip_backlightctrl = 0;

	return 0;
}
EXPORT_SYMBOL(anx7625_set_mux_path);
#endif


static int anx7625_init_gpio(struct anx7625_data *anx7625)
{
	int ret = 0;
	struct pinctrl *key_pinctrl;
	
	/* Get pinctrl if target uses pinctrl */
	key_pinctrl = devm_pinctrl_get(&anx7625->client->dev);
	if (IS_ERR(key_pinctrl)) {
		if (PTR_ERR(key_pinctrl) == -EPROBE_DEFER)
			return -EPROBE_DEFER;

		pr_debug("Target does not use pinctrl\n");
		key_pinctrl = NULL;
	}
	if (key_pinctrl) {
		pr_debug("Target uses pinctrl\n");
		ret = anx7625_pinctrl_configure(key_pinctrl, true);
		if (ret)
			pr_err("%s: cannot configure anx_int pinctrl\n", __func__);
	}

	ret = gpio_request_one(anx7625->gpio_p_dwn,
	                       GPIOF_OUT_INIT_HIGH, "anx_p_dwn_ctl");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7625->gpio_p_dwn);
		goto out;
	}

	ret = gpio_request_one(anx7625->gpio_reset,
	                       GPIOF_OUT_INIT_LOW, "anx7625_reset_n");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7625->gpio_reset);
		goto err0;
	}
/*
	ret = gpio_request_one(anx7625->gpio_int,
	                       GPIOF_IN, "anx7625_int_n");

	if (ret) {
		pr_err("failed to request gpio %d\n", anx7625->gpio_int);
		goto err1;
	}

	ret = gpio_request_one(anx7625->gpio_cbl_det,
	                       GPIOF_IN, "anx7625_cbl_det");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7625->gpio_cbl_det);
		goto err2;
	}*/
	//gpio_direction_input(anx7625->gpio_cbl_det);

#if defined(CONFIG_MACH_SDM450_DD14) || defined(CONFIG_MACH_SDM450_WIDEPD)
	ret = gpio_request_one(anx7625->gpio_mux_sw_sel,
	                       GPIOF_OUT_INIT_HIGH, "anx7625_mux_sw_sel");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7625->gpio_mux_sw_sel);
		goto err3;
	}
	ret = gpio_request_one(anx7625->gpio_mux_sw_oe,
			GPIOF_OUT_INIT_LOW, "anx7625_mux_sw_oe");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7625->gpio_mux_sw_oe);
		goto err4;
	}
#ifdef CONFIG_MACH_SDM450_WIDEPD
	ret = gpio_request_one(anx7625->gpio_lcd_3v3_buck,
			GPIOF_OUT_INIT_HIGH, "gpio_lcd_3v3_buck");
#else
	ret = gpio_request_one(anx7625->gpio_lcd_3v3_buck,
			GPIOF_OUT_INIT_LOW, "gpio_lcd_3v3_buck");
#endif
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7625->gpio_lcd_3v3_buck);
		goto err5;
	}
#endif

	gpio_direction_output(anx7625->gpio_reset, 0);
	gpio_direction_output(anx7625->gpio_p_dwn, 1);

	goto out;
#if defined(CONFIG_MACH_SDM450_DD14) || defined(CONFIG_MACH_SDM450_WIDEPD)
err5:
	gpio_free(anx7625->gpio_mux_sw_oe);
err4:
	gpio_free(anx7625->gpio_mux_sw_sel);
err3:
	gpio_free(anx7625->gpio_reset);
#endif
err0:
	gpio_free(anx7625->gpio_p_dwn);
out:
	return ret;
}

static int anx7625_system_init(void)
{
	int ret = 0;

	ret = SP_CTRL_Chip_Detect3();
	if (ret == 0) {
		pr_err("failed to detect anx7625\n");
		return -ENODEV;
	}

	SP_CTRL_Chip_Initial3();
	return 0;
}

#if 0
static irqreturn_t anx7625_cbl_det_isr(int irq, void *data)
{
	struct anx7625_data *anx7625 = data;
	int status;

	if (gpio_get_value(anx7625->gpio_cbl_det)) {
		wake_lock(&anx7625->slimport_lock);
		pr_info("detect cable insertion\n");
		queue_delayed_work(anx7625->workqueue, &anx7625->work, 0);
	} else {		
		/* check HPD state again after 5 ms to see if it is HPD irq event */
		#ifdef Standard_DP
		mdelay(5);

		if (!gpio_get_value(anx7625->gpio_cbl_det)) { // if it is one IRQ, should not destroy ANX7625 work queue
		#endif
			
		pr_info("detect cable removal\n");
		status = cancel_delayed_work_sync(&anx7625->work);
		if (status == 0)
			flush_workqueue(anx7625->workqueue);
		//when HPD low, power down ANX7625
		if(sp_tx_pd_mode3==0)
		{
			SP_CTRL_Set_System_State3(SP_TX_WAIT_SLIMPORT_PLUGIN);
			system_power_ctrl3(0);
		}
		
		wake_unlock(&anx7625->slimport_lock);
		wake_lock_timeout(&anx7625->slimport_lock, 2*HZ);



			/* Notify DBA framework disconnect event */
			anx7625_notify_clients(&anx7625->dev_info,
				MSM_DBA_CB_HPD_DISCONNECT);

			/* clear notify_control */
			notify_control = 0;

			/* clear EDID_ready3 */
			EDID_ready3 = 0;
		#ifdef Standard_DP
		}
		#endif
	}
	return IRQ_HANDLED;
}
#endif
static void anx7625_work_func(struct work_struct *work)
{
#ifndef EYE_TEST
	struct anx7625_data *td = container_of(work, struct anx7625_data,
	                                       work.work);
	int workqueu_timer = 0;
	if(get_system_state3() >= SP_TX_PLAY_BACK)
		workqueu_timer = 500;
	else
		workqueu_timer = 100;

	SP_CTRL_Main_Procss3();
	queue_delayed_work(td->workqueue, &td->work,
	                   msecs_to_jiffies(workqueu_timer));

	if (!notify_control && EDID_ready3 && slimport_is_connected2()) {
		/* Notify DBA framework connect event */
		anx7625_notify_clients(&td->dev_info,
				MSM_DBA_CB_HPD_CONNECT);

		notify_control = 1;
	}
#endif
}

static int anx7625_parse_dt(struct device_node *node,
                            struct anx7625_data *anx7625)
{
	int ret = 0;
	//struct platform_device *hdmi_pdev = NULL;
	//struct device_node *hdmi_tx_node = NULL;

	anx7625->gpio_p_dwn =
	    of_get_named_gpio(node, "analogix,p-dwn-gpio", 0);
	if (anx7625->gpio_p_dwn < 0) {
		pr_err("failed to get analogix,p-dwn-gpio.\n");
		ret = anx7625->gpio_p_dwn;
		goto out;
	}

	anx7625->gpio_reset =
	    of_get_named_gpio(node, "analogix,reset-gpio", 0);
	if (anx7625->gpio_reset < 0) {
		pr_err("failed to get analogix,reset-gpio.\n");
		ret = anx7625->gpio_reset;
		goto out;
	}
/*
	anx7625->gpio_int =
	    of_get_named_gpio(node, "analogix,irq-gpio", 0);
	if (anx7625->gpio_int < 0) {
		pr_err("failed to get analogix,irq-gpio.\n");
		ret = anx7625->gpio_int;
		goto out;
	}

	anx7625->gpio_cbl_det =
	    of_get_named_gpio(node, "analogix,cbl-det-gpio", 0);
	if (anx7625->gpio_cbl_det < 0) {
		pr_err("failed to get analogix,cbl-det-gpio.\n");
		ret = anx7625->gpio_cbl_det;
		goto out;
	}
*/
	ret = of_property_read_string(node, "analogix,vdd10-name",
	                              &anx7625->vdd10_name);
	if (ret) {
		pr_err("failed to get vdd10-name.\n");
		goto out;
	}

	ret = of_property_read_string(node, "analogix,vdd18-name",
	                              &anx7625->vdd18_name);
	if (ret) {
		pr_err("failed to get vdd18-name.\n");
		goto out;
	}

	ret = of_property_read_string(node, "analogix,avdd33-name",
	                              &anx7625->avdd33_name);
	if (ret) {
		pr_err("failed to get avdd33-name.\n");
		goto out;
	}

#ifdef CONFIG_MACH_SDM450_WIDEPD
	ret = of_property_read_string(node, "analogix,i2c_vdd18-name",
	                              &anx7625->i2c_vdd18_name);
	if (ret) {
		pr_err("failed to get avdd33-name.\n");
		goto out;
	}

	ret = of_property_read_string(node, "analogix,level_shifter-name",
	                              &anx7625->level_shifter_name);
	if (ret) {
		pr_err("failed to get level_shifter-name.\n");
		goto out;
	}
#endif
	/*
	hdmi_pdev = of_find_device_by_node(hdmi_tx_node);
	if (!hdmi_pdev) {
	pr_err("can't find the deivce by node\n");
	ret = -EINVAL;
	goto out;
	}
	anx7625->hdmi_pdev = hdmi_pdev;
	*/
#if defined(CONFIG_MACH_SDM450_DD14) || defined(CONFIG_MACH_SDM450_WIDEPD)
	anx7625->gpio_mux_sw_sel =
		of_get_named_gpio(node, "analogix,mux-mipi_sw_sel", 0);
	if (anx7625->gpio_mux_sw_sel < 0) {
		pr_err("failed to get analogix,.\n");
		ret = anx7625->gpio_mux_sw_sel;
		goto out;
	}

	anx7625->gpio_mux_sw_oe =
		of_get_named_gpio(node, "analogix,mux-mipi_sw_oe", 0);
	if (anx7625->gpio_mux_sw_oe < 0) {
		pr_err("failed to get analogix,.\n");
		ret = anx7625->gpio_mux_sw_oe;
		goto out;
	}
	anx7625->gpio_lcd_3v3_buck =
		of_get_named_gpio(node, "gpio_lcd_3v3_buck", 0);
		if (anx7625->gpio_lcd_3v3_buck < 0) {
			pr_err("failed to get analogix, gpio[%d]\n" , anx7625->gpio_lcd_3v3_buck);
			ret = anx7625->gpio_lcd_3v3_buck;
			goto out;
		}
#endif
out:
	return ret;
}

#if 0
static int anx7625_get_raw_edid(void *client,
	u32 size, char *buf, u32 flags)
{
	struct anx7625_data *pdata =
		anx7625_get_platform_data(client);

	if (!pdata || !buf) {
		pr_err("%s: invalid data\n", __func__);
		goto end;
	}

	mutex_lock(&pdata->lock);

	pr_info("%s: size=%d\n", __func__, size);
	size = min_t(u32, size, sizeof(bEDID_twoblock3));

	pr_info("%s: memcpy EDID block, size=%d\n", __func__, size);
	memcpy(buf, bEDID_twoblock3, size);
:q

	mutex_unlock(&pdata->lock);
end:
	return 0;
}


static int anx7625_register_dba(struct anx7625_data *pdata)
{
	struct msm_dba_ops *client_ops;
	struct msm_dba_device_ops *dev_ops;

	if (!pdata)
		return -EINVAL;

	client_ops = &pdata->dev_info.client_ops;
	dev_ops = &pdata->dev_info.dev_ops;

	client_ops->power_on        = NULL;
	client_ops->video_on        = anx7625_mipi_timing_setting;
	client_ops->configure_audio = NULL;
	client_ops->hdcp_enable3     = NULL;
	client_ops->hdmi_cec_on     = NULL;
	client_ops->hdmi_cec_write  = NULL;
	client_ops->hdmi_cec_read   = NULL;
	client_ops->get_edid_size   = NULL;
	client_ops->get_raw_edid    = anx7625_get_raw_edid;
	client_ops->check_hpd	    = NULL;

	strlcpy(pdata->dev_info.chip_name, "anx7625",
		sizeof(pdata->dev_info.chip_name));

	pdata->dev_info.instance_id = 0;

	mutex_init(&pdata->dev_info.dev_mutex);

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	return msm_dba_add_probed_device(&pdata->dev_info);
}
#endif

static int anx7625_i2c_probe(struct i2c_client *client,
                             const struct i2c_device_id *id)
{
	struct anx7625_data *anx7625;
	struct anx7625_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	//struct msm_hdmi_sp_ops *hdmi_sp_ops = NULL;
	int ret = 0;

	pr_info("%s++\n", __func__);

	if (!i2c_check_functionality(client->adapter,
	                             I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("i2c bus does not support anx7625\n");
		ret = -ENODEV;
		goto exit;
	}

	pr_info("%s: i2c device name=%s, addr=0x%x, adapter nr=%d\n", __func__,
			client->name, client->addr, client->adapter->nr);

	anx7625 = kzalloc(sizeof(struct anx7625_data), GFP_KERNEL);
	if (!anx7625) {
		pr_err("failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit;
	}

	anx7625->client = client;
	i2c_set_clientdata(client, anx7625);

	if (dev_node) {
		ret = anx7625_parse_dt(dev_node, anx7625);
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

		anx7625->gpio_p_dwn = pdata->gpio_p_dwn;
		anx7625->gpio_reset = pdata->gpio_reset;
		//anx7625->gpio_int = pdata->gpio_int;
		//anx7625->gpio_cbl_det = pdata->gpio_cbl_det;
		anx7625->vdd10_name = pdata->vdd10_name;
		anx7625->vdd18_name = pdata->vdd18_name;
		anx7625->avdd33_name = pdata->avdd33_name;
#ifdef CONFIG_MACH_SDM450_WIDEPD
		anx7625->i2c_vdd18_name = pdata->i2c_vdd18_name;
		anx7625->level_shifter_name = pdata->level_shifter_name;
#endif
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
	
	if (anx7625->hdmi_pdev) {
	ret = msm_hdmi_register_sp(anx7625->hdmi_pdev,
	hdmi_sp_ops);
	if (ret) {
	pr_err("register with hdmi_failed\n");
	goto err0;
	}
	}
	
	anx7625->hdmi_sp_ops = hdmi_sp_ops;
*/
	anx_chip3 = anx7625;

	mutex_init(&anx7625->lock);
	init_completion(&init_aux_ch_completion3);
	ret = anx7625_init_gpio(anx7625);
	if (ret) {
		pr_err("failed to initialize gpio\n");
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7625->work, anx7625_work_func);

	anx7625->workqueue = create_singlethread_workqueue("anx7625_work");
	if (!anx7625->workqueue) {
		pr_err("failed to create work queue\n");
		ret = -ENOMEM;
		goto err1;
	}
	ret = anx7625_system_init();
	if (ret) {
		pr_err("failed to initialize anx7625\n");
		goto err1;
	}
#if 0
	client->irq = gpio_to_irq(anx7625->gpio_cbl_det);
	if (client->irq < 0) {
		pr_err("failed to get gpio irq\n");
		goto err5;
	}

	wake_lock_init(&anx7625->slimport_lock, WAKE_LOCK_SUSPEND,
	               "slimport_wake_lock");

	ret = request_threaded_irq(client->irq, NULL, anx7625_cbl_det_isr,
	                           IRQF_TRIGGER_RISING
	                           | IRQF_TRIGGER_FALLING
	                           | IRQF_ONESHOT,
	                           "anx7625", anx7625);
	if (ret < 0) {
		pr_err("failed to request irq\n");
		goto err6;
	}

	ret = enable_irq_wake(client->irq);
	if (ret < 0) {
		pr_err("interrupt wake enable fail\n");
		goto err7;
	}

	/* Register msm dba device */
	ret = anx7625_register_dba(anx7625);
	if (ret) {
		pr_err("%s: Error registering with DBA %d\n",
			__func__, ret);
	}
#endif
#if defined(CONFIG_MACH_SDM450_DD14) || defined(CONFIG_MACH_SDM450_WIDEPD)
	anx7625_set_mux_path(ANX7625_WIFI);
#endif
	pr_info("%s succeed!\n", __func__);

	goto exit;
#if 0
err7:
	free_irq(client->irq, anx7625);
err6:
	wake_lock_destroy(&anx7625->slimport_lock);
#endif
#if 0
err5:
	if (!anx7625->vdd_reg)
		regulator_put(anx7625->vdd_reg);
err4:
	if (!anx7625->vdd18_reg)
		regulator_put(anx7625->vdd18_reg);
err3:
	if (!anx7625->avdd_reg)
		regulator_put(anx7625->avdd_reg);
err2:
	destroy_workqueue(anx7625->workqueue);
#endif
err1:
	anx7625_free_gpio(anx7625);
err0:
	anx_chip3 = NULL;
	kfree(anx7625);
exit:
	pr_info("%s--\n", __func__);
	return ret;
}

static int anx7625_i2c_remove(struct i2c_client *client)
{
	struct anx7625_data *anx7625 = i2c_get_clientdata(client);

	free_irq(client->irq, anx7625);
	wake_lock_destroy(&anx7625->slimport_lock);
	if (!anx7625->vdd_reg)
		regulator_put(anx7625->vdd_reg);
	if (!anx7625->vdd18_reg)
		regulator_put(anx7625->vdd18_reg);
	if (!anx7625->avdd_reg)
		regulator_put(anx7625->avdd_reg);
	destroy_workqueue(anx7625->workqueue);
	anx7625_free_gpio(anx7625);
	anx_chip3 = NULL;
	kfree(anx7625);
	return 0;
}

static const struct i2c_device_id anx7625_id[] = {
	{ "anx7625", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, anx7625_id);

static struct of_device_id anx_match_table[] = {
	{ .compatible = "analogix,anx7625",},
	{ },
};

static struct i2c_driver anx7625_driver = {
	.driver = {
		.name = "anx7625",
		.owner = THIS_MODULE,
		.of_match_table = anx_match_table,
	},
	.probe = anx7625_i2c_probe,
	.remove = anx7625_i2c_remove,
	.id_table = anx7625_id,
};

static void __init anx7625_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = i2c_add_driver(&anx7625_driver);
	if (ret)
		pr_err("%s: failed to register anx7625 driver\n", __func__);
}

static int __init anx7625_init(void)
{
	pr_info("%s\n", __func__);
	async_schedule(anx7625_init_async, NULL);
	return 0;
}

static void __exit anx7625_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&anx7625_driver);
}

module_init(anx7625_init);
module_exit(anx7625_exit);

MODULE_DESCRIPTION("Slimport transmitter ANX7625 driver");
MODULE_AUTHOR("swang@analogixsemi.com");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1");
