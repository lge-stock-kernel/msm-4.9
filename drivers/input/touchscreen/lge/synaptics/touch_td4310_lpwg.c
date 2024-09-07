/* touch_td4310.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <touch_hwif.h>
#include <touch_core.h>

#include "touch_td4310.h"
#include "touch_td4310_lpwg.h"
#include "touch_td4310_prd.h"

#define NUM_OF_TCI_FR_STR 8
const char *tci_fr_str[NUM_OF_TCI_FR_STR + 1] = {
	[0] = "SUCCESS",
	[1] = "DISTANCE_INTER_TAP",
	[2] = "DISTANCE_TOUCHSLOP",
	[3] = "TIMEOUT_INTER_TAP",
	[4] = "MULTI_FINGER",
	[5] = "DELAY_TIME(OVER_TAP)",
	[6] = "PALM_STATE",
	[7] = "OUT_OF_AREA",
	[8] = "TAP_COUNT",
};

void td4310_init_tci_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->tci.info[TCI_1].tap_count = 2;
	ts->tci.info[TCI_1].min_intertap = 0;
	ts->tci.info[TCI_1].max_intertap = 70;
	ts->tci.info[TCI_1].touch_slop = 100;
	ts->tci.info[TCI_1].tap_distance = 10;
	ts->tci.info[TCI_1].intr_delay = 0;

	ts->tci.info[TCI_2].min_intertap = 0;
	ts->tci.info[TCI_2].max_intertap = 70;
	ts->tci.info[TCI_2].touch_slop = 100;
	ts->tci.info[TCI_2].tap_distance = 255;
	ts->tci.info[TCI_2].intr_delay = 20;
}

static int td4310_tci_report_enable(struct device *dev, bool enable)
{
	struct td4310_data *d = to_td4310_data(dev);
	u8 data[3] = {0,};
	int ret = 0;

	TOUCH_TRACE();

	SYNA_READ(dev, TCI_REPORT_ENABLE_REG, data, sizeof(data), ret, error);

	data[2] &= TCI_REPORT_ENABLE_CLEAR_MASK;
	if (enable)
		data[2] |= TCI_REPORT_ENABLE_BIT;

	SYNA_WRITE(dev, TCI_REPORT_ENABLE_REG, data, sizeof(data), ret, error);

error:
	return ret;
}

int td4310_swipe_enable(struct device *dev, bool enable)
{
	TOUCH_TRACE();
	return 0;
}

static int td4310_lpwg_fail_reason_enable(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);
	u8 data[2] = {0, };
	int ret = 0;

	TOUCH_TRACE();

	if (!d->lpwg_fail_reason)
		return 0;

	ret = td4310_set_page(dev, LPWG_PAGE);
	if (ret < 0) {
		TOUCH_E("set LPWG_PAGE error\n");
		return ret;
	}

	data[0] = 0xFF;
	data[1] = 0xFF;
	SYNA_WRITE(dev, LPWG_FAIL_REASON_ENABLE_REG, data, sizeof(data), ret, error);

error:
	ret = td4310_set_page(dev, DEFAULT_PAGE);
	if (ret < 0)
		TOUCH_E("set DEFAULT_PAGE error\n");

	return ret;
}

static void td4310_lpwg_debug_real_time(struct device *dev, char* fr)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int data[2] = {(int)fr[0], (int)fr[1]};

	TOUCH_TRACE();

	if (fr[0] <= NUM_OF_TCI_FR_STR && fr[1] <= NUM_OF_TCI_FR_STR) {
		if (ts->tci.mode & 0x01)
			TOUCH_I("TCI_1 Fail-Reason : [%d] %s\n", data[0], tci_fr_str[data[0]]);
		if (ts->tci.mode & 0x02)
			TOUCH_I("TCI_2 Fail-Reason : [%d] %s\n", data[1], tci_fr_str[data[1]]);
	} else {
		TOUCH_I("unknown fail reason\n");
	}

	return;
}

#if 0
// alternative to real time fail reason
static int td4310_lpwg_debug_buffer(struct device *dev, int num)
{
	struct td4310_data *d = to_td4310_data(dev);
	u8 count = 0;
	u8 index = 0;
	u8 buf = 0;
	u8 i = 0;
	u8 addr = 0;
	u8 offset = num ? LPWG_MAX_BUFFER + 2 : 0;
	int ret = 0;

	TOUCH_TRACE();

	ret = td4310_set_page(dev, LPWG_PAGE);
	if (ret < 0) {
		TOUCH_E("set LPWG_PAGE error\n");
		return ret;
	}

	SYNA_READ(dev, LPWG_TCI1_FAIL_COUNT_REG + offset, &count, sizeof(count), ret, error);
	SYNA_READ(dev, LPWG_TCI1_FAIL_INDEX_REG + offset, &index, sizeof(index), ret, error);

	for (i = 1 ; i <= count ; i++) {
		addr = LPWG_TCI1_FAIL_BUFFER_REG + offset +
				((index + LPWG_MAX_BUFFER - i) % LPWG_MAX_BUFFER);
		SYNA_READ(dev, addr, &buf, sizeof(buf), ret, error);

		TOUCH_I("TCI_%d Fail-Reason[%d/%d] : [%d] %s\n",
				num, count - i + 1, count, buf,
				(buf > 0 && buf < 6) ? tci_fr_str[buf] : tci_fr_str[0]);

		if (i == LPWG_MAX_BUFFER)
			break;
	}

error:
	ret = td4310_set_page(dev, DEFAULT_PAGE);
	if (ret < 0)
		TOUCH_E("set DEFAULT_PAGE error\n");

	return ret;
}
#endif

static int td4310_tci_getdata(struct device *dev, int count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u32 data[12] = {0, };
	int i = 0;
	int ret = 0;

	TOUCH_TRACE();

	ts->lpwg.code_num = count;

	if (!count)
		return 0;

	SYNA_READ(dev, LPWG_DATA_REG, data, (sizeof(u32) * count), ret, error);

	for (i = 0 ; i < count ; i++) {
		if (data[i] == 0) {
			ts->lpwg.code[i].x = 1;
			ts->lpwg.code[i].y = 1;
		} else {
			ts->lpwg.code[i].x = data[i] & 0xffff;
			ts->lpwg.code[i].y = (data[i] >> 16) & 0xffff;
		}

		if ((ts->lpwg.mode & 0x02) && (ts->role.hide_coordinate))
			TOUCH_I("LPWG data xxxx, xxxx\n");
		else
			TOUCH_I("LPWG data %d, %d\n", ts->lpwg.code[i].x, ts->lpwg.code[i].y);
	}
	ts->lpwg.code[count].x = -1;
	ts->lpwg.code[count].y = -1;

error:
	return ret;
}

int td4310_irq_lpwg(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u8 fr[MAX_NUM_OF_FAIL_REASON] = {0, };
	u8 data = 0;
	int ret = 0;

	TOUCH_TRACE();

	ret = td4310_set_page(dev, LPWG_PAGE);
	if (ret < 0) {
		TOUCH_E("set LPWG_PAGE error\n");
		return ret;
	}

	SYNA_READ(dev, LPWG_INT_STATUS_REG, &data, 1, ret, error);

	if (data & LPWG_INT_DOUBLETAP) {
		td4310_tci_getdata(dev, ts->tci.info[TCI_1].tap_count);
		ts->intr_status = TOUCH_IRQ_KNOCK;
	} else if (data & LPWG_INT_PASSWORD) {
		td4310_tci_getdata(dev, ts->tci.info[TCI_2].tap_count);
		ts->intr_status = TOUCH_IRQ_PASSWD;
	} else if(d->lpwg_fail_reason) {
		TOUCH_I("LPWG Fail Detected\n");

		SYNA_READ(dev, LPWG_FAIL_REASON_DATA_REG, &data, 1, ret, error);

		if (data != 0) {
			fr[0] = data & 0x0F;
			fr[1] = (data >> 4) & 0x0F;
			td4310_lpwg_debug_real_time(dev, fr);
		} else {
			TOUCH_E("LPWG fail reason buffer is %d\n", data);
		}
	} else {
		/* Overtab */
		SYNA_READ(dev, LPWG_OVERTAP_REG, &data, 1, ret, error);

		if (data > ts->tci.info[TCI_2].tap_count) {
			td4310_tci_getdata(dev, ts->tci.info[TCI_2].tap_count + 1);
			ts->intr_status = TOUCH_IRQ_PASSWD;
			TOUCH_I("knock code fail to over tap count = %d\n", data);
		}
	}

error:
	td4310_set_page(dev, DEFAULT_PAGE);
	if (ret < 0)
		TOUCH_E("set DEFAULT_PAGE error\n");

	return ret;
}

static int td4310_tci_knock(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	struct tci_info *info = {0, };
	u8 data[7] = {0, };
	u8 tci_reg = 0;
	int i = 0;
	int ret = 0;

	TOUCH_TRACE();

	if(ts->lpwg.mode == LPWG_PASSWORD) {
		// Make sure (MultiTap Interrupt Delay Time < MultiTap Maximum InterTap Time 2)
		ts->tci.info[TCI_1].intr_delay = ts->tci.double_tap_check ? 68 : 0;
	} else {
		ts->tci.info[TCI_1].intr_delay = 0;
	}

	ret = td4310_set_page(dev, LPWG_PAGE);
	if (ret < 0) {
		TOUCH_E("set LPWG_PAGE error\n");
		return ret;
	}

	for (i = 0 ; i < LPWG_DOUBLE_TAP ; i++) {
		tci_reg = i ? LPWG_TAPCOUNT_REG2 : LPWG_TAPCOUNT_REG;

		if ((ts->tci.mode & (1 << i)) == 0x0) {
			TOUCH_D(LPWG, "%s disable\n", i ? "KNOCK-CODE" : "KNOCK-ON  ");
			data[0] = 0;
			SYNA_WRITE(dev, tci_reg, data, sizeof(u8), ret, error);
		} else {
			info = &ts->tci.info[i];

			TOUCH_D(LPWG, "%s tap_count(%d), "		\
					"intertap_time(%d/%d), "	\
					"touch_slop(%d), "		\
					"tap_distance(%d), "		\
					"intr_delay(%d)\n",
					i ? "KNOCK-CODE" : "KNOCK-ON  ",
					info->tap_count, info->min_intertap, info->max_intertap,
					info->touch_slop, info->tap_distance, info->intr_delay);

			data[0] = ((info->tap_count << 3) | 1);
			data[1] = info->min_intertap;
			data[2] = info->max_intertap;
			data[3] = info->touch_slop;
			data[4] = info->tap_distance;
			data[6] = (info->intr_delay << 1 | 1);
			SYNA_WRITE(dev, tci_reg, data, sizeof(data), ret, error);
		}
	}

error:
	ret = td4310_set_page(dev, DEFAULT_PAGE);
	if (ret < 0)
		TOUCH_E("set DEFAULT_PAGE error\n");

	return ret;
}

static int td4310_lpwg_control(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	bool tci_enable = false;

	TOUCH_TRACE();

	TOUCH_I("%s : lpwg mode = %d\n", __func__, mode);

	switch (mode) {
	case LPWG_NONE:
		ts->tci.mode = 0x00;
		break;

	case LPWG_DOUBLE_TAP:
		ts->tci.mode = 0x01;
		tci_enable = true;
		break;

	case LPWG_PASSWORD_ONLY:
		ts->tci.mode = 0x02;
		tci_enable = true;
		break;

	case LPWG_PASSWORD:
		ts->tci.mode = 0x03;
		tci_enable = true;
		break;

	default:
		TOUCH_E("Unknown lpwg mode\n");
		return 0;
	}

	td4310_tci_knock(dev);
	td4310_tci_report_enable(dev, tci_enable);

	return 0;
}

static int td4310_enter_deep_sleep(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);

	TOUCH_TRACE();

	if (atomic_read(&ts->state.sleep) == IC_NORMAL) {
		if (atomic_read(&d->state.enter_lpwg) == ENTER_LPWG_NEED) {
			td4310_lpwg_fail_reason_enable(dev);
			td4310_lpwg_control(dev, LPWG_DOUBLE_TAP);
			atomic_set(&d->state.enter_lpwg, ENTER_LPWG_DONE);
		}

		td4310_sleep_control(dev, IC_DEEP_SLEEP);
	}

	return 0;
}

int td4310_lpwg_mode(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);

	TOUCH_TRACE();

	if (atomic_read(&d->state.init) == IC_INIT_NEED) {
		TOUCH_I("Not Ready, Need IC init\n");
		return 0;
	}

	if (atomic_read(&ts->state.fb) == FB_SUSPEND) {
		if (ts->mfts_lpwg) {
			td4310_lpwg_control(dev, LPWG_DOUBLE_TAP);
			return 0;
		}

		if (ts->lpwg.screen) {
#if 0 // alternative to real time fail reason
			td4310_lpwg_debug_buffer(dev, TCI_1);
			td4310_lpwg_debug_buffer(dev, TCI_2);
#endif
		} else if (ts->lpwg.sensor == PROX_NEAR) {
			TOUCH_I("FB_SUSPEND : PROX_NEAR\n");
			td4310_enter_deep_sleep(dev);
		} else if (ts->lpwg.qcover == HALL_NEAR) {
			TOUCH_I("FB_SUSPEND : HALL_NEAR\n");
			td4310_enter_deep_sleep(dev);
		} else {
			/* knock on/code */
			if (ts->lpwg.mode == LPWG_NONE) {
				TOUCH_I("FB_SUSPEND : LPWG_NONE\n");
				if (atomic_read(&ts->state.sleep) == IC_NORMAL) {
					td4310_lpwg_control(dev, LPWG_DOUBLE_TAP);
					atomic_set(&d->state.enter_lpwg, ENTER_LPWG_DONE);
					td4310_sleep_control(dev, IC_DEEP_SLEEP);

				}
				return 0;
			}

			if (atomic_read(&ts->state.sleep) == IC_NORMAL) {
				td4310_lpwg_fail_reason_enable(dev);
				td4310_lpwg_control(dev, ts->lpwg.mode);
				atomic_set(&d->state.enter_lpwg, ENTER_LPWG_DONE);
			} else {
				td4310_sleep_control(dev, IC_NORMAL);
				//td4310_lpwg_control(dev, ts->lpwg.mode);
			}
		}
		return 0;
	}

	/* FB_RESUME */
	touch_report_all_event(ts);
	if (ts->lpwg.screen) {
		TOUCH_I("FB_RESUME : normal. screen on\n");
		if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP)
			td4310_sleep_control(dev, IC_NORMAL);
		td4310_lpwg_control(dev, LPWG_NONE);
	} else if (ts->lpwg.sensor == PROX_NEAR) {
		TOUCH_I("FB_RESUME : PROX_NEAR\n");
	} else if (ts->lpwg.qcover == HALL_NEAR) {
		TOUCH_I("FB_RESUME : HALL_NEAR\n");
	} else {
		TOUCH_I("FB_RESUME : resume partial\n");
	}

	return 0;
}

int td4310_lpwg(struct device *dev, u32 code, void *param)
{
	struct touch_core_data *ts = to_touch_core(dev);

	int *value = (int *)param;

	TOUCH_TRACE();

	switch (code) {
	case LPWG_ACTIVE_AREA:
		ts->tci.area.x1 = value[0];
		ts->tci.area.x2 = value[1];
		ts->tci.area.y1 = value[2];
		ts->tci.area.y2 = value[3];
		TOUCH_I("LPWG_ACTIVE_AREA : x0[%d], x1[%d], x2[%d], x3[%d]\n",
			value[0], value[1], value[2], value[3]);
		break;

	case LPWG_TAP_COUNT:
		ts->tci.info[TCI_2].tap_count = value[0];
		break;

	case LPWG_DOUBLE_TAP_CHECK:
		ts->tci.double_tap_check = value[0];
		break;

	case LPWG_UPDATE_ALL:
		ts->lpwg.mode = value[0];
		ts->lpwg.screen = value[1];
		ts->lpwg.sensor = value[2];
		ts->lpwg.qcover = value[3];
		TOUCH_I("LPWG_UPDATE_ALL : mode[%d], screen[%s], sensor[%s], qcover[%s]\n",
			ts->lpwg.mode,
			ts->lpwg.screen ? "ON" : "OFF",
			ts->lpwg.sensor ? "FAR" : "NEAR",
			ts->lpwg.qcover ? "CLOSE" : "OPEN");
		td4310_lpwg_mode(dev);
		break;
	}

	return 0;
}

static ssize_t show_lpwg_fail_reason(struct device *dev, char *buf)
{
	struct td4310_data *d = to_td4310_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret += sprintf(buf+ret, "LPWG_FAIL_REASON : [%s]\n",
			d->lpwg_fail_reason? "ENABLE" : "DISABLE");

	return ret;
}

static ssize_t store_lpwg_fail_reason(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int value = 0;

	TOUCH_TRACE();

	if (ts->lpwg.screen == 0) {
		TOUCH_I("LCD is off. Try after LCD On \n");
		return count;
	}

	if (sscanf(buf, "%d", &value) <= 0)
		return -EINVAL;

	d->lpwg_fail_reason = value;

	return count;
}

static TOUCH_ATTR(lpwg_fail_reason, show_lpwg_fail_reason, store_lpwg_fail_reason);

static struct attribute *td4310_lpwg_attribute_list[] = {
	&touch_attr_lpwg_fail_reason.attr,
	NULL,
};

static const struct attribute_group td4310_lpwg_attribute_group = {
	.attrs = td4310_lpwg_attribute_list,
};

int td4310_lpwg_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &td4310_lpwg_attribute_group);
	if (ret < 0)
		TOUCH_E("failed to create lpwg sysfs\n");

	return 0;
}

MODULE_AUTHOR("BSP-TOUCH@lge.com");
MODULE_DESCRIPTION("LGE touch driver v3");
MODULE_LICENSE("GPL");
