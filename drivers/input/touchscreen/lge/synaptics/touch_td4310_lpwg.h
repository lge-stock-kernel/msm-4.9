/* touch_synaptics.h
 *
 * Copyright (C) 2019 LGE.
 *
 * Author: BSP-TOUCH@lge.com
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

#ifndef LGE_TOUCH_SYNAPTICS_LPWG_H
#define LGE_TOUCH_SYNAPTICS_LPWG_H

/* LPWG */
#define TCI_REPORT_ENABLE_REG		(d->f12.dsc.control_base + 7)
#define TCI_REPORT_ENABLE_CLEAR_MASK	0xfc
#define TCI_REPORT_ENABLE_BIT		0x2

#define LPWG_INT_STATUS_REG		(d->f51.dsc.data_base)
#define LPWG_INT_DOUBLETAP		(1U << 0)
#define LPWG_INT_PASSWORD		(1U << 1)

#define LPWG_DATA_REG			(d->f51.dsc.data_base + 1)
#define LPWG_OVERTAP_REG		(d->f51.dsc.data_base + 73)

#define LPWG_TAPCOUNT_REG		(d->f51.dsc.control_base)
#define LPWG_MIN_INTERTAP_REG		(d->f51.dsc.control_base + 1)
#define LPWG_MAX_INTERTAP_REG		(d->f51.dsc.control_base + 2)
#define LPWG_TOUCH_SLOP_REG		(d->f51.dsc.control_base + 3)
#define LPWG_TAP_DISTANCE_REG		(d->f51.dsc.control_base + 4)
#define LPWG_INTERRUPT_DELAY_REG	(d->f51.dsc.control_base + 6)

#define LPWG_TAPCOUNT_REG2		(d->f51.dsc.control_base + 7)
#define LPWG_MIN_INTERTAP_REG2		(d->f51.dsc.control_base + 8)
#define LPWG_MAX_INTERTAP_REG2		(d->f51.dsc.control_base + 9)
#define LPWG_TOUCH_SLOP_REG2		(d->f51.dsc.control_base + 10)
#define LPWG_TAP_DISTANCE_REG2		(d->f51.dsc.control_base + 11)
#define LPWG_INTERRUPT_DELAY_REG2	(d->f51.dsc.control_base + 13)

#if 1
/* REAL TIME FAIL REASON */
#define LPWG_FAIL_REASON_ENABLE_REG	(d->f51.dsc.control_base + 15)
#define LPWG_FAIL_REASON_DATA_REG	(d->f51.dsc.data_base + 74)
#define MAX_NUM_OF_FAIL_REASON		2
#else /* alternative to real time fail reason */
/* BUFFER TYPE FAIL REASON */
#define LPWG_MAX_BUFFER			10

#define LPWG_TCI1_FAIL_COUNT_REG	(d->f51.dsc.data_base + 49)
#define LPWG_TCI1_FAIL_INDEX_REG	(d->f51.dsc.data_base + 50)
#define LPWG_TCI1_FAIL_BUFFER_REG	(d->f51.dsc.data_base + 51)

#define LPWG_TCI2_FAIL_COUNT_REG	(d->f51.dsc.data_base + 61)
#define LPWG_TCI2_FAIL_INDEX_REG	(d->f51.dsc.data_base + 62)
#define LPWG_TCI2_FAIL_BUFFER_REG	(d->f51.dsc.data_base + 63)
#endif

enum {
	FAIL_DISTANCE_INTER_TAP = 1,
	FAIL_DISTANCE_TOUCHSLOP,
	FAIL_TIMEOUT_INTER_TAP,
	FAIL_MULTI_FINGER,
	FAIL_DELAY_TIME,
	FAIL_PALM_STATE,
	FAIL_ACTIVE_AREA,
	FAIL_TAP_COUNT,
};

enum {
	TCI_REPORT_NOT_SET = -1,
	TCI_REPORT_DISABLE,
	TCI_REPORT_ENABLE,
};

int td4310_irq_lpwg(struct device *dev);
int td4310_lpwg_mode(struct device *dev);
int td4310_lpwg(struct device *dev, u32 code, void *param);
void td4310_init_tci_info(struct device *dev);
int td4310_swipe_enable(struct device *dev, bool enable);
int td4310_lpwg_register_sysfs(struct device *dev);

#endif /* LGE_TOUCH_SYNAPTICS_LPWG_H */
