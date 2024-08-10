/*
 * mip4_notify.c
 *
 * Copyright (c) 2020 LGE.
 *
 * author : sunghwan84.jang@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/fb.h>
//#include <linux/notifier.h>
#include <linux/input/melfas_mip4_notify.h>
#include <linux/export.h>

static BLOCKING_NOTIFIER_HEAD(mip4_fb_notifier);

/**
 *	mip4_fb_register_client - register a client notifier
 *	@nb: notifier block to callback on events
 */
int mip4_fb_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mip4_fb_notifier, nb);
}
EXPORT_SYMBOL(mip4_fb_register_client);

/**
 *	mip4_fb_unregister_client - unregister a client notifier
 *	@nb: notifier block to callback on events
 */
int mip4_fb_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&mip4_fb_notifier, nb);
}
EXPORT_SYMBOL(mip4_fb_unregister_client);

/**
 * mip4_fb_notifier_call_chain - notify clients of fb_events
 *
 */
int mip4_fb_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mip4_fb_notifier, val, v);
}
EXPORT_SYMBOL_GPL(mip4_fb_notifier_call_chain);
