#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include "mdss_dsi.h"
#include "lge_mdss_dsi_cmd_helper.h"

#define CABC_MAX_LEVEL 3
#define CABC_BL_CTRL_DATA_LEN 7
char CABC_BL_CTRL_DATA[CABC_MAX_LEVEL][CABC_BL_CTRL_DATA_LEN] = {
	{0x23, 0x3D, 0x15, 0x1E, 0x01, 0x50, 0x50}, // 10%
	{0x55, 0x3D, 0x20, 0x1E, 0x0C, 0x50, 0x50}, // 25%
	{0x77, 0x3D, 0x2C, 0x1E, 0x18, 0x50, 0x50}, // 35%
};

#define CABC_BL_CTRL_CMD 0xB8
#define CABC_ENABLE_CMD 0x55

#define CABC_DISABLE 0x00
#define CABC_ENABLE 0x81

static char cabc_bl_ctrl_cmd[8] = { 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static char cabc_cmd[2] = { 0x55, 0x00 };
#define CABC_CMDS_NUM 2
static struct dsi_cmd_desc cabc_cmds[CABC_CMDS_NUM] = {
	{ { DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(cabc_bl_ctrl_cmd) }, cabc_bl_ctrl_cmd },
	{ { DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(cabc_cmd) }, cabc_cmd },
};

static void change_cabc_cmd_data(struct dsi_cmd_desc *cmds, int cmd_cnt, int cabc)
{
	struct dsi_cmd_desc *cmd = NULL;
	if (cabc > 0) {
		cmd = find_cmd(cmds, cmd_cnt, CABC_BL_CTRL_CMD);
		if (cmd != NULL) {
			change_cmd_data(cmd, CABC_BL_CTRL_DATA[cabc-1], CABC_BL_CTRL_DATA_LEN);
		}
	}
	cmd = find_cmd(cmds, cmd_cnt, CABC_ENABLE_CMD);
	if (cmd != NULL) {
		cmd->payload[1] = cabc==0?CABC_DISABLE:CABC_ENABLE;
	}
}

static void send_cabc_cmd(struct mdss_dsi_ctrl_pdata *ctrl, int cabc)
{
	change_cabc_cmd_data(cabc_cmds, CABC_CMDS_NUM, cabc);
	send_dsi_cmd(ctrl, cabc_cmds, CABC_CMDS_NUM);
}

static int cabc_get_td4310(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int cabc = 0;
	struct dsi_panel_cmds *on_cmd = NULL;
	struct dsi_cmd_desc *cmd = NULL;
	int i = 0, j = 0;;

	if (ctrl == NULL)
		return -ENODEV;

	on_cmd = &ctrl->on_cmds;
	cmd = find_cmd(on_cmd->cmds, on_cmd->cmd_cnt, CABC_ENABLE_CMD);
	if (cmd == NULL)
		return 0;

	if (cmd->payload[1] != CABC_ENABLE)
		return 0;

	cmd = find_cmd(on_cmd->cmds, on_cmd->cmd_cnt, CABC_BL_CTRL_CMD);
	if (cmd == NULL)
		return 0;
	for (i = 0; i < CABC_MAX_LEVEL; i++) {
		bool match = true;
		for (j = 0; j < CABC_BL_CTRL_DATA_LEN; j++) {
			if (cmd->payload[j+1] != CABC_BL_CTRL_DATA[i][j]) {
				match = false;
				break;
			}
		}
		if (match) {
			cabc = i+1;
			break;
		}
	}
	return cabc;
}

static void cabc_set_td4310(struct mdss_dsi_ctrl_pdata *ctrl, int cabc)
{
	struct dsi_panel_cmds *on_cmd = NULL;

	if (ctrl == NULL)
		return;

	if (cabc < 0 || cabc > CABC_MAX_LEVEL)
		return;

	on_cmd = &ctrl->on_cmds;
	mutex_lock(&ctrl->cmd_mutex);
	change_cabc_cmd_data(on_cmd->cmds, on_cmd->cmd_cnt, cabc);
	mutex_unlock(&ctrl->cmd_mutex);

	if (ctrl->panel_data.panel_info.cont_splash_enabled)
		return;

	send_cabc_cmd(ctrl, cabc);
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
char COLOR_TEMPERATURE_ADJ[5][RGB_ALL] = {
	{72, 0, 82},
	{43, 0, 53},
	{0, 0, 0},
	{5, 29, 0},
	{5, 71, 0},
};

char R_ADJ[5][RGB_ALL] = {
	{0, 0, 0},
	{11, 0, 13},
	{21, 0, 9},
	{43, 0, 4},
	{58, 0, 4},
};

char G_ADJ[5][RGB_ALL] = {
	{0, 0, 0},
	{0, 13, 7},
	{5, 29, 7},
	{8, 45, 7},
	{11, 61, 7},
};

char B_ADJ[5][RGB_ALL] = {
	{0, 0, 0},
	{1, 0, 19},
	{0, 0, 36},
	{3, 4, 57},
	{3, 4, 71},
};

char W_POINT_PRESET[3][RGB_ALL] = {
	{0xFB, 0xDC, 0xC8},
	{0xA8, 0xBA, 0xFC},
	{0xCA, 0xD4, 0xFC},
};

#define INTENSITY_MAT_SIZE 6*3

#define SATURATION_STEP_NUM 5
char SAT2INT[SATURATION_STEP_NUM] = {0x40, 0x20, 0x00, 0xE0, 0xC0};
char COEFFI_SAT[INTENSITY_MAT_SIZE] = {
	0, 1, 1, //R
	1, 0, 1, //G
	1, 1, 0, //B
	1, 0, 0, //C
	0, 1, 0, //M
	0, 0, 1, //Y
};
char COEFFI_HUE_MINUS[INTENSITY_MAT_SIZE] = {
	0, 0, 1, //R
	1, 0, 0, //G
	1, 0, 0, //B
	0, 0, -1, //C
	-1, 0, 0, //M
	0, -1, 0, //Y
};
char COEFFI_HUE_PLUS[INTENSITY_MAT_SIZE] = {
	0, -1, 0, //R
	0, 0, -1, //G
	0, -1, 0, //B
	0, 1, 0, //C
	0, 0, 1, //M
	1, 0, 0, //Y
};
#define K_HUE 0x20

#define COLOR_ENHANCEMENT_CMD 0xCA

#define W_POINT_OFFSET 1
#define W_POINT_DEFAULT 0xFC

#define INTENSITY_6_AXIS_OFFSET 4

#define COLOR_ENHANCEMENT_CMDS_NUM 1
static char color_enhancement_cmd[44] = { COLOR_ENHANCEMENT_CMD,
	0x1D,             //
	0xFC, 0xFC, 0xFC, //W
	0x00, 0x00, 0x00, //R
	0x00, 0x00, 0x00, //G
	0x00, 0x00, 0x00, //B
	0x00, 0x00, 0x00, //C
	0x00, 0x00, 0x00, //M
	0x00, 0x00, 0x00, //Y
	0x00, 0x00, 0x00, //SEL0 BEF
	0x00, 0x00, 0x00, //SEL0 AFT
	0x00,             //SEL0 AREA
	0x00, 0x00, 0x00, //SEL1 BEF
	0x00, 0x00, 0x00, //SEL1 AFT
	0x00,             //SEL1 AREA
	0x00, 0x00, 0x00, //SEL2 BEF
	0x00, 0x00, 0x00, //SEL2 AFT
	0x00,             //SEL2 AREA
};
static struct dsi_cmd_desc color_enhancement_cmds[COLOR_ENHANCEMENT_CMDS_NUM] = {
	{ { DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(color_enhancement_cmd) }, color_enhancement_cmd },
};

#define EDGE_OFFSET 2
#define EDGE_ENHANCEMENT_CMD 0xDD
#define EDGE_ENHANCEMENT_CMDS_NUM 2
static char crosshair_cmd[9] = { 0xC5, 0x08, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01 };
static char edge_enhancement_cmd[5] = { EDGE_ENHANCEMENT_CMD, 0x31, 0x06, 0x23, 0x65 };
static struct dsi_cmd_desc edge_enhancement_cmds[EDGE_ENHANCEMENT_CMDS_NUM] = {
	{ { DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(crosshair_cmd) }, crosshair_cmd },
	{ { DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(edge_enhancement_cmd) }, edge_enhancement_cmd },
};
char SHARPNESS[5] = {0x00, 0x08, 0x10, 0x18, 0x20};

#define PRESET_DEFAULT 2
#define SAT_DEFAULT 2
#define HUE_DEFAULT 2
#define SHA_DEFAULT 2

static int screen_color_mode = LGE_COLOR_OPT;
static int cm_preset_step = PRESET_DEFAULT;
static int cm_red_step = 0;
static int cm_green_step = 0;
static int cm_blue_step = 0;
static int sc_sat_step = SAT_DEFAULT;
static int sc_hue_step = HUE_DEFAULT;
static int sc_sha_step = SHA_DEFAULT;

static void change_w_point(struct dsi_cmd_desc *cmds, int cmd_cnt)
{
	char w_point[RGB_ALL] = {0, 0, 0};
	struct dsi_cmd_desc *cmd = NULL;
	int i = 0;

	switch(screen_color_mode) {
	case LGE_COLOR_OPT:
	case LGE_COLOR_MAN:
		for (i = 0; i < RGB_ALL; i++)
			w_point[i] = W_POINT_DEFAULT - COLOR_TEMPERATURE_ADJ[cm_preset_step][i] - R_ADJ[cm_red_step][i] - G_ADJ[cm_green_step][i] - B_ADJ[cm_blue_step][i];
		break;
	case LGE_COLOR_CIN:
		for (i = 0; i < RGB_ALL; i++)
			w_point[i] = W_POINT_PRESET[0][i];
		break;
	case LGE_COLOR_SPO:
		for (i = 0; i < RGB_ALL; i++)
			w_point[i] = W_POINT_PRESET[1][i];
		break;
	case LGE_COLOR_GAM:
		for (i = 0; i < RGB_ALL; i++)
			w_point[i] = W_POINT_PRESET[2][i];
		break;
	default:
		for (i = 0; i < RGB_ALL; i++)
			w_point[i] = W_POINT_DEFAULT;
		break;
	}

	cmd = find_cmd(cmds, cmd_cnt, COLOR_ENHANCEMENT_CMD);
	if (cmd != NULL) {
		change_cmd_data_with_offset(cmd, W_POINT_OFFSET, w_point, RGB_ALL);
	}
}

static void change_6_axis_intensity(struct dsi_cmd_desc *cmds, int cmd_cnt)
{
	char intensity[INTENSITY_MAT_SIZE] = {0, };
	struct dsi_cmd_desc *cmd = NULL;
	int i = 0;
	int sat = SAT_DEFAULT, hue = HUE_DEFAULT;

	if (screen_color_mode == LGE_COLOR_MAN) {
		sat = sc_sat_step;
		hue = sc_hue_step;
	}

	for (i = 0; i < INTENSITY_MAT_SIZE; ++i) {
		intensity[i] = SAT2INT[sat]*COEFFI_SAT[i];
	}

	hue -= HUE_DEFAULT; // 0 ~ 5 --> -2 ~ 2
	for (i = 0; i < INTENSITY_MAT_SIZE; ++i) {
		intensity[i] += hue*K_HUE*(hue>0?COEFFI_HUE_PLUS[i]:COEFFI_HUE_MINUS[i]);
	}

	cmd = find_cmd(cmds, cmd_cnt, COLOR_ENHANCEMENT_CMD);
	if (cmd != NULL) {
		change_cmd_data_with_offset(cmd, INTENSITY_6_AXIS_OFFSET, intensity, INTENSITY_MAT_SIZE);
	}
}

static void check_ce_enable(struct dsi_cmd_desc *cmds, int cmd_cnt)
{
	int i = 0;
	bool enable = false;
	struct dsi_cmd_desc *cmd = NULL;
	char tmp = 0x00;

	cmd = find_cmd(cmds, cmd_cnt, COLOR_ENHANCEMENT_CMD);
	if (cmd != NULL) {
		for (i = 0; i < RGB_ALL; ++i) {
			if (cmd->payload[1+W_POINT_OFFSET+i] != 0xFC) {
				enable = true;
				break;
			}
		}
		for (i = 0; i < INTENSITY_MAT_SIZE; ++i) {
			if (cmd->payload[1+INTENSITY_6_AXIS_OFFSET+i] != 0x00) {
				enable = true;
				break;
			}
		}

		tmp = cmd->payload[1] & ~0x01;
		if (enable)
			tmp |= 0x01;
		cmd->payload[1] = tmp;
	}
}

static void change_sharpness(struct dsi_cmd_desc *cmds, int cmd_cnt)
{
	struct dsi_cmd_desc *cmd = NULL;
	int sha = SHA_DEFAULT;

	if (screen_color_mode == LGE_COLOR_MAN) {
		sha = sc_sha_step;
	}
	cmd = find_cmd(cmds, cmd_cnt, EDGE_ENHANCEMENT_CMD);
	if (cmd != NULL) {
		cmd->payload[1+EDGE_OFFSET] = SHARPNESS[sha];
	}
}

static void send_image_enhancement_cmd(struct mdss_dsi_ctrl_pdata *ctrl)
{
	change_w_point(color_enhancement_cmds, COLOR_ENHANCEMENT_CMDS_NUM);
	change_6_axis_intensity(color_enhancement_cmds, COLOR_ENHANCEMENT_CMDS_NUM);
	check_ce_enable(color_enhancement_cmds, COLOR_ENHANCEMENT_CMDS_NUM);
	send_dsi_cmd(ctrl, color_enhancement_cmds, COLOR_ENHANCEMENT_CMDS_NUM);
	change_sharpness(edge_enhancement_cmds, EDGE_ENHANCEMENT_CMDS_NUM);
	send_dsi_cmd(ctrl, edge_enhancement_cmds, EDGE_ENHANCEMENT_CMDS_NUM);
}

static void set_image_enhancement(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dsi_panel_cmds *on_cmd = NULL;
	if (ctrl == NULL)
		return;

	on_cmd = &ctrl->on_cmds;
	mutex_lock(&ctrl->cmd_mutex);
	change_w_point(on_cmd->cmds, on_cmd->cmd_cnt);
	change_6_axis_intensity(on_cmd->cmds, on_cmd->cmd_cnt);
	check_ce_enable(on_cmd->cmds, on_cmd->cmd_cnt);
	change_sharpness(on_cmd->cmds, on_cmd->cmd_cnt);
	mutex_unlock(&ctrl->cmd_mutex);

	if (ctrl->panel_data.panel_info.cont_splash_enabled)
		return;

	send_image_enhancement_cmd(ctrl);
}

static void screen_color_mode_set_td4310(struct mdss_dsi_ctrl_pdata *ctrl, int mode)
{
	screen_color_mode = mode;
	pr_info("mode=%d\n", mode);
	set_image_enhancement(ctrl);
}

static void rgb_tune_set_td4310(struct mdss_dsi_ctrl_pdata *ctrl, int preset, int r, int g, int b)
{
	cm_preset_step = preset;
	cm_red_step = r;
	cm_green_step = g;
	cm_blue_step = b;
	pr_info("preset=%d, red=%d, green=%d, blue=%d\n", preset, r, g, b);
	set_image_enhancement(ctrl);
}

static void screen_tune_set_td4310(struct mdss_dsi_ctrl_pdata *ctrl, int sat, int hue, int sha)
{
	sc_sat_step = sat;
	sc_hue_step = hue;
	sc_sha_step = sha;
	pr_info("sat=%d, hue=%d, sha=%d\n", sat, hue, sha);
	set_image_enhancement(ctrl);
}
#endif // CONFIG_LGE_DISPLAY_COLOR_MANAGER

static struct lge_ddic_ops ddic_ops_td4310 = {
	.op_cabc_get = cabc_get_td4310,
	.op_cabc_set = cabc_set_td4310,
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
	.op_screen_color_mode_set = screen_color_mode_set_td4310,
	.op_rgb_tune_set = rgb_tune_set_td4310,
	.op_screen_tune_set = screen_tune_set_td4310,
#endif
};

static int __init td4310_init(void)
{
	return register_ddic_ops("synaptics,td4310", &ddic_ops_td4310);
}

static void __exit td4310_exit(void)
{
	unregister_ddic_ops(&ddic_ops_td4310);
}

module_init(td4310_init);
module_exit(td4310_exit);
