
#ifndef _H_LGE_MDSS_DSI_CMD_HELPER_
#define _H_LGE_MDSS_DSI_CMD_HELPER_

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

static inline void change_cmd_data_with_offset(struct dsi_cmd_desc *cmd, int offset, char *data, unsigned int len)
{
	int i = 0;

	if (cmd == NULL || data == NULL)
		return;

	if (len > cmd->dchdr.dlen-1)
		return;

	for (i = 0; i < len; ++i) {
		cmd->payload[i+1+offset] = data[i];
	}
}

static inline void change_cmd_data(struct dsi_cmd_desc *cmd, char *data, unsigned int len)
{
	change_cmd_data_with_offset(cmd, 0, data, len);
}

static inline void send_dsi_cmd(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cmds_cnt)
{
	struct dcs_cmd_req cmdreq = {0,};

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds_cnt = cmds_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	cmdreq.cmds = cmds;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

#endif // _H_LGE_MDSS_DSI_CMD_HELPER_
