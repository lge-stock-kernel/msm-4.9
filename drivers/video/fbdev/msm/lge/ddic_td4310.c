#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include "mdss_dsi.h"

static inline struct dsi_cmd_desc* find_cmd(struct dsi_cmd_desc *cmds, int cnt, char cmd)
{
	int i = 0;

	if (cmds == NULL)
		return NULL;

	for (i = 0; i < cnt; ++i) {
		if (cmds[i].payload[0] == cmd)
			return &cmds[i];
	}
	return NULL;
}

static inline void change_cmd_data(struct dsi_cmd_desc *cmd, char *data, unsigned int len)
{
	int i = 0;

	if (cmd == NULL || data == NULL)
		return;

	if (len > cmd->dchdr.dlen-1)
		return;

	for (i = 0; i < len; ++i) {
		cmd->payload[i+1] = data[i];
	}
}

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
	struct dcs_cmd_req cmdreq = {0,};

	change_cabc_cmd_data(cabc_cmds, CABC_CMDS_NUM, cabc);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds_cnt = CABC_CMDS_NUM;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	cmdreq.cmds = cabc_cmds;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
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

static struct lge_ddic_ops ddic_ops_td4310 = {
	.op_cabc_get = cabc_get_td4310,
	.op_cabc_set = cabc_set_td4310,
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
