#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/stddef.h>
#include <linux/fb.h>
#include <linux/kobject.h>

extern struct fb_info *get_fb_info_primary(void);

#if defined(CONFIG_PXLW_IRIS3)
void lge_report_iris_ready(void)
{
	char *envp[2] = {"PANEL_ALIVE=1", NULL};
	struct fb_info *fbi = get_fb_info_primary();

	kobject_uevent_env(&fbi->dev->kobj, KOBJ_CHANGE, envp);
	pr_err("Iris ready\n");
}
#endif
