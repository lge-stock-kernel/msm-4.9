#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/delay.h>
#include "mdss_dsi.h"
#include <linux/msm_lcd_power_mode.h>
#include <linux/input/lge_touch_notify.h>

#define EXT_DSV_PRIVILEGED
#include <linux/mfd/external_dsv.h>

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
extern int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
					bool active);
#endif

enum {
	LCD_MODE_U0 = 0,
	LCD_MODE_U2_UNBLANK,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
};

#define TOVIS_TD4310 0
#define CDOT_TD4310 1

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_RESET)
static int request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;
#if defined(CONFIG_PXLW_IRIS3)
	if (gpio_is_valid(ctrl_pdata->abyp_gpio)) {
		rc = gpio_request(ctrl_pdata->abyp_gpio, "analog_bypass");
		if (rc) {
			pr_err("request analog bypass gpio failed,rc=%d\n", rc);
		}
	}
	if (gpio_is_valid(ctrl_pdata->iris_rst_gpio)) {
		rc = gpio_request(ctrl_pdata->iris_rst_gpio, "iris_reset");
		if (rc) {
			pr_err("request iris reset gpio failed,rc=%d\n", rc);
			if (gpio_is_valid(ctrl_pdata->abyp_gpio))
				gpio_free(ctrl_pdata->abyp_gpio);
		}
	}
#endif
	return rc;
}

static inline void panel_reset_enable(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int i = 0;
	struct mdss_panel_info *pinfo = NULL;
	if (ctrl_pdata == NULL) {
		pr_err("Invalid dsi ctrl pdata\n");
		return;
	}
	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if (pinfo == NULL) {
		pr_err("Invalid pinfo data\n");
		return;
	}
	touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_START, NULL);
	for (i = 0; i < pinfo->rst_seq_len; ++i) {
		gpio_set_value((ctrl_pdata->rst_gpio),
			pinfo->rst_seq[i]);

		if (pinfo->rst_seq[++i])
			usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
	}
	touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_END, NULL);
	pr_info("reset sequence done \n");
}

static inline void panel_reset_disable(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_START, NULL);
	gpio_set_value((ctrl_pdata->rst_gpio), 0);
}

static void panel_reset_cdot(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int enable, bool seq_all)
{
	if (enable) {
		panel_reset_enable(ctrl_pdata);
		ext_dsv_chip_enable(1);
		ext_dsv_mode_change(POWER_ON);
		usleep_range(5000, 5000);
	} else {
		if (seq_all) {
			panel_reset_disable(ctrl_pdata);
			usleep_range(5000, 5000);
			ext_dsv_mode_change(POWER_OFF);
			ext_dsv_chip_enable(0);
		}
	}
}

static void panel_reset_tovis(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int enable, bool seq_all)
{
	if (enable) {
		ext_dsv_chip_enable(1);
		ext_dsv_mode_change(POWER_ON_1);
		panel_reset_enable(ctrl_pdata);
		usleep_range(10000, 10000);
		ext_dsv_mode_change(POWER_ON_2);
		usleep_range(10000, 10000);
	} else {
		if (seq_all) {
			panel_reset_disable(ctrl_pdata);
			usleep_range(5000, 5000);
			ext_dsv_mode_change(POWER_ON_1);
			usleep_range(10000, 10000);
			ext_dsv_mode_change(POWER_OFF);
			usleep_range(10000, 10000);
			ext_dsv_chip_enable(0);
		}
	}
}

static void panel_reset(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int enable, bool seq_all)
{
	if (ctrl_pdata->lge_extra.panel_id == CDOT_TD4310) {
		panel_reset_cdot(ctrl_pdata, enable, seq_all);
	} else {
		panel_reset_tovis(ctrl_pdata, enable, seq_all);
	}
}

#ifndef CONFIG_PXLW_IRIS3
DEFINE_SPINLOCK(iris_lock);
static void iris_send_one_wired_cmd(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int cmd)
{
	int i = 0;
	unsigned long flags = 0;
	int iris_wakeup_gpio = lge_extra_find_gpio_by_name(ctrl_pdata, "iris-wakeup");

	if (gpio_is_valid(iris_wakeup_gpio)) {
		pr_info("iris one-wired cmd: %d\n", cmd);
		spin_lock_irqsave(&iris_lock, flags);
		for (i = 0; i < cmd; ++i) {
			gpio_set_value(iris_wakeup_gpio, 1);
			udelay(56);
			gpio_set_value(iris_wakeup_gpio, 0);
			udelay(56);
		}
		udelay(10);
		spin_unlock_irqrestore(&iris_lock, flags);
		usleep_range(16*1000, 16*1000);
	}
}

static void iris_reset(struct mdss_dsi_ctrl_pdata *ctrl_pdata, int enable)
{
	int iris_rst_gpio = lge_extra_find_gpio_by_name(ctrl_pdata, "iris-rst");
	if (gpio_is_valid(iris_rst_gpio)) {
		pr_info("iris reset %d\n", enable);
		gpio_set_value(iris_rst_gpio, enable?1:0);
	}
}
#endif

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int rc = 0;

	if (pdata == NULL) {
		pr_err("Invalid input data\n");
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	if (pinfo == NULL) {
		pr_err("Invalid pinfo data\n");
		return -EINVAL;
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("reset line not configured\n");
		return rc;
	}

	pr_info("+ enable = %d (override: mh3j)\n", enable);

	if (enable) {
		rc = request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
		if (!pinfo->cont_splash_enabled) {
#if defined(CONFIG_PXLW_IRIS3)
			int i = 0;
			if (gpio_is_valid(ctrl_pdata->iris_rst_gpio)) {
				if (pdata->panel_info.rst_seq_len) {
					rc = gpio_direction_output(ctrl_pdata->iris_rst_gpio,
						pdata->panel_info.rst_seq[0]);
					if (rc) {
						pr_err("unable to set dir for iris rst gpio\n");
						goto exit;
					}
				}

				for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
					gpio_set_value((ctrl_pdata->iris_rst_gpio),
						pdata->panel_info.rst_seq[i]);
					if (pdata->panel_info.rst_seq[++i])
						usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
				}
			}
#else
			iris_reset(ctrl_pdata, 1);
#endif
			panel_reset(ctrl_pdata, enable, lge_mdss_dsi_panel_power_seq_all());
#ifndef CONFIG_PXLW_IRIS3
			iris_send_one_wired_cmd(ctrl_pdata, 2); // analog bypass mode
			iris_send_one_wired_cmd(ctrl_pdata, 4); // low power mode
#endif
		}

		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("Panel Not properly turned OFF\n");
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("Reset panel done\n");
		}
	} else {
		panel_reset(ctrl_pdata, enable, lge_mdss_dsi_panel_power_seq_all());
#if defined(CONFIG_PXLW_IRIS3)
		if (gpio_is_valid(ctrl_pdata->iris_rst_gpio)) {
			gpio_set_value(ctrl_pdata->iris_rst_gpio, 0);
			gpio_free(ctrl_pdata->iris_rst_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->abyp_gpio)) {
			gpio_free(ctrl_pdata->abyp_gpio);
		}
#else
		iris_reset(ctrl_pdata, 0);
#endif
	}
#if defined(CONFIG_PXLW_IRIS3)
exit:
#endif
	pr_info("-\n");

	return rc;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("Invalid pdata\n");
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("Invalid ctrl_pdata\n");
		return -EINVAL;
	}

	pr_info("+ (override: mh3j)\n");

	// off
	if (!pdata->panel_info.cont_splash_enabled && !lge_mdss_dsi_panel_power_seq_all()) {
		panel_reset(ctrl_pdata, 0, true);
		lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	}

	// on
	lge_extra_gpio_set_value(ctrl_pdata, "vddio", 1);
	usleep_range(5000, 5000);

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
			pr_err("Panel reset failed. rc=%d\n", ret);
	}

	pr_info("-\n");

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("Invalid pdata\n");
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("Invalid ctrl_pdata\n");
		return -EINVAL;
	}

	pr_info("+ (override: mh3j)\n");

	if (!ctrl_pdata->lge_extra.lp11_off) {
		ret = mdss_dsi_panel_reset(pdata, 0);
	}

	if (ret) {
		pr_warn("Panel reset failed. rc=%d\n", ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	if (lge_mdss_dsi_panel_power_seq_all()) {
		lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	}

	pr_info("-\n");
end:
	return ret;
}
#endif

void mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = platform_get_drvdata(pdev);

	if (!ctrl_pdata) {
		return;
	}

	pr_info("+\n");
	panel_reset(ctrl_pdata, 0, true);
	lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	pr_info("-\n");
}

int lge_mdss_dsi_pre_event_handler(struct mdss_panel_data *pdata, int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	switch (event) {
	case MDSS_EVENT_LINK_READY:
		rc = touch_notifier_call_chain(LCD_EVENT_LCD_UNBLANK, (void *)&event);
		break;
	case MDSS_EVENT_BLANK:
		rc = touch_notifier_call_chain(LCD_EVENT_LCD_BLANK, (void *)&event);
		break;
	default:
		break;
	}

	return rc;
}

int lge_mdss_dsi_post_event_handler(struct mdss_panel_data *pdata, int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	static int pre_panel_mode = LCD_MODE_STOP;
	int cur_panel_mode = pre_panel_mode;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	switch (event) {
	case MDSS_EVENT_PANEL_ON:
		cur_panel_mode = LCD_MODE_U3;
		pr_info("event=MDSS_EVENT_PANEL_ON panel_mode=%d -> %d\n", pre_panel_mode, cur_panel_mode);
		lge_set_panel_recovery_flag(0);
		break;
	case MDSS_EVENT_PANEL_OFF:
		cur_panel_mode = LCD_MODE_U0;
		pr_info("event=MDSS_EVENT_PANEL_OFF panel_mode=%d -> %d\n", pre_panel_mode, cur_panel_mode);
		break;
	default:
		break;
	}

	if (pre_panel_mode != cur_panel_mode) {
		rc = touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void *)&cur_panel_mode);
		pre_panel_mode = cur_panel_mode;
	}

	return rc;
}

int lge_get_lpwg_on_event(void)
{
		return MDSS_EVENT_MAX;
}
int lge_get_lpwg_off_event(void)
{
		return MDSS_EVENT_MAX;
}

static int panel_recovery_flag = 0;
int lge_get_panel_recovery_flag()
{
	pr_info("flag=%d", panel_recovery_flag);
	return panel_recovery_flag;
}

void lge_set_panel_recovery_flag(int flag)
{
	pr_info("flag=%d", flag);
	panel_recovery_flag = flag;
}
