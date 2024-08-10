#include <linux/delay.h>
#include "mdss_dsi.h"

#if IS_ENABLED(CONFIG_LGE_DISPLAY_MFTS)
#include "lge/mfts_mode.h"
#endif

#include <soc/qcom/lge/board_lge.h>
#include "lge_mdss_dsi_smallpd.h"

#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
#define EXT_DSV_PRIVILEGED
#include <linux/mfd/external_dsv.h>
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
#include "lge/lge_mdss_debug.h"
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY)
#include <linux/msm_lcd_recovery.h>
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
#include <soc/qcom/lge/board_lge.h>
#endif

#include <linux/input/lge_touch_notify_oos.h>
#include <linux/msm_lcd_power_mode.h>

#if defined(CONFIG_BACKLIGHT_SGM37603A)
extern void sgm37603a_backlight_on(int level, int cabc);
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

enum smallpd_panel_type {
	BOE_ONCELL_SW43103 = 0,
	BOE_INCELL_ILI9881H,
	BOE_INCELL_FT8006P,
	CPT_INCELL_FT8006P,
	UNDEFINED,
};

/* Touch LPWG Status */
static unsigned int pre_panel_mode = LCD_MODE_STOP;
static unsigned int cur_panel_mode = LCD_MODE_STOP;

#if defined(CONFIG_LGE_DISPLAY_RECOVERY)
static int panel_recovery_flag = 0;
#endif

bool flag_panel_deep_sleep_ctrl = false;
bool flag_panel_deep_sleep_status = false;

int lge_get_panel_id(void)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	ctrl_pdata = lge_mdss_dsi_get_ctrl_pdata();
	pr_info("%s : panel_id = %d\n", __func__, ctrl_pdata->lge_extra.panel_id);

	return ctrl_pdata->lge_extra.panel_id;
}
void lge_panel_enter_deep_sleep(void)
{
	switch (lge_get_panel_id()) {
		case BOE_ONCELL_SW43103:
			sw43103_panel_enter_deep_sleep();
			break;	
		case BOE_INCELL_ILI9881H:
			ili9881h_panel_enter_deep_sleep();
			break;
		case CPT_INCELL_FT8006P:
			cpt_ft8006p_panel_enter_deep_sleep();
			break;
		case BOE_INCELL_FT8006P:
			boe_ft8006p_panel_enter_deep_sleep();
			break;
		default:
			break;
	}
}

void lge_panel_exit_deep_sleep(void)
{
	usleep_range(10000, 10000);
	switch (lge_get_panel_id()) {
		case BOE_ONCELL_SW43103:
			sw43103_panel_exit_deep_sleep();
			break;	
		case BOE_INCELL_ILI9881H:
			ili9881h_panel_exit_deep_sleep();
			break;
		case CPT_INCELL_FT8006P:
			cpt_ft8006p_panel_exit_deep_sleep();
			break;
		case BOE_INCELL_FT8006P:
			boe_ft8006p_panel_exit_deep_sleep();
			break;
		default:
			break;
	}
}

void lge_panel_set_power_mode(int mode)
{
	struct mdss_dsi_ctrl_pdata *pdata = NULL;

	pr_info("%s start, mode = %d \n", __func__, mode);

	pdata = lge_mdss_dsi_get_ctrl_pdata();
	switch (mode){
		case DEEP_SLEEP_ENTER:
			lge_panel_enter_deep_sleep();
			break;
		case DEEP_SLEEP_EXIT:
			lge_panel_exit_deep_sleep();
			break;
		default :
			break;
	}

	pr_info("%s done \n", __func__);
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_RESET)
int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;

	switch (lge_get_panel_id()) {
		case BOE_ONCELL_SW43103:
			rc = sw43103_mdss_dsi_panel_reset(pdata, enable);
			break;
		case BOE_INCELL_ILI9881H:
			rc = ili9881h_mdss_dsi_panel_reset(pdata, enable);
			break;
		case CPT_INCELL_FT8006P:
			rc = cpt_ft8006p_mdss_dsi_panel_reset(pdata, enable);
			break;
		case BOE_INCELL_FT8006P:
			rc = boe_ft8006p_mdss_dsi_panel_reset(pdata, enable);
			break;
		default:
			break;
        }
	return rc;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;

	switch (lge_get_panel_id()) {
		case BOE_ONCELL_SW43103:
			ret = sw43103_mdss_dsi_panel_power_on(pdata);
			break;
		case BOE_INCELL_ILI9881H:
			ret = ili9881h_mdss_dsi_panel_power_on(pdata);
			break;
		case CPT_INCELL_FT8006P:
			ret = cpt_ft8006p_mdss_dsi_panel_power_on(pdata);
			break;
		case BOE_INCELL_FT8006P:
			ret = boe_ft8006p_mdss_dsi_panel_power_on(pdata);
			break;
		default:
			break;
       }
	pr_info("%s: -\n", __func__);
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;

	switch (lge_get_panel_id()) {
		case BOE_ONCELL_SW43103:
			ret = sw43103_mdss_dsi_panel_power_off(pdata);
			break;
		case BOE_INCELL_ILI9881H:
			ret = ili9881h_mdss_dsi_panel_power_off(pdata);
			break;
		case CPT_INCELL_FT8006P:
			ret = cpt_ft8006p_mdss_dsi_panel_power_off(pdata);
			break;
		case BOE_INCELL_FT8006P:
			ret = boe_ft8006p_mdss_dsi_panel_power_off(pdata);
			break;
		default:
			break;
       }
	pr_info("%s: -\n", __func__);
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
extern int mdss_dsi_set_clk_src(struct mdss_dsi_ctrl_pdata *ctrl);
extern int mdss_dsi_clk_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, void *clk_handle,
 enum mdss_dsi_clk_type clk_type, enum mdss_dsi_clk_state clk_state);

void mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	switch (lge_get_panel_id()) {
		case BOE_ONCELL_SW43103:
			sw43103_mdss_dsi_ctrl_shutdown(pdev);
			break;
		case BOE_INCELL_ILI9881H:
			ili9881h_mdss_dsi_ctrl_shutdown(pdev);
			break;
		case CPT_INCELL_FT8006P:
			cpt_ft8006p_mdss_dsi_ctrl_shutdown(pdev);
			break;
		case BOE_INCELL_FT8006P:
			boe_ft8006p_mdss_dsi_ctrl_shutdown(pdev);
			break;
		default:
			break;
	}
}
#endif

int lge_mdss_dsi_pre_event_handler(struct mdss_panel_data *pdata, int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	switch (event) {
	case MDSS_EVENT_LINK_READY:
		rc = touch_notifier_call_chain(LCD_EVENT_LCD_UNBLANK, (void *)&event);
		break;
	case MDSS_EVENT_RESET:
		break;
	case MDSS_EVENT_UNBLANK:
		break;
	case MDSS_EVENT_POST_PANEL_ON:
		break;
	case MDSS_EVENT_BLANK:
		rc = touch_notifier_call_chain(LCD_EVENT_LCD_BLANK, (void *)&event);
		break;
	case MDSS_EVENT_PANEL_OFF:
		break;
	default:
		pr_info("%s: nothing to do about this event=%d\n", __func__, event);
	}
	return rc;
}

int lge_mdss_dsi_post_event_handler(struct mdss_panel_data *pdata, int event, void *arg)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	switch (event) {
	case MDSS_EVENT_RESET:
		flag_panel_deep_sleep_ctrl = false;
		if (flag_panel_deep_sleep_status) {
				lge_set_panel_recovery_flag(true);
		}
		break;
	case MDSS_EVENT_UNBLANK:
		if (lge_mdss_dsi_panel_power_seq_all()) {
			lge_set_panel_recovery_flag(false);
		}
		pr_info("%s: event=MDSS_EVENT_UNBLANK\n", __func__);
		break;
	case MDSS_EVENT_POST_PANEL_ON:
		if(lge_get_panel_id() != BOE_INCELL_ILI9881H)
			touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_END, NULL);

		cur_panel_mode = LCD_MODE_U3;
		pr_info("%s: event=MDSS_EVENT_PANEL_ON panel_mode=%d,%d\n",
			__func__, pre_panel_mode, cur_panel_mode);

		if (flag_panel_deep_sleep_status) {
				lge_set_panel_recovery_flag(false);
				flag_panel_deep_sleep_status = false;
		}
	#if defined(CONFIG_BACKLIGHT_SGM37603A)
		sgm37603a_backlight_on(0xff, ctrl_pdata->lge_extra.cabc_status);
	#endif
		break;
	case MDSS_EVENT_BLANK:
		break;
	case MDSS_EVENT_PANEL_OFF:
		cur_panel_mode = LCD_MODE_U0;
		pr_info("%s: event=MDSS_EVENT_PANEL_OFF panel_mode=%d,%d\n",
			__func__, pre_panel_mode, cur_panel_mode);
		flag_panel_deep_sleep_ctrl = true;
		break;
	default:
		pr_info("%s: nothing to do about this event=%d\n", __func__, event);
	}

	if (pre_panel_mode != cur_panel_mode) {
		rc = touch_notifier_call_chain(LCD_EVENT_LCD_MODE, (void *)&cur_panel_mode);
		pre_panel_mode = cur_panel_mode;
	}

	return rc;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_TOUCH_NOTIFIER_CALL_CHAIN)
int lge_get_lpwg_on_event(void)
{
		return MDSS_EVENT_MAX;
}
int lge_get_lpwg_off_event(void)
{
		return MDSS_EVENT_MAX;
}
#endif

#if defined(CONFIG_LGE_DISPLAY_RECOVERY)

int lge_get_panel_recovery_flag()
{
	pr_info("%s: flag=%d", __func__, panel_recovery_flag);
	return panel_recovery_flag;
}

void lge_set_panel_recovery_flag(int flag)
{
	pr_info("%s: flag=%d", __func__, flag);
	panel_recovery_flag = flag;
}

int lge_mdss_report_touchintpin_keep_low(void)
{
	pr_info("%s : D-IC is in abnormal status", __func__);
	lge_mdss_report_panel_dead(PANEL_HW_RESET);

	return 0;
}

int lge_mdss_dsi_panel_power_seq_all()
{
	int ret = 0;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_MFTS)
	if (lge_get_display_power_ctrl())
		ret = 1;
#endif

	if (lge_get_panel_recovery_flag())
		ret = 1;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
	if (lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO)
		ret = 1;
#endif

	return ret;
}

EXPORT_SYMBOL(lge_mdss_report_touchintpin_keep_low);
#endif
