#include <linux/of_gpio.h>
#include "mdss_dsi.h"
#include "lge_mdss_dsi.h"
#if IS_ENABLED(CONFIG_LGE_DISPLAY_MFTS)
#include "mfts_mode.h"
#endif

static int lge_mdss_dsi_parse_gpio_params(struct platform_device *ctrl_pdev,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc, i;
	const char *name;
	char buf[256];

	rc = of_property_count_strings(ctrl_pdev->dev.of_node, "lge,extra-gpio-names");
	if (rc > 0) {
		ctrl_pdata->lge_extra.num_gpios = rc;
		pr_info("%s: num_gpios=%d\n", __func__, ctrl_pdata->lge_extra.num_gpios);
		ctrl_pdata->lge_extra.gpio_array = kmalloc(sizeof(struct lge_gpio_entry)*ctrl_pdata->lge_extra.num_gpios, GFP_KERNEL);
		if (NULL == ctrl_pdata->lge_extra.gpio_array) {
			pr_err("%s: no memory\n", __func__);
			ctrl_pdata->lge_extra.num_gpios = 0;
			return -ENOMEM;
		}
		for (i = 0; i < ctrl_pdata->lge_extra.num_gpios; ++i) {
			of_property_read_string_index(ctrl_pdev->dev.of_node, "lge,extra-gpio-names", i, &name);
			strlcpy(ctrl_pdata->lge_extra.gpio_array[i].name, name, sizeof(ctrl_pdata->lge_extra.gpio_array[i].name));
			snprintf(buf, sizeof(buf), "lge,gpio-%s", name);
			ctrl_pdata->lge_extra.gpio_array[i].gpio = of_get_named_gpio(ctrl_pdev->dev.of_node, buf, 0);
			if (!gpio_is_valid(ctrl_pdata->lge_extra.gpio_array[i].gpio))
				pr_err("%s: %s not specified\n", __func__, buf);
		}
	} else {
		ctrl_pdata->lge_extra.num_gpios = 0;
		pr_info("%s: no lge specified gpio\n", __func__);
	}
	return 0;
}

int lge_mdss_dsi_parse_extra_params(struct platform_device *ctrl_pdev,
        struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	lge_mdss_dsi_parse_gpio_params(ctrl_pdev, ctrl_pdata);

	return 0;
}

static int gpio_name_to_index(struct mdss_dsi_ctrl_pdata *ctrl_pdata, const char *name)
{
	int i, index = -1;

	for (i = 0; i < ctrl_pdata->lge_extra.num_gpios; ++i) {
		if (!strcmp(ctrl_pdata->lge_extra.gpio_array[i].name, name)) {
			index = i;
			break;
		}
	}

	return index;
}

int lge_extra_find_gpio_by_name(struct mdss_dsi_ctrl_pdata *ctrl_pdata, const char *name)
{
	int index = gpio_name_to_index(ctrl_pdata, name);
	if (index != -1)
		return ctrl_pdata->lge_extra.gpio_array[index].gpio;
	else
		return -1;
}

void lge_extra_gpio_set_value(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	const char *name, int value)
{
	int index = -1;

	index = gpio_name_to_index(ctrl_pdata, name);

	if (index != -1) {
		gpio_set_value(ctrl_pdata->lge_extra.gpio_array[index].gpio, value);
	} else {
		pr_err("%s: couldn't get gpio by name %s\n", __func__, name);
	}
}

int lge_extra_gpio_request(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	const char *name)
{
	int index = -1;

	index = gpio_name_to_index(ctrl_pdata, name);

	if (index != -1) {
		return gpio_request(ctrl_pdata->lge_extra.gpio_array[index].gpio, name);
	} else {
		pr_err("%s: couldn't get gpio by name %s\n", __func__, name);
		return -EINVAL;
	}
}

void lge_extra_gpio_free(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	const char *name)
{
	int index = -1;

	index = gpio_name_to_index(ctrl_pdata, name);

	if (index != -1) {
		gpio_free(ctrl_pdata->lge_extra.gpio_array[index].gpio);
	} else {
		pr_err("%s: couldn't get gpio by name %s\n", __func__, name);
	}
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_TOUCH_NOTIFIER_CALL_CHAIN)
/*
 * lge_get_lpwg_on_event(), lge_get_lpwg_off_event() returns
 * one of below values:
 *  for LCD_EVENT_TOUCH_LPWG_OFF
 *    MDSS_EVENT_PANEL_ON
 *    MDSS_EVENT_LINK_READY
 *  for LCD_EVENT_TOUCH_LPWG_ON
 *    MDSS_EVENT_PANEL_OFF
 *    MDSS_EVENT_BLANK
 */
__weak int lge_get_lpwg_on_event(void)
{
	return MDSS_EVENT_PANEL_OFF;
}

__weak int lge_get_lpwg_off_event(void)
{
	return MDSS_EVENT_PANEL_ON;
}
#endif

__weak int lge_mdss_dsi_pre_event_handler(struct mdss_panel_data *pdata, int event, void *arg)
{
	return 0;
}

__weak int lge_mdss_dsi_post_event_handler(struct mdss_panel_data *pdata, int event, void *arg)
{
	return 0;
}

__weak int lge_get_panel_recovery_flag()
{
	return 0;
}

__weak void lge_set_panel_recovery_flag(int flag)
{
}

__weak int lge_mdss_dsi_panel_power_seq_all()
{
	int ret = 0;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_MFTS)
	if (lge_get_display_power_ctrl())
		ret = 1;
#endif

	if (lge_get_panel_recovery_flag())
		ret = 1;

	return ret;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_HT_LCD_TUNE_MODE)
void lcd_tune_hightemp(int ht_val)
{
	char ht_cmd_name[256];

	if( ht_val < 0 )
		return;

	pr_info("%s: %d level \n",__func__, ht_val );
	snprintf(ht_cmd_name, sizeof(ht_cmd_name), "temp-%d", ht_val);
	lge_mdss_dsi_panel_extra_cmds_send(NULL, ht_cmd_name);

	return;
}
#endif

struct lge_ddic_ops_entry {
	const char *name;
	struct lge_ddic_ops *ddic_ops;
	struct list_head list;
};

static LIST_HEAD(ddic_ops_list);

int register_ddic_ops(const char *name, struct lge_ddic_ops *ddic_ops)
{
	struct lge_ddic_ops_entry *entry = NULL;

	if (name == NULL || ddic_ops == NULL)
		return -EINVAL;

	if (find_ddic_ops(name)) {
		pr_err("driver for %s is already registered\n", name);
		return -EINVAL;
	}

	entry = kmalloc(sizeof(struct lge_ddic_ops_entry), GFP_KERNEL);
	if (entry == NULL) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}
	entry->name = name;
	entry->ddic_ops = ddic_ops;
	list_add(&entry->list, &ddic_ops_list);
	pr_info("driver for %s is registered\n", name);
	return 0;
}

struct lge_ddic_ops *find_ddic_ops(const char *name)
{
	struct lge_ddic_ops_entry *entry = NULL;

	if (name == NULL)
		return NULL;

	list_for_each_entry(entry, &ddic_ops_list, list) {
		if (!strcmp(name, entry->name))
			return entry->ddic_ops;
	}
	pr_err("driver for %s is not registered\n", name);
	return NULL;
}

void unregister_ddic_ops(struct lge_ddic_ops *ddic_ops)
{
	struct lge_ddic_ops_entry *entry = NULL;

	if (ddic_ops == NULL)
		return;

	list_for_each_entry(entry, &ddic_ops_list, list) {
		if (ddic_ops ==  entry->ddic_ops) {
			list_del(&entry->list);
			pr_info("driver for %s is unregistered\n", entry->name);
			kfree(entry);
			return;
		}
	}
}
