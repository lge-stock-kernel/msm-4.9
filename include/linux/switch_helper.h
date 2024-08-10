#ifndef _SWITCH_HELPER_H_
#define _SWITCH_HELPER_H_

#include <linux/switch.h>

#define DEFINE_SWITCH(s_name) \
struct switch_dev sdev_##s_name = { \
	.name = #s_name,\
}

static inline bool check_gpio_button_data(struct gpio_button_data *bdata)
{
	if (bdata && bdata->button && bdata->button->desc)
		return true;

	return false;
}

static inline void check_switch(struct switch_dev *sdev, struct gpio_button_data *bdata, int state, unsigned int timeout)
{
	if (sdev == NULL || sdev->name == NULL || !check_gpio_button_data(bdata))
		return;

	if (!strncmp(bdata->button->desc, sdev->name, strlen(sdev->name))) {
		pr_info("[switch] %s state = %d\n", sdev->name, state);
		if (sdev->state != state) {
			switch_set_state(sdev, state);
			pr_info("[switch] %s state changed to %d\n", sdev->name, state);
			if(sdev->state == 0) {
				pr_info("[switch] Set %s wakelock\n", sdev->name);
				wake_lock_timeout(&bdata->gpio_irq_wakelock, msecs_to_jiffies(timeout));
			}
		}
	}
}
#define CHECK_SWITCH(s_name, timeout) check_switch(&sdev_##s_name, bdata, state, timeout)

static inline void setup_switch(struct switch_dev *sdev, struct gpio_button_data *bdata, const char *wakelock_name, bool smartcover)
{
	if (sdev == NULL || sdev->name == NULL || !check_gpio_button_data(bdata))
		return;

	pr_info("setup_switch: %s\n", bdata->button->desc);
	if (!strncmp(bdata->button->desc, sdev->name, strlen(sdev->name))) {
		if (switch_dev_register(sdev) < 0) {
			pr_info("[switch] %s switch registration failed\n", sdev->name);
		} else {
			if (smartcover)
				smartcover_dev_register(sdev);
			wake_lock_init(&bdata->gpio_irq_wakelock, WAKE_LOCK_SUSPEND, wakelock_name);
			pr_info("[switch] %s stylus registration succeeded\n", sdev->name);
		}
	}
}
#define SETUP_SWITCH(s_name) setup_switch(&sdev_##s_name, bdata, #s_name "_wakelock", false)
#define SETUP_SMARTCOVER_SWITCH(s_name) setup_switch(&sdev_##s_name, bdata, #s_name "_wakelock", true)

#endif //_SWITCH_HELPER_H_
