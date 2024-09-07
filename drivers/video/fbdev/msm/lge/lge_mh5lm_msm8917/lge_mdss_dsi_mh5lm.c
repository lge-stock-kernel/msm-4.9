#include <linux/delay.h>
#include "mdss_dsi.h"

#if IS_ENABLED(CONFIG_LGE_DISPLAY_MFTS)
#include "lge/mfts_mode.h"
#endif

#include <soc/qcom/lge/board_lge.h>
#include "lge_mdss_dsi_mh5lm.h"

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
#include "lge/lge_mdss_debug.h"
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY)
#include <linux/msm_lcd_recovery.h>
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
#include <soc/qcom/lge/board_lge.h>
#endif

#include <linux/msm_lcd_power_mode.h>

enum {
	LCD_MODE_U0 = 0,
	LCD_MODE_U2_UNBLANK,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
};

enum mh5lm_panel_type {
	TXD_ILI9881C = 0,
	GX_JD9365Z,
	UNDEFINED,
};

/* Touch LPWG Status */
static unsigned int pre_panel_mode = LCD_MODE_STOP;
static unsigned int cur_panel_mode = LCD_MODE_STOP;

#if defined(CONFIG_LGE_DISPLAY_RECOVERY)
static int panel_recovery_flag = 0;
#endif


int lge_get_panel_id(void)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	ctrl_pdata = lge_mdss_dsi_get_ctrl_pdata();
	pr_info("%s : panel_id = %d\n", __func__, ctrl_pdata->lge_extra.panel_id);

	return ctrl_pdata->lge_extra.panel_id;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_RESET)
int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;

	switch (lge_get_panel_id()) {
		case TXD_ILI9881C:
			rc = mdss_dsi_panel_reset_ili9881c(pdata, enable);
			break;
		case GX_JD9365Z:
			rc = mdss_dsi_panel_reset_jd9365z(pdata, enable);
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
		case TXD_ILI9881C:
			ret = mdss_dsi_panel_power_on_ili9881c(pdata);
			break;
		case GX_JD9365Z:
			ret = mdss_dsi_panel_power_on_jd9365z(pdata);
			break;
		default:
			break;
       }
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;

	switch (lge_get_panel_id()) {
		case TXD_ILI9881C:
			ret = mdss_dsi_panel_power_off_ili9881c(pdata);
			break;
		case GX_JD9365Z:
			ret = mdss_dsi_panel_power_off_jd9365z(pdata);
			break;
		default:
			break;
       }
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
void mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	switch (lge_get_panel_id()) {
		case TXD_ILI9881C:
			mdss_dsi_ctrl_shutdown_ili9881c(pdev);
			break;
		case GX_JD9365Z:
			mdss_dsi_ctrl_shutdown_jd9365z(pdev);
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
		break;
	case MDSS_EVENT_BLANK:
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
		lge_mdss_panel_dic_reg_dump(ctrl_pdata);
#endif
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
	struct mdss_panel_info *pinfo = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	switch (event) {
	case MDSS_EVENT_RESET:
		break;
	case MDSS_EVENT_UNBLANK:
		pr_info("%s: event=MDSS_EVENT_UNBLANK\n", __func__);
		break;
	case MDSS_EVENT_PANEL_ON:
		cur_panel_mode = LCD_MODE_U3;
		pr_info("%s: event=MDSS_EVENT_PANEL_ON panel_mode=%d,%d\n",
			__func__, pre_panel_mode, cur_panel_mode);

		lge_set_panel_recovery_flag(false);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
		lge_mdss_panel_dic_reg_dump(ctrl_pdata);

		lge_debug_event_trigger(pdata, "/etc/debug_dsi_cmd_tx", DEBUG_DSI_CMD_TX);
		lge_debug_event_trigger(pdata, "/etc/debug_dsi_cmd_rx", DEBUG_DSI_CMD_RX);
#endif
		break;
	case MDSS_EVENT_BLANK:
		pr_info("%s: event=MDSS_EVENT_BLANK\n", __func__);
		break;
	case MDSS_EVENT_PANEL_OFF:
		cur_panel_mode = LCD_MODE_U0;
		pr_info("%s: event=MDSS_EVENT_PANEL_OFF panel_mode=%d,%d\n",
			__func__, pre_panel_mode, cur_panel_mode);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
		lge_debug_event_trigger(pdata, "", INVALID); //NOTE : This is must-do-null-event-trigger for debug_event to escape from unblnak
#endif
		break;
	default:
		break;
	}
	return rc;
}

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
EXPORT_SYMBOL(lge_mdss_report_touchintpin_keep_low);
#endif
