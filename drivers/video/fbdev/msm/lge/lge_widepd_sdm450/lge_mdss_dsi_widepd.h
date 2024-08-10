#ifndef LGE_MDSS_DSI_WIDEPD
#define LGE_MDSS_DSI_WIDEPD
int anx7625_widepd_mdss_dsi_panel_power_on(struct mdss_panel_data *pdata);
int anx7625_widepd_mdss_dsi_panel_power_off(struct mdss_panel_data *pdata);
void anx7625_widepd_mdss_dsi_ctrl_shutdown(struct platform_device *pdev);
void anx7625_widepd_panel_enter_deep_sleep(void);
void anx7625_widepd_panel_exit_deep_sleep(void);
int anx7625_widepd_mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable);
#endif
