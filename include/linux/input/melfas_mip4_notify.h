#ifndef __LINUX_MELFAS_MIP4_NOTIFY_H
#define __LINUX_MELFAS_MIP4_NOTIFY_H

#include <linux/notifier.h>

/**
 * mip4_fb_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *
 * This function registers a notifier callback function
 * to msm_drm_notifier_list, which would be called when
 * received unblank/power down event.
 */
int mip4_fb_register_client(struct notifier_block *nb);

/**
 * mip4_fb_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *
 * This function unregisters the callback function from
 * msm_drm_notifier_list.
 */
int mip4_fb_unregister_client(struct notifier_block *nb);

/**
 * mip4_fb_notifier_call_chain - notify clients of fb_events
 */
int mip4_fb_notifier_call_chain(unsigned long val, void *v);
#endif
