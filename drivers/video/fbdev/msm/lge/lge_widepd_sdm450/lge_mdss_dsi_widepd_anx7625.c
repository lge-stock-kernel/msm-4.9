#include <linux/delay.h>
#include "mdss_dsi.h"

#include <linux/input/lge_touch_notify_oos.h>
#define GPIO_LCD_3V3_BUCK 20

extern bool flag_panel_deep_sleep_ctrl;
extern bool flag_panel_deep_sleep_status;

extern int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata, bool active);

typedef unsigned char BYTE;

//extern BYTE ATE_pattern_off(void);
extern BYTE ATE_pattern7625(void);
extern BYTE ATE_pattern7580(void);
extern int anx7625_power_control(int on_off);
extern int anx7580_power_control(int on_off);

static int first_boot = 1; 
int anx7625_widepd_mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: + (override: widepd)\n", __func__);
#if 0
	if (pdata->panel_info.cont_splash_enabled) {
		/*
		 * Vote for vreg due to unbalanced regulator disable
		 */
		ret = msm_mdss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 1);
		if (ret) {
			pr_err("%s: failed to enable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
			return ret;
		}
	} else if (lge_mdss_dsi_panel_power_seq_all()) {
		ret = msm_mdss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 1);
		if (ret) {
			pr_err("%s: failed to enable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
			return ret;
		}
		usleep_range(12000, 12000);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
		lge_extra_gpio_set_value(ctrl_pdata, "dsv_vsp_en", 1);
		usleep_range(5000, 5000);
		lge_extra_gpio_set_value(ctrl_pdata, "dsv_vsn_en", 1);
		usleep_range(2000, 2000);
		ext_dsv_mode_change(POWER_ON_1);
#endif
	}
#endif
	/*
	 * If continuous splash screen feature is enabled, then we need to
	 * request all the GPIOs that have already been configured in the
	 * bootloader. This needs to be done irresepective of whether
	 * the lp11_init flag is set or not.
	 */
	if (pdata->panel_info.cont_splash_enabled ||
		!pdata->panel_info.mipi.lp11_init) {
		if (mdss_dsi_pinctrl_set_state(ctrl_pdata, true))
			pr_debug("reset enable: pinctrl not enabled\n");

		ret = mdss_dsi_panel_reset(pdata, 1);
		if (ret)
			pr_err("%s: Panel reset failed. rc=%d\n",
					__func__, ret);
	}

	if(first_boot)
	{
		first_boot = 0;
	}
	else
	{
		ret = gpio_direction_output(GPIO_LCD_3V3_BUCK, 1);//gpio 20 --> 3.3 buck
		
		if(ret)
			pr_err("%s : fail to set gpio[%d] to high\n", __func__,GPIO_LCD_3V3_BUCK);
/********************************************************************************************************
		ANX7580 power on and ATE_Pattern called by duhan.kim  20200206
		history : MACBOOK DP connection fail  / configration requested by MACBOOK before ANX7580 standby
*********************************************************************************************************/
		usleep_range(2000, 2000); //2ms
		anx7580_power_control(1);  //0ms
		usleep_range(200000, 200000); //200ms
		ATE_pattern7580(); //340ms  ==> 3sec
				
		anx7625_power_control(1);
		usleep_range(200000, 200000);
		ATE_pattern7625(); //link training
		usleep_range(10000, 10000);
		lge_extra_gpio_set_value(ctrl_pdata, "led_boost_en", 1);//8v boost
		usleep_range(200000, 200000);
		lge_extra_gpio_set_value(ctrl_pdata, "led_en", 1); //gpio 0 led en && backlight on
		usleep_range(2000, 2000);
	}	
	pr_info("%s: -\n", __func__);
	return ret;
}

int anx7625_widepd_mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: + (override: widepd)\n", __func__);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
	if (!ctrl_pdata->lge_extra.lp11_off) {
		ret = mdss_dsi_panel_reset(pdata, 0);
	}
#else
	ret = mdss_dsi_panel_reset(pdata, 0);
#endif

	if (ret) {
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	lge_extra_gpio_set_value(ctrl_pdata, "led_en", 0);//gpio 0  && backlight off
	usleep_range(10000, 10000);
	lge_extra_gpio_set_value(ctrl_pdata, "led_boost_en", 0);//8v boost
    usleep_range(200000, 200000);
	anx7625_power_control(0);
	usleep_range(200000, 200000);
	anx7580_power_control(0);
	ret = gpio_direction_output(GPIO_LCD_3V3_BUCK, 0);//3.3v buck
	if(ret)
		pr_err("%s : fail to set gpio[%d] low\n", __func__, GPIO_LCD_3V3_BUCK);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
	if (lge_mdss_dsi_panel_power_seq_all()) {
		if (ret)
			pr_err("%s: failed to disable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

		ext_dsv_mode_change(POWER_OFF);
		lge_extra_gpio_set_value(ctrl_pdata, "dsv_vsn_en", 0);
		usleep_range(2000, 2000);
		lge_extra_gpio_set_value(ctrl_pdata, "dsv_vsp_en", 0);
		usleep_range(2000, 2000);

		ret = msm_mdss_enable_vreg(
			ctrl_pdata->panel_power_data.vreg_config,
			ctrl_pdata->panel_power_data.num_vreg, 0);
		if (ret)
			pr_err("%s: failed to disable vregs for %s\n",
				__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
	}
#endif


	pr_info("%s: -\n", __func__);
	return ret;
}

void anx7625_widepd_panel_enter_deep_sleep(void)
{
	struct mdss_dsi_ctrl_pdata *pdata = NULL;

	if (flag_panel_deep_sleep_ctrl) {
		pdata = lge_mdss_dsi_get_ctrl_pdata();
		if (pdata == NULL)
			return;

		gpio_set_value((pdata->rst_gpio), 0);
		usleep_range(2000, 2000);

		lge_extra_gpio_set_value(pdata, "led_boost_en", 0);
		usleep_range(5000, 5000);
		gpio_direction_output(GPIO_LCD_3V3_BUCK, 0);
		usleep_range(5000, 5000);
                lge_extra_gpio_set_value(pdata, "led_en", 0);
                usleep_range(2000, 2000);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
		ext_dsv_mode_change(POWER_OFF);
		lge_extra_gpio_set_value(pdata, "dsv_vsn_en", 0);
		usleep_range(5000, 5000);
		lge_extra_gpio_set_value(pdata, "dsv_vsp_en", 0);
		usleep_range(5000, 5000);
#endif
		flag_panel_deep_sleep_status = true;
		pr_info("%s done \n", __func__);
	}
}

void anx7625_widepd_panel_exit_deep_sleep(void)
{
	struct mdss_dsi_ctrl_pdata *pdata = NULL;

	if (flag_panel_deep_sleep_ctrl) {
		pdata = lge_mdss_dsi_get_ctrl_pdata();
		if (pdata == NULL)
			return;

		mdss_dsi_clk_ctrl(pdata, pdata->dsi_clk_handle, MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
		mdss_dsi_sw_reset(pdata, true);

		usleep_range(12000, 12000);

		lge_extra_gpio_set_value(pdata, "led_boost_en", 1);
		usleep_range(2000, 2000);
		gpio_direction_output(GPIO_LCD_3V3_BUCK, 1);
		usleep_range(2000, 2000);
                lge_extra_gpio_set_value(pdata, "led_en", 1);
                usleep_range(2000, 2000);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
		lge_extra_gpio_set_value(pdata, "dsv_vsp_en", 1);
		usleep_range(12000, 12000);
		lge_extra_gpio_set_value(pdata, "dsv_vsn_en", 1);
		usleep_range(2000, 2000);
		ext_dsv_mode_change(POWER_ON_1);
#endif
		usleep_range(2000, 2000);

		gpio_set_value((pdata->rst_gpio), 1);
		usleep_range(1000, 1000);

		lge_extra_gpio_set_value(pdata, "touch_reset", 1);
		usleep_range(6000, 6000);

		lge_mdss_dsi_panel_extra_cmds_send(pdata, "lpwg-on");

		mdss_dsi_clk_ctrl(pdata, pdata->dsi_clk_handle, MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);

		flag_panel_deep_sleep_status = false;

		pr_info("%s done \n", __func__);
	}
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
extern int mdss_dsi_set_clk_src(struct mdss_dsi_ctrl_pdata *ctrl);
void anx7625_widepd_mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);
	int ret = 0;

	if (!ctrl_pdata) {
		pr_err("%s: no driver data\n", __func__);
		return;
	}

	ret += mdss_dsi_set_clk_src(ctrl_pdata);
	ret += mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
			MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);
	if (ret) {
		pr_err("%s: could fail to set LP11\n", __func__);
	}
	usleep_range(5000, 5000);

	gpio_set_value((ctrl_pdata->rst_gpio), 0);

	ret += mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
		MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	if (ret) {
		pr_err("%s: could fail to set LP00\n", __func__);
	}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
	ext_dsv_mode_change(POWER_OFF);
	lge_extra_gpio_set_value(ctrl_pdata, "dsv_vsn_en", 0);
	usleep_range(5000, 5000);
	lge_extra_gpio_set_value(ctrl_pdata, "dsv_vsp_en", 0);
	usleep_range(5000, 5000);
#endif
	lge_extra_gpio_set_value(ctrl_pdata, "led_boost_en", 0);
    usleep_range(5000, 5000);
	gpio_direction_output(GPIO_LCD_3V3_BUCK, 0);
        usleep_range(5000, 5000);
        lge_extra_gpio_set_value(ctrl_pdata, "led_en", 0);
        usleep_range(5000, 5000);

	ret = msm_mdss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

	pr_info("%s: panel shutdown done \n", __func__);

	return;
}
#endif

int anx7625_widepd_mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return rc;
	}

	pr_info("%s: + enable = %d (override: widepd)\n", __func__, enable);

	if (enable) {
		if (!pinfo->cont_splash_enabled) {
			touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_START, NULL);
			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);
				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
			}
		}

		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		if (lge_mdss_dsi_panel_power_seq_all()) {
			touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_START, NULL);
			usleep_range(2000, 2000);

			gpio_set_value((ctrl_pdata->rst_gpio), enable);
			usleep_range(2000, 2000);
		}
	}
	pr_info("%s: -\n", __func__);

	return rc;
}
