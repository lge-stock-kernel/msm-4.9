#define pr_fmt(fmt)	"[Display] %s: " fmt, __func__

#include "lge_mdss_sysfs.h"

struct class *panel = NULL;					/* lge common class node "/sys/class/panel/" */
struct device *lge_panel_sysfs_dev = NULL;	/* lge common device node "/sys/class/panel/dev0/" */
struct device *lge_panel_sysfs_imgtune = NULL; /* lge common img tune node "/sys/class/panel/img_tune/" */
struct fb_info *fbi = NULL;
struct msm_fb_data_type *mfd = NULL;
struct mdss_panel_data *pdata = NULL;
struct mdss_dsi_ctrl_pdata *ctrl = NULL;

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
extern ssize_t lge_get_multi_panel_support_flag(struct device *dev, struct device_attribute *attr, char *buf);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
extern ssize_t get_lge_debug_event(struct device *dev, struct device_attribute *attr, char *buf);
extern ssize_t set_lge_debug_event(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
ssize_t mdss_fb_get_panel_type(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata = dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	int ret = snprintf(buf, PAGE_SIZE, "%s\n", ctrl->lge_extra.panel_type);

	return ret;
}

ssize_t lge_get_multi_panel_support_flag(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0, val = 0;

	fbi = dev_get_drvdata(dev);
	if (fbi == NULL) {
		pr_err("uninitialzed fb0\n");
		return -EINVAL;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	if (mfd == NULL) {
		pr_err("uninitialzed mfd\n");
		return -EINVAL;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected!\n");
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (!ctrl) {
		pr_err("ctrl is null\n");
		return -EINVAL;
	}

	val = BIT(ctrl->lge_extra.panel_id);
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", val);

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY)
extern int lge_mdss_report_panel_dead(int reset_type);

static ssize_t mdss_fb_set_lcd_reset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 value;

	value = simple_strtoul(buf, NULL, 10);

	lge_mdss_report_panel_dead(value);
	return count;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_HT_LCD_TUNE_MODE)
ssize_t set_ht_lcd_tune(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	int value = 0;

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("no panel connected!\n");
		return len;
	}

	if (pinfo->cont_splash_enabled) {
		pr_err("cont_splash_enabled\n");
		return len;
	}

	if (mfd->panel_power_state == MDSS_PANEL_POWER_OFF) {
		pr_err("Panel is off\n");
		return len;
	}
	value = simple_strtol(buf, NULL, 10);
	lcd_tune_hightemp(value);
	return len;
}
#endif

#if defined(CONFIG_LGE_DISPLAY_BL_SP_MIRRORING)
static ssize_t mdss_fb_get_sp_link_backlight_off(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "sp link backlight status : %d\n", sp_link_backlight_status);
	return ret;
}

static ssize_t mdss_fb_set_sp_link_backlight_off(struct device *dev,
		struct device_attribute *attr,const char *buf,  size_t count)
{
	if(!count || !sp_link_backlight_is_ready ||!dev) {
		pr_warn("invalid value : %d, %d || NULL check\n", (int) count, sp_link_backlight_is_ready);
		return -EINVAL;
	}

	fbi = dev_get_drvdata(dev);
	mfd = fbi->par;

	sp_link_backlight_status = simple_strtoul(buf, NULL, 10);
	if(sp_link_backlight_status) {
		pr_info("status : %d, brightness : 0 \n", sp_link_backlight_status);
		mdss_fb_set_backlight(mfd, 0);
	} else {
		pr_info("status : %d, brightness : %d \n", sp_link_backlight_status, sp_link_backlight_brightness);
		mdss_fb_set_backlight(mfd, sp_link_backlight_brightness);
	}

	return count;
}
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
static ssize_t screen_color_mode_get(struct device *dev,
		        struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[screen_color_mode] no panel connected!\n");
		return -EINVAL;
	}
	return sprintf(buf, "%d\n", pinfo->screen_color_mode);
}

static ssize_t screen_color_mode_set(struct device *dev,
		        struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_data *pdata;

	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input_param;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[screen_color_mode] no panel connected!\n");
		return len;
	}

	sscanf(buf, "%d", &input_param);
	pinfo->screen_color_mode = input_param;
	pr_info("screen_color_mode : %d \n", pinfo->screen_color_mode);
	lge_mdss_dsi_screen_color_mode_set(ctrl);

	return ret;
}

static ssize_t rgb_tune_get(struct device *dev,
		        struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[rgb_tune] no panel connected!\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d %d %d %d \n", pinfo->cm_preset_step,
		pinfo->cm_red_step, pinfo->cm_green_step, pinfo->cm_blue_step);
}

static ssize_t rgb_tune_set(struct device *dev,
		        struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_data *pdata;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input_param[4];

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[rgb_tune] no panel connected!\n");
		return len;
	}

	sscanf(buf, "%d %d %d %d", &input_param[0], &input_param[1], &input_param[2], &input_param[3]);
	pinfo->cm_preset_step = input_param[0];
	pinfo->cm_red_step    = abs(input_param[1]);
	pinfo->cm_green_step  = abs(input_param[2]);
	pinfo->cm_blue_step   = abs(input_param[3]);

	if (pinfo->cm_preset_step > 4)
		pinfo->cm_preset_step = 4;

	pr_info("preset : %d , red = %d , green = %d , blue = %d \n",
		pinfo->cm_preset_step, pinfo->cm_red_step, pinfo->cm_green_step, pinfo->cm_blue_step);
	lge_mdss_dsi_rgb_tune_set(ctrl);

	return ret;
}

static ssize_t screen_tune_get(struct device *dev,
		        struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;

	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[screen_tune] no panel connected!\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d %d %d \n", pinfo->sc_sat_step, pinfo->sc_hue_step, pinfo->sc_sha_step);

}

static ssize_t screen_tune_set(struct device *dev,
		        struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_info *pinfo;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_data *pdata;

	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int input_param[3];

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pinfo = mfd->panel_info;

	if (!pinfo) {
		pr_err("[screen_tune] no panel connected!\n");
		return len;
	}

	sscanf(buf, "%d %d %d", &input_param[0], &input_param[1], &input_param[2]);
		pinfo->sc_sat_step 	= abs(input_param[0]);
	pinfo->sc_hue_step 	= abs(input_param[1]);
	pinfo->sc_sha_step 	= abs(input_param[2]);

	pr_info("sat : %d , hue = %d , sha = %d \n", pinfo->sc_sat_step, pinfo->sc_hue_step, pinfo->sc_sha_step);
	lge_mdss_dsi_screen_tune_set(ctrl);

	return ret;
}
#endif

static ssize_t get_panel_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_timing *pt = NULL;
	int cnt = 0;

	if (!fbi) {
		pr_err("fbi is NULL\n");
		return -ENODEV;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	if (!mfd) {
		pr_err("mfd is NULL\n");
		return -ENODEV;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel data!\n");
		return -ENODEV;
	}

	sprintf(buf, "available modes:\n");
	if (list_empty(&pdata->timings_list)) {
		sprintf(buf+strlen(buf), "None\n");
	} else {
		list_for_each_entry(pt, &pdata->timings_list, list) {
			sprintf(buf+strlen(buf), "%s[%d] %s\n", pdata->current_timing==pt?">":" ", cnt++, pt->name);
		}
	}

	return strlen(buf);

}

extern int mdss_dsi_panel_timing_switch(struct mdss_dsi_ctrl_pdata *ctrl, struct mdss_panel_timing *timing);
static ssize_t set_panel_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = NULL;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_timing *pt = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int cnt = 0;
	u32 value = 0;
	bool switched = false;

	if (!fbi) {
		pr_err("fbi is NULL\n");
		return -ENODEV;
	}

	mfd = (struct msm_fb_data_type *)fbi->par;
	if (!mfd) {
		pr_err("mfd is NULL\n");
		return -ENODEV;
	}

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel data!\n");
		return -ENODEV;
	}

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("no panel connected!\n");
		return len;
	}

	if (list_empty(&pdata->timings_list)) {
		pr_err("no panel mode\n");
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	value = simple_strtoul(buf, NULL, 10);

	list_for_each_entry(pt, &pdata->timings_list, list) {
		if (value == cnt++) {
			if (!mdss_dsi_panel_timing_switch(ctrl, pt)) {
				switched = true;
				if (pinfo->debugfs_info)
					pinfo->debugfs_info->panel_info = *pinfo;
#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY)
				lge_mdss_report_panel_dead(1);
#endif
			}
			break;
		}
	}

	pr_info("%s\n", switched?"succeed":"failed");

	return len;
}

static ssize_t cabc_get(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata = dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	return sprintf(buf, "%d", LGE_DDIC_OP(ctrl, cabc_get));
}

static ssize_t cabc_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	struct mdss_panel_data *pdata = dev_get_platdata(&mfd->pdev->dev);
	struct mdss_dsi_ctrl_pdata *ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	int cabc = 0;
	int rc = kstrtoint(buf, 10, &cabc);
	if (rc) {
		pr_err("invalid parameter: %s\n", buf);
	} else {
		// set cabc
		LGE_DDIC_OP(ctrl, cabc_set, cabc);
	}
	return ret;
}

#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_USE_BLMAP)
ssize_t mdss_fb_set_blmap_index(
        struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t len);
ssize_t mdss_fb_get_blmap_index(
        struct device *dev,
        struct device_attribute *attr,
        char *buf);
ssize_t mdss_fb_set_blmap_value(
        struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t len);
ssize_t mdss_fb_get_blmap_value(
        struct device *dev,
        struct device_attribute *attr,
        char *buf);
ssize_t mdss_fb_get_blmap_size(
        struct device *dev,
        struct device_attribute *attr,
        char *buf);
#endif

#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
static DEVICE_ATTR(panel_flag, S_IRUGO, lge_get_multi_panel_support_flag, NULL);
static DEVICE_ATTR(mode, S_IRUGO|S_IWUSR, get_panel_mode, set_panel_mode);
static DEVICE_ATTR(panel_type, S_IRUGO, mdss_fb_get_panel_type, NULL);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY)
static DEVICE_ATTR(lcd_reset, S_IRUGO | S_IWUSR | S_IWGRP,  NULL, mdss_fb_set_lcd_reset);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_DEBUG)
static DEVICE_ATTR(lge_debug_event, S_IRUGO | S_IWUSR, get_lge_debug_event, set_lge_debug_event);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_HT_LCD_TUNE_MODE)
static DEVICE_ATTR(ht_lcd_tune, S_IWUSR|S_IRUGO, NULL, set_ht_lcd_tune);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_SP_MIRRORING)
static DEVICE_ATTR(sp_link_backlight_off, S_IRUGO | S_IWUSR,
	mdss_fb_get_sp_link_backlight_off, mdss_fb_set_sp_link_backlight_off);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_USE_BLMAP)
static DEVICE_ATTR(blmap_index, S_IRUGO | S_IWUSR, mdss_fb_get_blmap_index, mdss_fb_set_blmap_index);
static DEVICE_ATTR(blmap_value, S_IRUGO | S_IWUSR, mdss_fb_get_blmap_value, mdss_fb_set_blmap_value);
static DEVICE_ATTR(blmap_size, S_IRUGO, mdss_fb_get_blmap_size, NULL);
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
static DEVICE_ATTR(screen_mode, S_IRUGO|S_IWUSR|S_IWGRP, screen_color_mode_get, screen_color_mode_set);
static DEVICE_ATTR(rgb_tune, S_IRUGO|S_IWUSR|S_IWGRP, rgb_tune_get, rgb_tune_set);
static DEVICE_ATTR(screen_tune, S_IRUGO|S_IWUSR|S_IWGRP, screen_tune_get, screen_tune_set);
#endif
static DEVICE_ATTR(cabc, S_IRUGO|S_IWUSR|S_IWGRP, cabc_get, cabc_set);

/* "/sys/class/panel/dev0/" */
struct attribute *lge_mdss_panel_sysfs_list[] = {
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
	&dev_attr_panel_flag.attr,
	&dev_attr_mode.attr,
#endif
	NULL,
};

/* "/sys/class/graphics/fb0/" */
struct attribute *lge_mdss_fb_sysfs_list[] = {
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMMON)
	&dev_attr_panel_type.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY)
	&dev_attr_lcd_reset.attr,
#endif
#if defined(CONFIG_LGE_DISPLAY_BL_SP_MIRRORING)
		&dev_attr_sp_link_backlight_off.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_BL_USE_BLMAP)
	&dev_attr_blmap_index.attr,
	&dev_attr_blmap_value.attr,
	&dev_attr_blmap_size.attr,
#endif
	NULL,
};

/* "/sys/class/panel/img_tune/" */
struct attribute *lge_mdss_imgtune_sysfs_list[] = {
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COMFORT_MODE)
	&dev_attr_comfort_view.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_HT_LCD_TUNE_MODE)
	&dev_attr_ht_lcd_tune.attr,
#endif
#if IS_ENABLED(CONFIG_LGE_DISPLAY_COLOR_MANAGER)
	&dev_attr_screen_mode.attr,
	&dev_attr_rgb_tune.attr,
	&dev_attr_screen_tune.attr,
#endif
	&dev_attr_cabc.attr,
	NULL,
};

static struct attribute_group lge_mdss_fb_sysfs_group = {
	.attrs = lge_mdss_fb_sysfs_list,
};

static struct attribute_group lge_mdss_panel_sysfs_group = {
	.attrs = lge_mdss_panel_sysfs_list,
};

static struct attribute_group lge_mdss_imgtune_sysfs_group = {
	.attrs = lge_mdss_imgtune_sysfs_list
};

int lge_mdss_sysfs_init(struct fb_info *fbi)
{
	int ret = 0;

	if(!panel) {
		panel = class_create(THIS_MODULE, "panel");
		if (IS_ERR(panel))
			pr_err("Failed to create panel class\n");
	}

	if(!lge_panel_sysfs_imgtune) {
		lge_panel_sysfs_imgtune = device_create(panel, NULL, 0, fbi, "img_tune");
		if(IS_ERR(lge_panel_sysfs_imgtune)) {
			pr_err("Failed to create dev(lge_panel_sysfs_imgtune)!");
		}
		else {
			ret += sysfs_create_group(&lge_panel_sysfs_imgtune->kobj,
					&lge_mdss_imgtune_sysfs_group);
		}
	}

	if(!lge_panel_sysfs_dev) {
		lge_panel_sysfs_dev = device_create(panel, NULL, 0, fbi, "dev0");
		if (IS_ERR(lge_panel_sysfs_dev)) {
			pr_err("Failed to create lge_panel_sysfs_dev class\n");
		}else{
			ret += sysfs_create_group(&lge_panel_sysfs_dev->kobj, &lge_mdss_panel_sysfs_group);
		}
	}
	ret += sysfs_create_group(&fbi->dev->kobj, &lge_mdss_fb_sysfs_group);

	if (ret)
		return ret;

	return 0;
}
