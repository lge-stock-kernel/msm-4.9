#ifndef LGE_MDSS_DSI_H
#define LGE_MDSS_DSI_H

struct lge_supply_entry {
	char name[32];
};

struct lge_gpio_entry {
	char name[32];
	int gpio;
};

struct lge_cmds_entry {
	char name[32];
	struct dsi_panel_cmds cmds;
};

struct lge_ddic_ops {
	// CABC
	int (*op_cabc_get)(struct mdss_dsi_ctrl_pdata *ctrl);
	void (*op_cabc_set)(struct mdss_dsi_ctrl_pdata *ctrl, int cabc);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
	// COLOR MANAGER
	void (*op_screen_color_mode_set)(struct mdss_dsi_ctrl_pdata *ctrl, int mode);
	void (*op_rgb_tune_set)(struct mdss_dsi_ctrl_pdata *ctrl, int preset, int r, int g, int b);
	void (*op_screen_tune_set)(struct mdss_dsi_ctrl_pdata *ctrl, int sat, int hue, int sha);
#endif
};

struct lge_mdss_dsi_ctrl_pdata {
	/* panel type for minios */
	char panel_type[MDSS_MAX_PANEL_LEN];

	/* gpio */
	int num_gpios;
	struct lge_gpio_entry *gpio_array;

	/* esc_clk_rate */
	int esc_clk_rate;

	/* lp11 during lcd power off */
	char lp11_off;

	/* delay */
	int pre_on_cmds_delay;
	int post_ldo_on_delay;
	int pre_bl_on_delay;

	/* cmds */
	int num_extra_cmds;
	struct lge_cmds_entry *extra_cmds_array;

	/* multi support panel */
	int panel_id;

	/* enable cabc */
	int cabc_status;

	// DDIC ops
	struct lge_ddic_ops *ddic_ops;
};

#define LGE_DDIC_OP_CHECK(c, op) (c && c->lge_extra.ddic_ops && c->lge_extra.ddic_ops->op_##op)
#define LGE_DDIC_OP(c, op, ...) (LGE_DDIC_OP_CHECK(c,op)?c->lge_extra.ddic_ops->op_##op(c, ##__VA_ARGS__):-ENODEV)
#define LGE_DDIC_OP_LOCKED(c, op, lock, ...) do { \
	mutex_lock(lock); \
	if (LGE_DDIC_OP_CHECK(c,op)) c->lge_extra.ddic_ops->op_##op(c, ##__VA_ARGS__); \
	mutex_unlock(lock); } while(0)

#define LGE_MDELAY(m) do { if ( m > 0) usleep_range((m)*1000,(m)*1000); } while(0)
#define LGE_OVERRIDE_VALUE(x, v) do { if ((v)) (x) = (v); } while(0)

#include "lge/lge_mdss_dsi_panel.h"
#include "lge/lge_mdss_sysfs.h"
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
#include "lge/lge_color_manager.h"
#endif

int lge_mdss_dsi_parse_extra_params(struct platform_device *ctrl_pdev,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata);
int lge_mdss_dsi_init_extra_pm(struct platform_device *ctrl_pdev,
        struct device_node *pan_node, struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void lge_mdss_dsi_deinit_extra_pm(struct platform_device *pdev,
        struct mdss_dsi_ctrl_pdata *ctrl_pdata);
void lge_extra_gpio_set_value(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	const char *name, int value);
int lge_extra_gpio_request(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	const char *name);
void lge_extra_gpio_free(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	const char *name);
int lge_extra_find_gpio_by_name(struct mdss_dsi_ctrl_pdata *ctrl_pdata, const char *name);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_OVERRIDE_MDSS_DSI_CTRL_SHUTDOWN)
void mdss_dsi_ctrl_shutdown(struct platform_device *pdev);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_TOUCH_NOTIFIER_CALL_CHAIN)
int lge_get_lpwg_off_event(void);
int lge_get_lpwg_on_event(void);
#endif
int lge_mdss_dsi_pre_event_handler(struct mdss_panel_data *pdata, int event, void *arg);
int lge_mdss_dsi_post_event_handler(struct mdss_panel_data *pdata, int event, void *arg);
int lge_mdss_dsi_panel_power_seq_all(void);
int lge_get_panel_recovery_flag(void);
void lge_set_panel_recovery_flag(int flag);
#if IS_ENABLED(CONFIG_LGE_DISPLAY_HT_LCD_TUNE_MODE)
void lcd_tune_hightemp(int ht_val);
#endif
int register_ddic_ops(const char *name, struct lge_ddic_ops *ddic_ops);
void unregister_ddic_ops(struct lge_ddic_ops *ddic_ops);
struct lge_ddic_ops *find_ddic_ops(const char *name);
#endif
