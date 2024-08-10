
#ifndef _LGE_MDSS_DSI_PANEL_H
#define _LGE_MDSS_DSI_PANEL_H

int lge_mdss_panel_parse_dt_extra(struct device_node *np,
                        struct mdss_dsi_ctrl_pdata *ctrl_pdata);

void lge_mdss_dsi_panel_extra_cmds_read(struct mdss_dsi_ctrl_pdata *ctrl, const char *name);
void lge_mdss_dsi_panel_extra_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, const char *name);
struct dsi_panel_cmds *lge_get_extra_cmds_by_name(struct mdss_dsi_ctrl_pdata *ctrl_pdata, char *name);

struct mdss_dsi_ctrl_pdata *lge_mdss_dsi_get_ctrl_pdata(void);
void lge_mdss_dsi_store_ctrl_pdata(struct mdss_dsi_ctrl_pdata *pdata);
void lge_mdss_dsi_pr_status_buf(struct mdss_dsi_ctrl_pdata *ctrl);

void lge_mdss_panel_dic_reg_dump(struct mdss_dsi_ctrl_pdata *pdata);

int lge_mdss_panel_select_initial_cmd_set(struct mdss_dsi_ctrl_pdata *ctrl);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
int lge_mdss_dsi_screen_color_mode_set(struct mdss_dsi_ctrl_pdata *ctrl);
int lge_mdss_dsi_rgb_tune_set(struct mdss_dsi_ctrl_pdata *ctrl);
int lge_mdss_dsi_screen_tune_set(struct mdss_dsi_ctrl_pdata *ctrl);
#endif
#endif
