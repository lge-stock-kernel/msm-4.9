#ifndef LGE_MDSS_DSI_MH4
#define LGE_MDSS_DSI_MH4
int ili9881h_mdss_dsi_panel_power_on(struct mdss_panel_data *pdata);
int ili9881h_mdss_dsi_panel_power_off(struct mdss_panel_data *pdata);
void ili9881h_mdss_dsi_ctrl_shutdown(struct platform_device *pdev);
void ili9881h_panel_enter_deep_sleep(void);
void ili9881h_panel_exit_deep_sleep(void);
int ili9881h_mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable);

int cpt_ft8006p_mdss_dsi_panel_power_on(struct mdss_panel_data *pdata);
int cpt_ft8006p_mdss_dsi_panel_power_off(struct mdss_panel_data *pdata);
void cpt_ft8006p_mdss_dsi_ctrl_shutdown(struct platform_device *pdev);
void cpt_ft8006p_panel_enter_deep_sleep(void);
void cpt_ft8006p_panel_exit_deep_sleep(void);
int cpt_ft8006p_mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable);

int boe_ft8006p_mdss_dsi_panel_power_on(struct mdss_panel_data *pdata);
int boe_ft8006p_mdss_dsi_panel_power_off(struct mdss_panel_data *pdata);
void boe_ft8006p_mdss_dsi_ctrl_shutdown(struct platform_device *pdev);
void boe_ft8006p_panel_enter_deep_sleep(void);
void boe_ft8006p_panel_exit_deep_sleep(void);
int boe_ft8006p_mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable);
#endif
