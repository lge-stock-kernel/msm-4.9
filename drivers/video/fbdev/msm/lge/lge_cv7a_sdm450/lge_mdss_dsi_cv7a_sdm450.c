#include <linux/delay.h>
#include "mdss_dsi.h"

#if IS_ENABLED(CONFIG_LGE_DISPLAY_MFTS)
#include "lge/mfts_mode.h"
#endif

#include <soc/qcom/lge/board_lge.h>

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

#include <linux/msm_lcd_power_mode.h>

#include <linux/input/lge_touch_notify.h>
enum {
	LCD_MODE_U0 = 0,
	LCD_MODE_U2_UNBLANK,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
};

/* Touch LPWG Status */
static unsigned int pre_panel_mode = LCD_MODE_STOP;
static unsigned int cur_panel_mode = LCD_MODE_STOP;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
#define NUM_SHA_CTRL 		5
#define NUM_SC_CTRL 		4
#define OFFSET_SC_CTRL		1
#define NUM_SAT_CTRL 		5
#define OFFSET_SAT_CTRL 	8
#define NUM_HUE_CTRL 		5
#define OFFSET_HUE_CTRL 	2

#define CHECK_BOUNDARY(v, min, max) do {\
	if (v < min) v = min; \
	if (v > max) v = max; \
} while(0)

static char sha_ctrl_values[NUM_SHA_CTRL] = {0x00, 0x0D, 0x1A, 0x30, 0xD2};

static char sat_ctrl_values[NUM_SAT_CTRL][OFFSET_SAT_CTRL] = {
	{0x00, 0x38, 0x70, 0xA8, 0xE1, 0x00, 0x00, 0x00},
	{0x00, 0x3C, 0x78, 0xB4, 0xF1, 0x00, 0x00, 0x00},
	{0x00, 0x40, 0x80, 0xC0, 0x00, 0x01, 0x00, 0x00},
	{0x00, 0x43, 0x87, 0xCB, 0x00, 0x01, 0x00, 0x00},
	{0x00, 0x47, 0x8F, 0xD7, 0x00, 0x01, 0x00, 0x00},
};

static char hue_ctrl_values[NUM_HUE_CTRL][OFFSET_HUE_CTRL] = {
	{0xF7, 0x00},
	{0xF4, 0x00},
	{0xF0, 0x00},
	{0x74, 0x00},
	{0x77, 0x00},
};

static int rgb_preset[STEP_DG_PRESET][RGB_ALL] = {
	{PRESET_SETP2_OFFSET, PRESET_SETP0_OFFSET, PRESET_SETP2_OFFSET},
	{PRESET_SETP2_OFFSET, PRESET_SETP1_OFFSET, PRESET_SETP2_OFFSET},
	{PRESET_SETP0_OFFSET, PRESET_SETP0_OFFSET, PRESET_SETP0_OFFSET},
	{PRESET_SETP0_OFFSET, PRESET_SETP1_OFFSET, PRESET_SETP0_OFFSET},
	{PRESET_SETP0_OFFSET, PRESET_SETP2_OFFSET, PRESET_SETP0_OFFSET}
};

// I add offset for index 1 ~ 3.
static int gc_preset[STEP_GC_PRESET][RGB_ALL] = {
	{0x00, 0x00, 0x00},
	{0x00, 0x00, 0x06},
	{0x03, 0x00, 0x00},
	{0x00, 0x00, 0x00},
};

static char dg_ctrl_values[NUM_DG_PRESET][OFFSET_DG_CTRL] = {
	{0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40},
	{0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F},
	{0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E},
	{0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D},
	{0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C},
	{0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B},
	{0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39}
};
#endif

#if defined(CONFIG_LGE_DISPLAY_RECOVERY)
static int panel_recovery_flag = 0;
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
extern int mdss_dsi_pinctrl_set_state(struct mdss_dsi_ctrl_pdata *ctrl_pdata, bool active);
#endif

extern int lge_mdss_fb_get_shutdown_state(void);
extern bool lge_mdss_fb_get_splash_iommu_status(void);

extern void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *pcmds, u32 flags);
extern int mdss_dsi_parse_dcs_cmds(struct device_node *np, struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key);

void lge_panel_set_power_mode(int mode)
{
	struct mdss_dsi_ctrl_pdata *pdata = NULL;

	pr_info("%s start, mode = %d \n", __func__, mode);

	pdata = lge_mdss_dsi_get_ctrl_pdata();
	if (pdata == NULL)
		return;

	switch (mode){
	case DSV_TOGGLE:
		ext_dsv_mode_change(ENM_ENTER);
		usleep_range(20000, 20000);
		lge_mdss_dsi_panel_extra_cmds_send(pdata, "enm-toggle-start");
		break;
	case DSV_ALWAYS_ON:
		ext_dsv_mode_change(ENM_EXIT);
		break;
	case DEEP_SLEEP_ENTER:
		pr_err("%s: skip LCD power control(enter deep)\n", __func__);
		break;
	case DEEP_SLEEP_EXIT:
		pr_err("%s: skip LCD power control(exit deep)\n", __func__);
		break;
	default :
		break;
	}

	pr_info("%s done \n", __func__);
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_RESET)
int mdss_dsi_panel_reset_sw49107(struct mdss_panel_data *pdata, int enable)
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
	if (pinfo == NULL) {
		pr_err("%s: Invalid pinfo data\n", __func__);
		return -EINVAL;
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_err("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return rc;
	}

	pr_info("%s: + enable = %d (override: cv7a)\n", __func__, enable);

	if (enable) {
		if (!pinfo->cont_splash_enabled) {
			touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_START, NULL);
			for (i = 0; i < pdata->panel_info.rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					pdata->panel_info.rst_seq[i]);

				if (pdata->panel_info.rst_seq[++i])
					usleep_range(pinfo->rst_seq[i] * 1000, pinfo->rst_seq[i] * 1000);
			}
			touch_notifier_call_chain(LCD_EVENT_TOUCH_RESET_END, NULL);
			usleep_range(11000, 11000);
			pr_info("%s: LCD/Touch reset sequence done \n", __func__);
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
			usleep_range(1000, 1000);

			gpio_set_value((ctrl_pdata->rst_gpio), enable);
			usleep_range(5000, 5000);
		}
	}

	pr_info("%s: -\n", __func__);

	return rc;
}


int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	int rc = 0;

	rc = mdss_dsi_panel_reset_sw49107(pdata, enable);

	return rc;
}
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_ON)
int mdss_dsi_panel_power_on_sw49107(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid pdata\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: + (override: cv7a)\n", __func__);

	if (lge_mdss_dsi_panel_power_seq_all()) {
		lge_extra_gpio_set_value(ctrl_pdata, "vddio", 1);
		usleep_range(5000, 5000);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
		ext_dsv_chip_enable(1);
		ext_dsv_mode_change(POWER_ON);
		usleep_range(5000, 5000);
#endif
	}

	ret = msm_mdss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 1);
	if (ret) {
		pr_err("%s: failed to enable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));
		return ret;
	}

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

	pr_info("%s: -\n", __func__);

	return ret;
}

int mdss_dsi_panel_power_on(struct mdss_panel_data *pdata)
{
	int ret = 0;

	ret = mdss_dsi_panel_power_on_sw49107(pdata);

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_PANEL_POWER_OFF)
int mdss_dsi_panel_power_off_sw49107(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid pdata\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (ctrl_pdata == NULL) {
		pr_err("%s: Invalid ctrl_pdata\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: + (override: cv7a)\n", __func__);

	if (!ctrl_pdata->lge_extra.lp11_off) {
		ret = mdss_dsi_panel_reset(pdata, 0);
	}

	if (ret) {
		pr_warn("%s: Panel reset failed. rc=%d\n", __func__, ret);
		ret = 0;
	}

	if (mdss_dsi_pinctrl_set_state(ctrl_pdata, false))
		pr_debug("reset disable: pinctrl not enabled\n");

	ret = msm_mdss_enable_vreg(
		ctrl_pdata->panel_power_data.vreg_config,
		ctrl_pdata->panel_power_data.num_vreg, 0);
	if (ret)
		pr_err("%s: failed to disable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_PANEL_PM));

#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
	if (lge_mdss_dsi_panel_power_seq_all()) {
		ext_dsv_mode_change(POWER_OFF);
		ext_dsv_chip_enable(0);
	}
#endif

	if (lge_mdss_dsi_panel_power_seq_all()) {
		lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	}

	pr_info("%s: -\n", __func__);
end:
	return ret;
}

int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata)
{
	int ret = 0;

	ret = mdss_dsi_panel_power_off_sw49107(pdata);

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
extern int mdss_dsi_set_clk_src(struct mdss_dsi_ctrl_pdata *ctrl);
extern int mdss_dsi_clk_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, void *clk_handle,
	enum mdss_dsi_clk_type clk_type, enum mdss_dsi_clk_state clk_state);

void mdss_dsi_ctrl_shutdown_sw49107(struct platform_device *pdev)
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

	lge_extra_gpio_set_value(ctrl_pdata, "touch_reset", 0);
	usleep_range(1000, 1000);

	gpio_set_value((ctrl_pdata->rst_gpio), 0);
	usleep_range(3000, 3000);

	ret += mdss_dsi_clk_ctrl(ctrl_pdata, ctrl_pdata->dsi_clk_handle,
		MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
	if (ret) {
		pr_err("%s: could fail to set LP00\n", __func__);
	}
	usleep_range(5000, 5000);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_EXTERNAL_DSV)
	ext_dsv_mode_change(POWER_OFF);
	ext_dsv_chip_enable(0);
	usleep_range(3000, 3000);
#endif

	lge_extra_gpio_set_value(ctrl_pdata, "vddio", 0);
	usleep_range(1000, 1000);

	pr_info("%s: panel shutdown done \n", __func__);

	return;
}

void mdss_dsi_ctrl_shutdown(struct platform_device *pdev)
{
	mdss_dsi_ctrl_shutdown_sw49107(pdev);
}
#endif

/*
	This function is reffered from mdss_dsi_cmd_test_pattern() in mdss_dsi_host.c.
	It just needs DSI clock on/off between the operation.
*/
void lge_mdss_dsi_cmd_test_pattern(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i;

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON);

	MIPI_OUTP((ctrl->ctrl_base) + 0x015c, 0x201);
	MIPI_OUTP((ctrl->ctrl_base) + 0x016c, 0x000000); /* black */
	i = 0;
	while (i++ < 1) {
		MIPI_OUTP((ctrl->ctrl_base) + 0x0184, 0x1);
		/* Add sleep to get ~1 fps frame rate*/
		msleep(17);
	}
	MIPI_OUTP((ctrl->ctrl_base) + 0x015c, 0x0);

	mdss_dsi_clk_ctrl(ctrl, ctrl->dsi_clk_handle, MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_OFF);
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
	case MDSS_EVENT_UNBLANK:
		if (!lge_mdss_fb_get_splash_iommu_status()) // true : first unblank, false : always false except first unblank
			lge_mdss_dsi_cmd_test_pattern(ctrl_pdata); // To avoid showing short term g-ram garbage data, transfer black frame to let d-ic initiate earlier
		if (lge_mdss_dsi_panel_power_seq_all()) {
			lge_set_panel_recovery_flag(false);
		}
		pr_info("%s: event=MDSS_EVENT_UNBLANK\n", __func__);
		break;
	case MDSS_EVENT_PANEL_ON:
		cur_panel_mode = LCD_MODE_U3;
		pr_info("%s: event=MDSS_EVENT_PANEL_ON panel_mode=%d,%d\n",
			__func__, pre_panel_mode, cur_panel_mode);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
		lge_mdss_panel_dic_reg_dump(ctrl_pdata);
#endif
		break;
	case MDSS_EVENT_BLANK:
		pr_info("%s: event=MDSS_EVENT_BLANK\n", __func__);
		break;
	case MDSS_EVENT_PANEL_OFF:
		cur_panel_mode = LCD_MODE_U0;
		pr_info("%s: event=MDSS_EVENT_PANEL_OFF panel_mode=%d,%d\n",
			__func__, pre_panel_mode, cur_panel_mode);
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

int lge_get_panel_id(void)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	ctrl_pdata = lge_mdss_dsi_get_ctrl_pdata();
	pr_info("%s : panel_id = %d\n", __func__, ctrl_pdata->lge_extra.panel_id);

	return ctrl_pdata->lge_extra.panel_id;
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
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
int lge_mdss_dsi_rgb_tune_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i = 0;
	int red_index = 0, green_index = 0, blue_index = 0;
	enum lge_gamma_correction_mode cur_gc_mode = LGE_GC_MOD_NOR;

	char *payload_ctrl[4] = {NULL, };
	struct mdss_panel_info *pinfo = NULL;
	struct dsi_panel_cmds *pcmds = NULL;

	if (ctrl == NULL) {
		pr_err("Invalid input\n");
		return -ENODEV;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo == NULL) {
		pr_err("%s: invalid panel_info \n", __func__);
		return -ENODEV;
	}

	cur_gc_mode = pinfo->gc_mode;
	pcmds = lge_get_extra_cmds_by_name(ctrl, "dg-dummy");
	if (pcmds) {
		for (i = 0; i < pcmds->cmd_cnt; i++)
		    payload_ctrl[i] = pcmds->cmds[i].payload;
	} else {
		pr_err("no cmds: dg-dummy\n");
		return -ENODEV;
	}

	CHECK_BOUNDARY(pinfo->cm_preset_step, 0, 4);
	CHECK_BOUNDARY(pinfo->cm_red_step, 0, 4);
	CHECK_BOUNDARY(pinfo->cm_green_step, 0, 4);
	CHECK_BOUNDARY(pinfo->cm_blue_step, 0, 4);

	if (cur_gc_mode == LGE_GC_MOD_NOR) {
		//Color temperature and RGB should be considered simultaneously.
		red_index   = rgb_preset[pinfo->cm_preset_step][RED] + pinfo->cm_red_step;
		green_index = rgb_preset[pinfo->cm_preset_step][GREEN] + pinfo->cm_green_step;
		blue_index  = rgb_preset[pinfo->cm_preset_step][BLUE] + pinfo->cm_blue_step;
	} else {
		//Only the color temperature specification should be considered.
		red_index   = gc_preset[cur_gc_mode][RED];
		green_index = gc_preset[cur_gc_mode][GREEN];
		blue_index  = gc_preset[cur_gc_mode][BLUE];
	}

	pr_info("%s red_index=(%d) green_index=(%d) blue_index=(%d)\n", __func__, red_index, green_index, blue_index);
	for (i = 0; i < OFFSET_DG_CTRL; i++) {
		payload_ctrl[RED][i+1] = dg_ctrl_values[red_index][i];
		payload_ctrl[GREEN][i+1] = dg_ctrl_values[green_index][i];
		payload_ctrl[BLUE][i+1] = dg_ctrl_values[blue_index][i];
	}
	mdss_dsi_panel_cmds_send(ctrl, pcmds, CMD_REQ_COMMIT);

	pr_info("%s R Reg[0x%02x] Value[0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x] \n", __func__,
	        payload_ctrl[RED][0],
	        payload_ctrl[RED][1], payload_ctrl[RED][2], payload_ctrl[RED][3], payload_ctrl[RED][4],
	        payload_ctrl[RED][5], payload_ctrl[RED][6], payload_ctrl[RED][7], payload_ctrl[RED][8]);

	pr_info("%s G Reg[0x%02x] Value[0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x] \n", __func__,
	        payload_ctrl[GREEN][0],
	        payload_ctrl[GREEN][1], payload_ctrl[GREEN][2], payload_ctrl[GREEN][3], payload_ctrl[GREEN][4],
	        payload_ctrl[GREEN][5], payload_ctrl[GREEN][6], payload_ctrl[GREEN][7], payload_ctrl[GREEN][8]);

	pr_info("%s B Reg[0x%02x] Value[0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x] \n", __func__,
	        payload_ctrl[BLUE][0],
	        payload_ctrl[BLUE][1], payload_ctrl[BLUE][2], payload_ctrl[BLUE][3], payload_ctrl[BLUE][4],
	        payload_ctrl[BLUE][5], payload_ctrl[BLUE][6], payload_ctrl[BLUE][7], payload_ctrl[BLUE][8]);
	return 0;
}

int lge_mdss_dsi_screen_tune_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char *payload_ctrl[3] = {NULL, };
	struct mdss_panel_info *pinfo = NULL;
	struct dsi_panel_cmds *pcmds;

	int i = 0;
	int sc_sha_step_,sc_sat_step_, sc_hue_step_ ;

	if (ctrl == NULL) {
		pr_err("Invalid input\n");
		return -ENODEV;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo == NULL) {
		pr_err("%s: invalid panel_info \n", __func__);
		return -ENODEV;
	}

	pcmds = lge_get_extra_cmds_by_name(ctrl, "color-dummy");
	if (pcmds) {
		for (i = 0; i < pcmds->cmd_cnt; i++)
			payload_ctrl[i] = pcmds->cmds[i].payload;
	} else {
		pr_err("no cmds: color-dummy\n");
		return -ENODEV;
	}

	CHECK_BOUNDARY(pinfo->sc_sat_step, 0, 4);
	CHECK_BOUNDARY(pinfo->sc_hue_step, 0, 4);
	CHECK_BOUNDARY(pinfo->sc_sha_step, 0, 4);

	sc_sha_step_ = pinfo->sc_sha_step;
	sc_sat_step_ = pinfo->sc_sat_step;
	sc_hue_step_ = pinfo->sc_hue_step;

	// if not Manual, need default.
	if( pinfo->screen_color_mode != LGE_COLOR_MAN ){
		sc_sha_step_ = SHARP_DEFAULT;
		sc_sat_step_ = SC_MODE_DEFAULT;
		sc_hue_step_ = SC_MODE_DEFAULT;

		if( pinfo->screen_color_mode == LGE_COLOR_GAM )
			sc_sat_step_ = 3;// Color Saturation +10%
	}

	//sharpness
	payload_ctrl[0][3] = sha_ctrl_values[sc_sha_step_];

	//saturation
	for (i = 0; i < OFFSET_SAT_CTRL; i++)
		payload_ctrl[1][i+1] = sat_ctrl_values[sc_sat_step_][i];

	//hue
	for (i = 0; i < OFFSET_HUE_CTRL; i++)
		payload_ctrl[1][i+7] = hue_ctrl_values[sc_hue_step_][i];

	mdss_dsi_panel_cmds_send(ctrl, pcmds, CMD_REQ_COMMIT);

	pr_info("%s Sha Reg[0x%02x] Value[0x%02x][0x%02x][0x%02x][0x%02x] \n", __func__,
			payload_ctrl[0][0], payload_ctrl[0][1], payload_ctrl[0][2], payload_ctrl[0][3], payload_ctrl[0][4]);

	pr_info("%s Sat,Hue Reg[0x%02x] Value[0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x][0x%02x] \n", __func__,
			payload_ctrl[1][0],
			payload_ctrl[1][1], payload_ctrl[1][2], payload_ctrl[1][3], payload_ctrl[1][4],
			payload_ctrl[1][5], payload_ctrl[1][6], payload_ctrl[1][7], payload_ctrl[1][8]);
	return 0;
}

int lge_mdss_dsi_screen_color_mode_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo = NULL;
	enum lge_gamma_correction_mode gc_mode = LGE_GC_MOD_NOR;

	if (ctrl == NULL) {
        pr_err("Invalid input\n");
		return -ENODEV;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo == NULL) {
		pr_err("%s: invalid panel_info \n", __func__);
		return -ENODEV;
	}

	switch (pinfo->screen_color_mode) {
		case LGE_COLOR_OPT:
			break;
		case LGE_COLOR_CIN:
			gc_mode = LGE_GC_MOD_CIN;
			break;
		case LGE_COLOR_SPO:
			gc_mode = LGE_GC_MOD_SPO;
			break;
		case LGE_COLOR_GAM:
			gc_mode = LGE_GC_MOD_GAM;
			break;
		case LGE_COLOR_MAN:
			break;
		default:
			break;
	}

	pinfo->gc_mode = gc_mode;

	lge_mdss_dsi_rgb_tune_set(ctrl);
	lge_mdss_dsi_screen_tune_set(ctrl);

    return 0;
}
#endif
