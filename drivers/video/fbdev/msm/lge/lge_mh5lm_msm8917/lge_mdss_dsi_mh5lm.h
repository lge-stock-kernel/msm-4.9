#ifndef LGE_MDSS_DSI_MH5LM
#define LGE_MDSS_DSI_MH5LM

int mdss_dsi_panel_power_on_ili9881c(struct mdss_panel_data *pdata);
int mdss_dsi_panel_power_off_ili9881c(struct mdss_panel_data *pdata);
void mdss_dsi_ctrl_shutdown_ili9881c(struct platform_device *pdev);
int mdss_dsi_panel_reset_ili9881c(struct mdss_panel_data *pdata, int enable);

int mdss_dsi_panel_power_on_jd9365z(struct mdss_panel_data *pdata);
int mdss_dsi_panel_power_off_jd9365z(struct mdss_panel_data *pdata);
void mdss_dsi_ctrl_shutdown_jd9365z(struct platform_device *pdev);
int mdss_dsi_panel_reset_jd9365z(struct mdss_panel_data *pdata, int enable);
#endif
