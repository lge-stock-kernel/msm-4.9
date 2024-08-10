/* touch_td4310.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: hoyeon.jang@lge.com
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

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
#include <soc/qcom/lge/board_lge.h>
#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY) || IS_ENABLED(CONFIG_LGE_TOUCH_PANEL_GLOBAL_RESET)
#include <linux/msm_lcd_recovery.h>
#endif
#endif

#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
#include <soc/mediatek/lge/board_lge.h>
#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY) || IS_ENABLED(CONFIG_LGE_TOUCH_PANEL_GLOBAL_RESET)
extern void mtkfb_esd_recovery(void);
#endif
#endif

#define MAX_NUM_OF_REPORT_OBJECT 6
const char *report_object[MAX_NUM_OF_REPORT_OBJECT] = {
	"finger",
	"stylus pen",
	"palm",
	"unclassified object",
	"hovering finger",
	"gloved finger",
};

#define MAX_NUM_OF_DEV_STATUS 16
static const char *device_status_info_str[MAX_NUM_OF_DEV_STATUS + 1] = {
	[1] = "[0x01] Device Reset Occured",
	[2] = "[0x02] Invalid Configuration",
	[3] = "[0x03] Device Failure",
	[4] = "[0x04] Configuration CRC Fail",
	[5] = "[0x05] FW CRC Fail",
	[6] = "[0x06] CRC In Progress",
	[7] = "[0x07] Guest Code CRC Fail",
	[8] = "[0x08] External DDIC AFE Fail",
	[9] = "[0x09] Display Device Fail",
	[10] = "Unknown status",
	[11] = "Unknown status",
	[12] = "Unknown status",
	[13] = "Unknown status",
	[14] = "Unknown status",
	[15] = "Unknown status",
	[16] = "Unknown status",
};

static void project_param_set(struct device *dev)
{
#if defined(CONFIG_LGE_TOUCH_SYNAPTICS_TD4310)
	struct td4310_data *d = to_td4310_data(dev);

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	struct touch_core_data *ts = to_touch_core(dev);
	int board_rev = 0;

	board_rev = lge_get_board_rev_no();

	if (board_rev < HW_REV_B) {
		ts->def_fwpath[0] = ts->def_fwpath[2];
		ts->def_fwpath[1] = ts->def_fwpath[3];
		TOUCH_I("board_rev = %d. change FW path\n", board_rev);
		TOUCH_I("def_fwpath[0] : [%s]\n", ts->def_fwpath[0]);
		TOUCH_I("def_fwpath[1] : [%s]\n", ts->def_fwpath[1]);
	}
#endif

	d->ic_info.fw_pid_addr = 0x10;
	d->ic_info.fw_ver_addr = 0x1d100;

	d->lpwg_fail_reason = 1;
	d->uBL_addr = 0x2c;

#endif /* CONFIG_LGE_TOUCH_SYNAPTICS_TD4310 */
	return;
}

static struct td4310_rmidev_exp_fhandler rmidev_fhandler;

void td4310_rmidev_function(struct td4310_rmidev_exp_fn *exp_fn, bool insert)
{
	TOUCH_TRACE();

	rmidev_fhandler.insert = insert;

	if (insert) {
		rmidev_fhandler.exp_fn = exp_fn;
		rmidev_fhandler.insert = true;
		rmidev_fhandler.remove = false;
	} else {
		rmidev_fhandler.exp_fn = NULL;
		rmidev_fhandler.insert = false;
		rmidev_fhandler.remove = true;
	}

	return;
}

static int td4310_rmidev_init(struct device *dev)
{
	int ret = 0;

	TOUCH_TRACE();

	if (rmidev_fhandler.insert) { //TODO DEBUG_OPTION_2
		ret = rmidev_fhandler.exp_fn->init(dev);
		if (ret < 0)
			TOUCH_E("Failed to init rmi_dev settings : %d\n", ret);
		else
			rmidev_fhandler.initialized = true;
	}

	return 0;
}

int td4310_force_update(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);
	u8 data = 0;
	int retry = 0;
	int ret = 0;

	TOUCH_TRACE();

	ret = td4310_set_page(dev, ANALOG_PAGE);
	if (ret < 0) {
		TOUCH_E("set ANALOG_PAGE error\n");
		return ret;
	}

	data = FORCE_UPDATE_BIT;
	SYNA_WRITE(dev, ANALOG_COMMAND_REG, &data, 1, ret, error);

	do {
		/* Waiting for update complete */
		touch_msleep(5);
		SYNA_READ(dev, ANALOG_COMMAND_REG, &data, 1, ret, error);

		if ((data & FORCE_UPDATE_BIT) == FORCE_UPDATE_CLEAR_BIT) {
			TOUCH_I("Force update done.\n");
			break;
		}
	} while ((retry++) < 40);

	if (retry >= 40) {
		TOUCH_E("force update time out error : try(%d, %d ms)\n", retry + 1, (retry + 1) * 5);
		ret = -EPERM;
	} else {
		TOUCH_I("force update complete : try(%d, %d ms)\n", retry + 1, (retry + 1) * 5);
	}

error:
	ret |= td4310_set_page(dev, DEFAULT_PAGE);
	if (ret < 0)
		TOUCH_E("set DEFAULT_PAGE error\n");

	return ret;
}

int td4310_read(struct device *dev, u8 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct touch_bus_msg msg = {0,};
	int ret = 0;

	ts->tx_buf[0] = addr;
	msg.tx_buf = ts->tx_buf;
	msg.tx_size = 1;
	msg.rx_buf = ts->rx_buf;
	msg.rx_size = size;

	ret = touch_bus_read(dev, &msg);
	if (ret < 0) {
		TOUCH_E("touch_bus_read error : %d\n", ret);
		return ret;
	}

	memcpy(data, &ts->rx_buf[0], size);
	return 0;
}

int td4310_write(struct device *dev, u8 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct touch_bus_msg msg = {0,};
	int ret = 0;

	ts->tx_buf[0] = addr;
	memcpy(&ts->tx_buf[1], data, size);
	msg.tx_buf = ts->tx_buf;
	msg.tx_size = size + 1;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	ret = touch_bus_write(dev, &msg);
	if (ret < 0) {
		TOUCH_E("touch_bus_write error : %d\n", ret);
		return ret;
	}

	return 0;
}

int td4310_set_page(struct device *dev, u8 page)
{
	return td4310_write(dev, PAGE_SELECT_REG, &page, 1);
}

static int td4310_get_f12_reg(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);
	u8 obj_info[2] = {0, };
	u8 max_xy[4] = {0, };
	u8 offset = 0;
	u8 ctrl_p_size = 0;
	u8 data_p_size = 0;
	u8 *ctrl_p_data = NULL;
	u8 *data_p_data = NULL;
	int i = 0;
	int ret = 0;

	TOUCH_TRACE();

	/* [start] Dynamic alloc F12 Control Register set */
	SYNA_READ(dev, F12_SIZE_OF_CTRL_PRESENCE_REG,
			&ctrl_p_size, sizeof(ctrl_p_size), ret, error);

	TOUCH_I("     = f12 info =\n");
	TOUCH_I("     f12_ctrl_p_size = %d\n", ctrl_p_size);

	if (ctrl_p_size != 0) {
		ctrl_p_data = kzalloc(ctrl_p_size, GFP_KERNEL);
		if (!ctrl_p_data) {
			TOUCH_E("failed to allocate ctrl_p_data\n");
			goto alloc_error;
		}

		if (d->f12_reg.ctrl != NULL)
			devm_kfree(dev, d->f12_reg.ctrl);

		d->f12_reg.ctrl = devm_kzalloc(dev, ctrl_p_size * 8, GFP_KERNEL);
		if (!d->f12_reg.ctrl) {
			TOUCH_E("failed to allocate d->f12_reg.ctrl\n");
			goto alloc_error;
		}

		SYNA_READ(dev, F12_CTRL_PRESENCE_REG,
				ctrl_p_data, ctrl_p_size, ret, error);

		for (i = 0 ; i < ctrl_p_size ; i++)
			TOUCH_I("     f12_ctrl_p_data[%d] = 0x%02x\n", i + 1, ctrl_p_data[i]);

		for (i = 0, offset = 0 ; i < ctrl_p_size * 8 ; i++) {
			/* (i/8) -> array element change, (i%8) -> bit pattern change */
			if (ctrl_p_data[(i / 8) + 1] & (1 << (i % 8))) {
				d->f12_reg.ctrl[i] = d->f12.dsc.control_base + offset;
				TOUCH_I("     f12_reg.ctrl[%d] = 0x%02X (0x%02x+%d)\n",
						i, d->f12_reg.ctrl[i],
						d->f12.dsc.control_base, offset);
				offset++;
			}
		}
	}
	/* [end] Dynamic alloc F12 Control Register set */

	/* [start] Dynamic alloc F12 data Register set */
	SYNA_READ(dev, F12_SIZE_OF_DATA_PRESENCE_REG,
			&data_p_size, sizeof(data_p_size), ret, error);
	TOUCH_I("     f12_data_p_size = %d\n", data_p_size);

	if (data_p_size != 0) {
		data_p_data = kzalloc(data_p_size, GFP_KERNEL);
		if (!data_p_data) {
			TOUCH_E("failed to allocate data_p_data\n");
			goto alloc_error;
		}

		if (d->f12_reg.data != NULL)
			devm_kfree(dev, d->f12_reg.data);

		d->f12_reg.data = devm_kzalloc(dev, data_p_size * 8, GFP_KERNEL);
		if (!d->f12_reg.data) {
			TOUCH_E("failed to allocate d->f12_reg.data\n");
			goto alloc_error;
		}

		SYNA_READ(dev, F12_DATA_PRESENCE_REG,
				data_p_data, data_p_size, ret, error);

		for (i = 0 ; i < data_p_size ; i++)
			TOUCH_I("     f12_data_p_data[%d] = 0x%02x\n", i + 1, data_p_data[i]);

		for (i = 0, offset = 0 ; i < data_p_size * 8 ; i++) {
			if (data_p_data[(i / 8) + 1] & (1 << (i % 8))) {
				d->f12_reg.data[i] = d->f12.dsc.data_base + offset;
				TOUCH_I("     f12_reg.data[%d]=0x%02X (0x%02x+%d)\n",
						i, d->f12_reg.data[i],
						d->f12.dsc.data_base, offset);
				offset++;
			}
		}
	}
	/* [end] Dynamic alloc F12 data Register set */

	SYNA_READ(dev, F12_OBJECT_REPORT_ENABLE_REG, obj_info, sizeof(obj_info), ret, error);
	TOUCH_I("     max num of fingers = [%d]\n", obj_info[1]);
	TOUCH_I("     reporting object bits = [0x%02X]\n", obj_info[0]);

	for (i = 0 ; i < MAX_NUM_OF_REPORT_OBJECT ; i++) {
		if ((obj_info[0]) & (1U << i)) {
			if (report_object[i] != NULL)
				TOUCH_I("     - report %s\n", report_object[i]);
		}
	}

	SYNA_READ(dev, F12_MAX_XY_COORDINATE_REG, max_xy, sizeof(max_xy), ret, error);
	TOUCH_I("     max_x = [%d], max_y = [%d]\n",
			((u16)max_xy[0] << 0) | ((u16)max_xy[1] << 8),
			((u16)max_xy[2] << 0) | ((u16)max_xy[3] << 8));

	goto out;

alloc_error:
	ret = -ENOMEM;
	if (d->f12_reg.ctrl)
		devm_kfree(dev, d->f12_reg.ctrl);
	if (d->f12_reg.data)
		devm_kfree(dev, d->f12_reg.data);
out:
error:
	if (ctrl_p_data)
		kfree(ctrl_p_data);
	if (data_p_data)
		kfree(data_p_data);
	return ret;
}

static int td4310_page_description(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	struct function_descriptor dsc = {0,};
	struct i2c_client *client = to_i2c_client(dev);
	unsigned short pdt = 0;
	u8 page = 0;
	u8 bit_count = 0;
	u8 total_func = 0;
	u32 backup_slave_addr = client->addr;
	int ret = 0;

	TOUCH_TRACE();

	atomic_set(&d->state.fw_recovery, false);
	memset(&d->f01, 0, sizeof(struct td4310_function));
	memset(&d->f12, 0, sizeof(struct td4310_function));
	memset(&d->f34, 0, sizeof(struct td4310_function));
	memset(&d->f51, 0, sizeof(struct td4310_function));
	memset(&d->f54, 0, sizeof(struct td4310_function));
	memset(&d->f55, 0, sizeof(struct td4310_function));
	memset(&d->fdc, 0, sizeof(struct td4310_function));
	d->td4310_function_bits = 0;

	TOUCH_I("==== [start] PDT scan ====\n");

	for (page = 0 ; page < PAGES_TO_SERVICE ; page++) {
		ret = td4310_set_page(dev, page);
		if (ret < 0) {
			TOUCH_E("set %d page error (ret: %d)\n", page, ret);
			TOUCH_E("need FW recovery - change uBL slave addr [0x%x -> 0x%x]\n",
					client->addr, d->uBL_addr);
			client->addr = d->uBL_addr;
			td4310_set_page(dev, page);
		}

		for (pdt = PDT_START_ADDR ; pdt >= PDT_END_ADDR ; pdt -= sizeof(dsc)) {
			ret = td4310_read(dev, pdt, &dsc, sizeof(dsc));

			if (!dsc.fn_number)
				break;

			TOUCH_I("    PDT [F%02x] - query[%02x], cmd[%02x], ctrl[%02x], " \
					"data[%02x], isc[%02x]\n",
					dsc.fn_number, dsc.query_base, dsc.command_base,
					dsc.control_base, dsc.data_base, dsc.int_source_count);

			switch (dsc.fn_number) {
			case 0x01:
				d->f01.dsc = dsc;
				d->f01.page = page;
				d->td4310_function_bits |= TD4310_FUNC_01;
				break;
			case 0x12:
				d->f12.dsc = dsc;
				d->f12.page = page;
				d->td4310_function_bits |= TD4310_FUNC_12;
				ret = td4310_get_f12_reg(dev);
				if (ret < 0) {
					TOUCH_E("td4310_get_f12_reg error : %d\n", ret);
					TOUCH_I("==== [end] PDT scan fail ====\n");
					return ret;
				}
				break;
			case 0x34:
				d->f34.dsc = dsc;
				d->f34.page = page;
				d->td4310_function_bits |= TD4310_FUNC_34;
				break;
			case 0x35:
				d->f35.dsc = dsc;
				d->f35.page = page;
				d->td4310_function_bits |= TD4310_FUNC_35;
				break;
			case 0x51:
				d->f51.dsc = dsc;
				d->f51.page = page;
				d->td4310_function_bits |= TD4310_FUNC_51;
				break;
			case 0x54:
				d->f54.dsc = dsc;
				d->f54.page = page;
				d->td4310_function_bits |= TD4310_FUNC_54;
				break;
			case 0x55:
				d->f55.dsc = dsc;
				d->f55.page = page;
				d->td4310_function_bits |= TD4310_FUNC_55;
				break;
			case 0xdc:
				d->fdc.dsc = dsc;
				d->fdc.page = page;
				d->td4310_function_bits |= TD4310_FUNC_DC;
				break;
			default:
				TOUCH_E("Unknown Page : 0x%02x\n", dsc.fn_number);
				break;
			}
		}
	}

	TOUCH_I("==== [end] PDT scan ====\n");

	for (bit_count = 0 ; bit_count <= 7 ; bit_count++) {
		if(d->td4310_function_bits & (1 << bit_count))
			total_func++;
	}
	TOUCH_I("exist function number = %d, function_bits = 0x%x\n",
			total_func, d->td4310_function_bits);

	TOUCH_I("common[%dP:0x%02x]\n", d->f01.page, d->f01.dsc.fn_number);
	TOUCH_I("finger[%dP:0x%02x]\n", d->f12.page, d->f12.dsc.fn_number);
	TOUCH_I("flash [%dP:0x%02x]\n", d->f34.page, d->f34.dsc.fn_number);
	TOUCH_I("uBL   [%dP:0x%02x]\n", d->f35.page, d->f35.dsc.fn_number);
	TOUCH_I("analog[%dP:0x%02x]\n", d->f54.page, d->f54.dsc.fn_number);
	TOUCH_I("lpwg  [%dP:0x%02x]\n", d->f51.page, d->f51.dsc.fn_number);

	if (client->addr == d->uBL_addr) {
		TOUCH_I("restore address [0x%x -> 0x%x]\n", client->addr, backup_slave_addr);
		client->addr = backup_slave_addr;
	}

	if (d->td4310_function_bits == 0x00) {
		TOUCH_E("function not found - force FW upgrade\n");
		ts->force_fwup = 1;
		ret = -1;
	} else if (d->td4310_function_bits & TD4310_FUNC_35) {
		TOUCH_E("uBL mode(F35 detected) - Need FW recovery\n");
		atomic_set(&d->state.fw_recovery, true);
		return -1;
	} else if (d->td4310_function_bits == (TD4310_FUNC_01 | TD4310_FUNC_34)) {
		TOUCH_E("BL mode(f01,f34 only) - force FW upgrade\n");
		ts->force_fwup = 1;
		ret = -1;
	}

	ret |= td4310_set_page(dev, DEFAULT_PAGE);
	if (ret)
		TOUCH_E("set DEFAULT_PAGE error\n");

	return ret;
}

static int td4310_ic_info(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	if (atomic_read(&d->state.scan_pdt) == true) {
		ret = td4310_page_description(dev);
		if (ret < 0) {
			TOUCH_E("page description error\n");
			return ret;
		}
		atomic_set(&d->state.scan_pdt, false);
	}

	SYNA_READ(dev, IC_FW_VERSION_REG, d->ic_info.raws, sizeof(d->ic_info.raws), ret, error);
	SYNA_READ(dev, CUSTOMER_FAMILY_REG, &(d->ic_info.family), sizeof(d->ic_info.family), ret, error);
	SYNA_READ(dev, FW_REVISION_REG, &(d->ic_info.revision), sizeof(d->ic_info.revision), ret, error);
	SYNA_READ(dev, PRODUCT_ID_REG, d->ic_info.product_id, sizeof(d->ic_info.product_id), ret, error);

	d->ic_info.version.major = (d->ic_info.raws[3] & 0x80 ? 1 : 0);
	d->ic_info.version.minor = (d->ic_info.raws[3] & 0x7F);

	TOUCH_I("======= IC info =======\n");
	TOUCH_I(" IC_Version = v%d.%02d\n", d->ic_info.version.major, d->ic_info.version.minor);
	TOUCH_I(" Customer Family = %d\n", d->ic_info.family);
	TOUCH_I(" F/W Revision = %d\n", d->ic_info.revision);
	TOUCH_I(" Product ID = %s\n", d->ic_info.product_id);
	TOUCH_I("=======================\n");

	return 0;
error:
	TOUCH_E("read ic info error\n");

	atomic_set(&d->state.scan_pdt, true);
	ret = td4310_page_description(dev);
	if (ret < 0)
		TOUCH_E("page description error\n");

	atomic_set(&d->state.scan_pdt, false);

	return ret;
}

static int td4310_get_status(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);
	u8 data[2] = {0, };
	u8 retry = 0;
	int ret = 0;

	TOUCH_TRACE();

	memset(&(d->info), 0, sizeof(struct td4310_touch_info));

	do {
		ret = td4310_read(dev, DEVICE_STATUS_REG, &data, sizeof(data));
		if (ret < 0) {
			TOUCH_E("read DEVICE_STATUS_REG error : retry(%d)\n", retry);
			touch_msleep(30);
		} else {
			break;
		}
	} while ((retry++) < 10);

	if (retry >= 10) {
		if(atomic_read(&d->state.esd_recovery) == ESD_RECOVERY_DONE) {
			if(d->skip_global_reset) {
				TOUCH_I("%s : skip global reset\n", __func__);
			} else {
				TOUCH_I("Retry failed. Need Global Reset\n");
				atomic_set(&d->state.esd_recovery, ESD_RECOVERY_NEED);
				atomic_set(&d->state.scan_pdt, true);
				atomic_set(&d->state.init, IC_INIT_NEED);
				return -EGLOBALRESET;
			}
		}
	}

	d->info.device_status = data[0];
	d->info.irq_status = data[1];

	TOUCH_D(TRACE, "%s : status[device:0x%02x, interrupt:0x%02x]\n",
			__func__, d->info.device_status, d->info.irq_status);

	return ret;
}

static int td4310_check_status(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u8 status = (d->info.device_status & DEV_STATUS_MASK);
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_D(ABS, "%s : status[device:0x%02x, interrupt:0x%02x]\n",
			__func__, d->info.device_status, d->info.irq_status);

	if((!d->info.device_status) == DEV_STS_NORMAL_MASK) {
		if ((d->info.device_status) == GR_ESD_AFTER_CONFIGURED
				|| (d->info.device_status) == GR_FAIL_TO_FW_LOADING
				|| (d->info.device_status) == GR_ABNORMAL_RESET
				|| (d->info.device_status) == GR_ESD_UNCONFIGURED){
			if (atomic_read(&d->state.esd_recovery) == ESD_RECOVERY_DONE) {
				if(d->skip_global_reset) {
					TOUCH_I("%s : skip global reset, [0x%02x]\n", __func__, d->info.device_status);
				} else {
					TOUCH_I("Need Global Reset. [0x%02x]\n", d->info.device_status);
					atomic_set(&d->state.scan_pdt, true);
					atomic_set(&d->state.init, IC_INIT_NEED);
					atomic_set(&d->state.esd_recovery, ESD_RECOVERY_NEED);
					ret = -EGLOBALRESET;
				}
			} else {
				TOUCH_I("Global Reset in progress...[0x%02x]\n", d->info.device_status);
			}
		} else if (d->info.device_status & DEV_DEVICE_UNCONF_MASK) {
			if(atomic_read(&d->state.config) == IC_CONFIGURED_DONE) {
				TOUCH_I("Need SW reset. Unconfigured. [0x%02x]\n", d->info.device_status);
				atomic_set(&d->state.config, IC_CONFIGURED_NEED);
				ret = -ESWRESET;
			} else {
				TOUCH_I("IC Configuration in progress...[0x%02x]\n", d->info.device_status);
			}
		} else if (d->info.device_status & DEV_FLASH_PROGRESS_MASK) {
			if(atomic_read(&ts->state.core) == CORE_NORMAL) {
				TOUCH_I("Need FW Upgrade. [0x%02x]\n", d->info.device_status);
				atomic_set(&d->state.scan_pdt, true);
				atomic_set(&d->state.init, IC_INIT_NEED);
				ret = -EUPGRADE;
			} else {
				TOUCH_I("FW Upgrade in progress...[0x%02x]\n", d->info.device_status);
			}
		}
	}

	if (ret < 0) {
		if (status <= MAX_NUM_OF_DEV_STATUS)
			TOUCH_I("[DEVICE_STATUS_INFO] %s, dev_status = 0x%2x\n",
					device_status_info_str[status], d->info.device_status);
	}

	return ret;
}

static int td4310_get_finger_count(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u8 data[2] = {0, };
	u16 touch_attn = 0;
	int count = 0;

	TOUCH_TRACE();

	if (d->f12_reg.data != NULL) {
		SYNA_READ(dev, ABS_ATTN_REG, data, sizeof(data), count, error);

		touch_attn = (((u16)data[1] << 8) | ((u16)data[0]));

		for (count = ts->caps.max_id ; count > 0 ; count--) {
			if (touch_attn & (1U << (count - 1)))
				break;
		}

		TOUCH_D(ABS, "touch count : %d\n", count);
	} else {
		TOUCH_E("f12_reg.data is not allocated\n");
		atomic_set(&d->state.scan_pdt, true);
		return -ENOMEM;
	}
error:
	return count;
}

static int td4310_noise_log(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u8 data[2] = {0, };
	u8 cns = 0;
	u8 buf_lsb = 0;
	u8 buf_msb = 0;
	u16 im = 0;
	u16 cid_im = 0;
	u16 freq_scan_im = 0;
	int ret = 0;

	TOUCH_TRACE();

	ret = td4310_set_page(dev, ANALOG_PAGE);
	if (ret < 0) {
		TOUCH_E("set ANALOG_PAGE error\n");
		return ret;
	}

	SYNA_READ(dev, CURRENT_NOISE_STATUS_REG, &cns, sizeof(cns), ret, error);
	SYNA_READ(dev, INTERFERENCE_METRIC_LSB_REG, &buf_lsb, sizeof(buf_lsb), ret, error);
	SYNA_READ(dev, INTERFERENCE_METRIC_MSB_REG, &buf_msb, sizeof(buf_msb), ret, error);
	im = (buf_msb << 8) | buf_lsb;
	SYNA_READ(dev, CID_IM_REG, data, sizeof(data), ret, error);
	cid_im = (data[1] << 8) | data[0];
	SYNA_READ(dev, FREQ_SCAN_IM_REG, data, sizeof(data), ret, error);
	freq_scan_im = (data[1] << 8) | data[0];

	if (ts->new_mask != 0) {
		if (ts->old_mask == 0)
			TOUCH_I("Curr : CNS[%5d] IM[%5d] CID_IM[%5d] FREQ_SCAN_IM[%5d]\n",
					cns, im, cid_im, freq_scan_im);

		d->noise.cnt++;

		d->noise.cns_sum += cns;
		d->noise.im_sum += im;
		d->noise.cid_im_sum += cid_im;
		d->noise.freq_scan_im_sum += freq_scan_im;
	}

	if (ts->new_mask == 0 || d->noise.cnt >= 500) {
		TOUCH_I("Aver : CNS[%5lu] IM[%5lu] CID_IM[%5lu] FREQ_SCAN_IM[%5lu] (cnt:%lu)\n",
				d->noise.cns_sum / d->noise.cnt,
				d->noise.im_sum / d->noise.cnt,
				d->noise.cid_im_sum / d->noise.cnt,
				d->noise.freq_scan_im_sum / d->noise.cnt,
				d->noise.cnt);

		d->noise.cnt = 0;

		d->noise.im_sum = 0;
		d->noise.cns_sum = 0;
		d->noise.cid_im_sum = 0;
		d->noise.freq_scan_im_sum = 0;
	}

error:
	ret = td4310_set_page(dev, DEFAULT_PAGE);
	if (ret < 0)
		TOUCH_E("set DEFAULT_PAGE error\n");
	return ret;
}

static int td4310_irq_abs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	struct touch_data *tdata = {0, };
	u8 finger_index = 0;
	int i = 0;
	int ret = 0;

	TOUCH_TRACE();

	d->info.touch_cnt = td4310_get_finger_count(dev);
	if (d->info.touch_cnt < 0) {
		TOUCH_E("get finger count error\n");
		return ret;
	}

	ts->new_mask = 0;

	if (d->info.touch_cnt == 0) {
		if (d->is_palm) {
			TOUCH_I("Palm Released\n");
			d->is_palm = false;
		}
	goto out;
	}

	SYNA_READ(dev, ABS_DATA_REG, d->info.data,
			sizeof(*(d->info.data)) * d->info.touch_cnt, ret, error);

	for (i = 0 ; i < d->info.touch_cnt ; i++) {
		if (d->info.data[i].type == ABS_NO_OBJECT)
			continue;

		if (d->info.data[i].type > ABS_MAX_OBJECT)
			TOUCH_D(ABS, "id : %d, type : %d\n", i, d->info.data[i].type);

		if (d->info.data[i].type == ABS_PALM && !d->is_palm) {
			TOUCH_I("Palm Detected\n");
			ts->is_cancel = 1;
			d->is_palm = true;

			ts->tcount = 0;
			ts->intr_status = TOUCH_IRQ_FINGER;
			return ret;
		}

		if (d->info.data[i].type == ABS_FINGER) {
			ts->new_mask |= (1 << i);

			tdata = ts->tdata + i;
			tdata->id = i;
			tdata->type = d->info.data[i].type;
			tdata->x = d->info.data[i].x_lsb | d->info.data[i].x_msb << 8;
			tdata->y = d->info.data[i].y_lsb | d->info.data[i].y_msb << 8;
			tdata->pressure = d->info.data[i].z;

			if (d->info.data[i].wx > d->info.data[i].wy) {
				tdata->width_major = d->info.data[i].wx;
				tdata->width_minor = d->info.data[i].wy;
				tdata->orientation = 0;
			} else {
				tdata->width_major = d->info.data[i].wy;
				tdata->width_minor = d->info.data[i].wx;
				tdata->orientation = 1;
			}

			finger_index++;

			TOUCH_D(ABS, "tdata [id:%d t:%d - x:%4d y:%4d z:%3d - M:%d m:%d o:%d]\n",
					tdata->id,
					tdata->type,
					tdata->x,
					tdata->y,
					tdata->pressure,
					tdata->width_major,
					tdata->width_minor,
					tdata->orientation);
		}
	}

	if (d->noise.noise_log == NOISE_ENABLE)
		td4310_noise_log(dev);
out:
error:
	ts->tcount = finger_index;
	ts->intr_status = TOUCH_IRQ_FINGER;

	return ret;
}

static int td4310_irq_handler(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	pm_qos_update_request(&d->pm_qos_req, 10);

	ret = td4310_get_status(dev);
	if (ret < 0) {
		TOUCH_E("td4310_get_status error : %d\n", ret);
		goto error;
	}

	ret = td4310_check_status(dev);
	if (ret == 0) {
		if (d->info.irq_status & INT_ABS_MASK)
			ret = td4310_irq_abs(dev);
		else if (d->info.irq_status & INT_LPWG_MASK)
			ret = td4310_irq_lpwg(dev);
	} else {
		TOUCH_E("td4310_check_status error : %d\n", ret);
	}

error:
	pm_qos_update_request(&d->pm_qos_req, PM_QOS_DEFAULT_VALUE);

	return ret;
}

void td4310_reset_ctrl(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u8 data = 0;
	int ret = 0;

	TOUCH_TRACE();

	switch (mode) {
	case POWER_SW_RESET:
		TOUCH_I("%s : SW Reset\n", __func__);

		data = DEV_SW_RESET_BIT;
		SYNA_WRITE(dev, DEV_SW_RESET_REG, &data, sizeof(data), ret, error);
		touch_msleep(ts->caps.sw_reset_delay);

		break;

	case POWER_HW_RESET_ASYNC:
	case POWER_HW_RESET_SYNC:
		TOUCH_I("%s : skip HW Reset - shared reset pin. do not control\n", __func__);
#if 0
		TOUCH_I("%s : HW Reset (mode:%d)\n", __func__, mode);
		touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_msleep(1);
		touch_gpio_direction_output(ts->reset_pin, 1);

		atomic_set(&d->init, IC_INIT_NEED);

		if (mode == POWER_HW_RESET_ASYNC){
			mod_delayed_work(ts->wq, &ts->init_work,
					msecs_to_jiffies(ts->caps.hw_reset_delay));
		} else if (mode == POWER_HW_RESET_SYNC) {
			touch_msleep(ts->caps.hw_reset_delay);
			td4310_init(dev);
			touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
		}
#endif
		break;

	default:
		TOUCH_E("not supported reset control\n");
		break;
	}
error:
	return;
}

static int td4310_power(struct device *dev, int ctrl)
{
	TOUCH_TRACE();

	switch (ctrl) {
	case POWER_OFF:
		TOUCH_I("%s : POWER_OFF - shared reset pin. do not control\n", __func__);
#if 0
		TOUCH_I("%s : POWER_OFF\n", __func__);
		atomic_set(&d->init, IC_INIT_NEED);
		touch_gpio_direction_output(ts->reset_pin, 0);
		touch_power_3_3_vcl(dev, 0);
		touch_power_1_8_vdd(dev, 0);
		touch_msleep(1);
#endif
		break;

	case POWER_ON:
		TOUCH_I("%s : POWER_ON - shared reset pin. do not control\n", __func__);
#if 0
		TOUCH_I("%s : POWER_ON\n", __func__);
		touch_power_1_8_vdd(dev, 1);
		touch_power_3_3_vcl(dev, 1);
		touch_gpio_direction_output(ts->reset_pin, 1);
		touch_msleep(ts->caps.hw_reset_delay);
#endif
		break;

	case POWER_SW_RESET:
		TOUCH_I("%s : POWER_SW_RESET\n", __func__);
		td4310_reset_ctrl(dev, POWER_SW_RESET);
		break;

	case POWER_HW_RESET_ASYNC:
		TOUCH_I("%s : POWER_HW_RESET_ASYNC\n", __func__);
		td4310_reset_ctrl(dev, POWER_HW_RESET_ASYNC);
		break;

	case POWER_HW_RESET_SYNC:
		TOUCH_I("%s : POWER_HW_RESET_SYNC\n", __func__);
		td4310_reset_ctrl(dev, POWER_HW_RESET_SYNC);
		break;

	default:
		TOUCH_I("%s : not supported Power ctrl\n", __func__);
		break;
	}

	return 0;
}

int td4310_sleep_control(struct device *dev, u8 mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u8 data = 0;
	int ret = 0;

	TOUCH_TRACE();

	SYNA_READ(dev, DEVICE_CONTROL_REG, &data, sizeof(data), ret, error);

	data &= DEV_SLEEP_CTRL_CLEAR_MASK;
	if (mode) {
		data |= DEV_CTRL_SLEEP;
		atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
	} else {
		data |= DEV_CTRL_NORMAL_OP;
		atomic_set(&ts->state.sleep, IC_NORMAL);
	}

	SYNA_WRITE(dev, DEVICE_CONTROL_REG, &data, sizeof(data), ret, error);

	TOUCH_I("%s : %s\n", __func__, mode ? "IC_DEEP_SLEEP" : "IC_NORMAL");
error:
	return ret;
}

static void td4310_connect(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int charger_state = atomic_read(&ts->state.connect);

	TOUCH_TRACE();

	switch (charger_state) {
	case CONNECT_INVALID:
		atomic_set(&d->state.charger, CONNECT_NONE);
		break;
	case CONNECT_DCP:
	case CONNECT_PROPRIETARY:
		atomic_set(&d->state.charger, CONNECT_TA);
		break;
	case CONNECT_HUB:
		atomic_set(&d->state.charger, CONNECT_OTG);
		break;
	default:
		atomic_set(&d->state.charger, CONNECT_USB);
		break;
	}
#if 0
	if (atomic_read(&ts->state.debug_option_mask) & DEBUG_OPTION_4) {
		TOUCH_I("TA Simulator mode, Set CONNECT_TA\n");
		atomic_set(&d->state.charger, CONNECT_TA);
	}
#endif
	return;
}

static int td4310_debug_option(struct device *dev, u32 *data)
{
	u32 option_mask = data[0];
	u32 enable = data[1];

	TOUCH_TRACE();

	switch (option_mask) {
	case DEBUG_OPTION_4:
		TOUCH_I("TA Simulator mode %s. - skip\n", enable ? "Enable" : "Disable");
		td4310_connect(dev);
		break;
	default:
		TOUCH_E("Not supported debug option\n");
		break;
	}

	return 0;
}

static void td4310_fb_notify_work_func(struct work_struct *fb_notify_work)
{
	struct td4310_data *d = container_of(to_delayed_work(fb_notify_work),
			struct td4310_data, fb_notify_work);
	int ret = 0;

	if (d->lcd_mode == LCD_MODE_U0 || d->lcd_mode == LCD_MODE_U2)
		ret = FB_SUSPEND;
	else
		ret = FB_RESUME;

	touch_notifier_call_chain(NOTIFY_FB, &ret);
}

static int td4310_notify_etc(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	switch (event) {
		case LCD_EVENT_TOUCH_RESET_START:
			TOUCH_I("LCD_EVENT_TOUCH_RESET_START\n");
			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			break;

		case LCD_EVENT_TOUCH_RESET_END:
			TOUCH_I("LCD_EVENT_TOUCH_RESET_END\n");
			break;

		default:
			TOUCH_I("%s : not supported event = 0x%x\n", __func__, (unsigned int)event);
			break;
	}

	return 0;
}

static void td4310_lcd_mode(struct device *dev, u32 mode)
{
	struct td4310_data *d = to_td4310_data(dev);

	TOUCH_I("lcd_mode: %d (prev: %d)\n", mode, d->lcd_mode);

	if (mode == LCD_MODE_U2_UNBLANK)
		mode = LCD_MODE_U2;

	d->lcd_mode = mode;
}

static int td4310_notify_normal(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	switch (event) {
	case NOTIFY_TOUCH_RESET:
		ret = NOTIFY_OK;
		TOUCH_I("NOTIFY_TOUCH_RESET\n");
		break;
	case LCD_EVENT_TOUCH_RESET_START:
		TOUCH_I("LCD_EVENT_TOUCH_RESET_START\n");
		touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
		break;
	case LCD_EVENT_TOUCH_RESET_END:
		TOUCH_I("LCD_EVENT_TOUCH_RESET_END\n");
		break;
	case LCD_EVENT_LCD_BLANK:
		TOUCH_I("LCD_EVENT_LCD_BLANK\n");
		atomic_set(&ts->state.fb, FB_SUSPEND);
		break;
	case LCD_EVENT_LCD_UNBLANK:
		TOUCH_I("LCD_EVENT_LCD_UNBLANK\n");
		atomic_set(&ts->state.fb, FB_RESUME);
		break;
	case LCD_EVENT_LCD_MODE:
		TOUCH_I("LCD_EVENT_LCD_MODE\n");
		td4310_lcd_mode(dev, *(u32 *)data);
		queue_delayed_work(ts->wq, &d->fb_notify_work, 0);
		break;
	case NOTIFY_DEBUG_OPTION:
		TOUCH_I("NOTIFY_DEBUG_OPTION\n");
		ret = td4310_debug_option(dev, (u32 *)data);
		break;
	case NOTIFY_CONNECTION:
		TOUCH_I("NOTIFY_CONNECTION\n");
		TOUCH_I("TA Type: %d\n", atomic_read(&ts->state.connect));
		td4310_connect(dev);
		break;
	case NOTIFY_WIRELESS:
		TOUCH_I("NOTIFY_WIRELESS\n");
		TOUCH_I("Wireless charger: 0x%02X\n", atomic_read(&ts->state.wireless));
		break;
	case NOTIFY_EARJACK:
		TOUCH_I("NOTIFY_EARJACK\n");
		TOUCH_I("Earjack Type: 0x%02X\n", atomic_read(&ts->state.earjack));
		break;
	case NOTIFY_IME_STATE:
		TOUCH_I("NOTIFY_IME_STATE\n");
		break;
	case NOTIFY_CALL_STATE:
		if ((*(u32*)data >= INCOMING_CALL_IDLE) && (*(u32*)data <= INCOMING_CALL_LTE_OFFHOOK))
			TOUCH_I("NOTIFY_CALL_STATE\n");
		break;
	default:
		TOUCH_I("%s : not supported event = 0x%x\n", __func__, (unsigned int)event);
		break;
	}

	return ret;
}

static int td4310_notify(struct device *dev, ulong event, void *data)
{
	int boot_mode = TOUCH_NORMAL_BOOT;

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);

	if (boot_mode == TOUCH_CHARGER_MODE
			|| boot_mode == TOUCH_LAF_MODE
			|| boot_mode == TOUCH_RECOVERY_MODE) {
		TOUCH_I("%s : boot_mode = %d\n", __func__, boot_mode);
		return td4310_notify_etc(dev, event, data);
	}

	return td4310_notify_normal(dev, event, data);
}

#if defined(CONFIG_DRM) && defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
static int td4310_drm_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct msm_drm_notifier *ev = (struct msm_drm_notifier *)data;

	TOUCH_TRACE();

	if (ev && ev->data && event == MSM_DRM_EVENT_BLANK) {
		int *blank = (int *)ev->data;

		if (*blank == MSM_DRM_BLANK_UNBLANK)
			TOUCH_I("DRM_UNBLANK\n");
		else if (*blank == MSM_DRM_BLANK_POWERDOWN)
			TOUCH_I("DRM_POWERDOWN\n");
	}

	return 0;
}
#endif
#elif defined(CONFIG_FB)
static int td4310_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *ev = (struct fb_event *)data;

	if (ev && ev->data && event == FB_EVENT_BLANK) {
		int *blank = (int *)ev->data;

		if (*blank == FB_BLANK_UNBLANK)
			TOUCH_I("FB_UNBLANK\n");
		else if (*blank == FB_BLANK_POWERDOWN)
			TOUCH_I("FB_BLANK\n");
	}

	return 0;
}
#endif

static void td4310_init_works(struct td4310_data *d)
{
	INIT_DELAYED_WORK(&d->fb_notify_work, td4310_fb_notify_work_func);
}

static int td4310_recovery(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	struct i2c_client *client = to_i2c_client(dev);
	const struct firmware *fw = NULL;
	char fwpath[256] = {0, };
	u32 backup_slave_addr = client->addr;
	int ret = 0;

	TOUCH_TRACE();

	memcpy(fwpath, ts->def_fwpath[1], sizeof(fwpath));
	if (fwpath == NULL) {
		TOUCH_E("get fw path error\n");
		return -EPERM;
	}

	fwpath[sizeof(fwpath) - 1] = '\0';
	if (strlen(fwpath) <= 0) {
		TOUCH_E("error get fw path\n");
		return -EPERM;
	}

	TOUCH_I("%s : fwpath [%s]\n", __func__, fwpath);

	ret = request_firmware(&fw, fwpath, dev);
	if (ret < 0) {
		TOUCH_E("request_firmware error. fwpath[%s] (ret:%d)\n", fwpath, ret);
		return ret;
	}
	TOUCH_I("%s : fw->size[%zu], fw->data[%p]\n", __func__, fw->size, fw->data);

	if (atomic_read(&d->state.fw_recovery) == true) {
		TOUCH_I("%s : change uBL slave addr [0x%x -> 0x%x]\n",
					__func__, client->addr, d->uBL_addr);
		client->addr = d->uBL_addr;
	}

	ret = FirmwareRecovery(dev, fw);
	client->addr = backup_slave_addr;

	release_firmware(fw);

	return ret;
}

static int td4310_fw_compare(struct device *dev, const struct firmware *fw)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	struct td4310_version *device = &d->ic_info.version;
	struct td4310_version *binary = NULL;
	int update = 0;

	TOUCH_TRACE();

	memcpy(d->ic_info.img_product_id, &fw->data[d->ic_info.fw_pid_addr], 6);

	memcpy(d->ic_info.img_raws, &fw->data[d->ic_info.fw_ver_addr], 4);
	d->ic_info.img_version.major = d->ic_info.img_raws[3] & 0x80 ? 1 : 0;
	d->ic_info.img_version.minor = d->ic_info.img_raws[3] & 0x7F;
	binary = &d->ic_info.img_version;

	d->ic_info.bootloader_type = (fw->data[0x06] & 0x08) ? 1 : 0;

	if (ts->force_fwup) {
		update = 1;
		goto out;
	}

	if ((binary->major != device->major)
		|| (binary->minor != device->minor)
		|| (binary->build != device->build)) {
		update = 1;
	}

out:
	TOUCH_I("%s : binary[%d.%02d.%d] device[%d.%02d.%d] -> update: %d, force: %d\n",
			__func__,
			binary->major, binary->minor, binary->build,
			device->major, device->minor, device->build,
			update, ts->force_fwup);
	TOUCH_I("%s : FW bootloader type = %s\n",
			__func__, d->ic_info.bootloader_type ? "Optimized" : "Non-Optimized");

	return update;
}

static int td4310_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	struct i2c_client *client = to_i2c_client(dev);
	const struct firmware *fw = NULL;
	char fwpath[256] = {0};
	int ret = 0;

	TOUCH_TRACE();

	if (ts->test_fwpath[0]) {
		memcpy(fwpath, &ts->test_fwpath[0], sizeof(fwpath));
		TOUCH_I("test_fwpath : [%s]\n", &ts->test_fwpath[0]);
	} else if (ts->def_fwcnt) {
		memcpy(fwpath, ts->def_fwpath[0], sizeof(fwpath));
		TOUCH_I("def_fwpath : [%s]\n", ts->def_fwpath[0]);
	} else {
		TOUCH_E("no firmware file\n");
		return -EPERM;
	}

	fwpath[sizeof(fwpath) - 1] = '\0';
	if (strlen(fwpath) <= 0) {
		TOUCH_E("error get fw path\n");
		return -EPERM;
	}

	ret = request_firmware(&fw, fwpath, dev);
	if (ret < 0) {
		TOUCH_E("request_firmware error. fwpath[%s] (ret:%d)\n", fwpath, ret);
		return ret;
	}
	TOUCH_I("fw size : %zu, data : %p\n", fw->size, fw->data);
	TOUCH_I("i2c address [0x%x]\n", client->addr);

	if (atomic_read(&d->state.fw_recovery) == true) {
		ret = td4310_recovery(dev);
		if (ret < 0) {
			TOUCH_E("Firmware Recovery failed\n");
			goto error;
		}
		atomic_set(&d->state.scan_pdt, true);
		atomic_set(&d->state.fw_recovery, false);
	} else if (td4310_fw_compare(dev, fw) == 1) {
		atomic_set(&d->state.scan_pdt, true);
		ret = FirmwareUpgrade(dev, fw);
		if (ret < 0) {
			TOUCH_E("Firmware Upgrade failed\n");
			goto error;
		}
	} else {
		ret = -EPERM;
		goto error;
	}

	td4310_reset_ctrl(dev, POWER_SW_RESET);
error:
	release_firmware(fw);

	return ret;
}

static int td4310_probe(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = NULL;

	TOUCH_TRACE();

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d) {
		TOUCH_E("failed to allocate td4310_data\n");
		return -ENOMEM;
	}

	touch_set_device(ts, d);
	project_param_set(dev);

/* [bringup] mh3j
	touch_gpio_init(ts->reset_pin, "touch_reset");
	touch_gpio_direction_output(ts->reset_pin, 0); //[bringup] TD4300 is set 1
*/
	touch_gpio_init(ts->int_pin, "touch_int");
	touch_gpio_direction_input(ts->int_pin);

	touch_power_init(dev);
	touch_bus_init(dev, MAX_BUF_SIZE);

	td4310_init_works(d);

	td4310_init_tci_info(dev);
	pm_qos_add_request(&d->pm_qos_req, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	d->lcd_mode = LCD_MODE_U3;
	atomic_set(&d->state.scan_pdt,true);

	return 0;
}

static int td4310_irq_clear(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	u8 data[2] = {0, };
	u8 int_pin = 0;
	int retry = 0;
	int ret = 0;

	TOUCH_TRACE();

	do {
		memset(data, 0x00, sizeof(data));
		SYNA_READ(dev, DEVICE_STATUS_REG, &data, sizeof(data), ret, error);

		retry++;

		int_pin = gpio_get_value(ts->int_pin);
		TOUCH_I("status[device:0x%02x, interrupt:0x%02x, int_pin:%s], try:%d\n",
				data[0], data[1], int_pin ? "HIGH" : "LOW", retry);
	} while(!int_pin && (retry <= 6));

error:
	return ret;
}

static int td4310_set_configured(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);
	u8 dev_status = 0;
	u8 dev_ctrl = 0;
	int ret = 0;

	SYNA_READ(dev, DEVICE_STATUS_REG, &dev_status, sizeof(dev_status), ret, error);

	if (dev_status == DEV_NEED_CONF_AFTER_RST) {
		TOUCH_I("need configured. dev_status : 0x%02x\n", dev_status);

		dev_ctrl = DEV_REQUEST_CONF;
		SYNA_WRITE(dev, DEVICE_CONTROL_REG, &dev_ctrl, sizeof(dev_ctrl), ret, error);
		SYNA_READ(dev, DEVICE_STATUS_REG, &dev_status, sizeof(dev_status), ret, error);
		SYNA_READ(dev, DEVICE_CONTROL_REG, &dev_ctrl, sizeof(dev_ctrl), ret, error);
	}

	if (dev_status == DEV_CONFIFUTED) {
		TOUCH_I("device configured. dev_status : 0x%02x, ctrl_reg : 0x%02x\n",
				dev_status, dev_ctrl);
		atomic_set(&d->state.config, IC_CONFIGURED_DONE);
	} else {
		TOUCH_E("device configuration error\n");
	}

error:
	return ret;
}

static int td4310_init(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	atomic_set(&d->state.init, IC_INIT_NEED);

	TOUCH_I("charger_state = 0x%02X\n", atomic_read(&d->state.charger));

	ret = td4310_ic_info(dev);
	if (ret < 0) {
		TOUCH_E("get ic info error\n");
		if(atomic_read(&d->state.fw_recovery) == true) {
			ret = td4310_recovery(dev);
			if (ret < 0) {
				TOUCH_E("Firmware Recovery failed\n");
				if (atomic_read(&d->state.panel_reset_flag) < PANEL_RESET_MAX) {
					goto panel_reset;
				}
			}
			atomic_set(&d->state.scan_pdt, true);
			atomic_set(&d->state.fw_recovery, false);
			ret = td4310_ic_info(dev);
			if (ret < 0) {
				TOUCH_E("get ic info error\n");
				if (atomic_read(&d->state.panel_reset_flag) < PANEL_RESET_MAX) {
					goto panel_reset;
				}
			}
		}
		if(ts->force_fwup == 1) {
			TOUCH_I("force FW upgrade\n");
			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			ret = td4310_upgrade(dev);
			ts->force_fwup = 0;
			touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
			ret |= td4310_ic_info(dev);
			if (ret < 0) {
				TOUCH_E("get ic info error\n");
				goto panel_reset;
			}
		}
	}

	atomic_set(&d->state.init, IC_INIT_DONE);
	atomic_set(&d->state.esd_recovery, ESD_RECOVERY_DONE);
	atomic_set(&d->state.panel_reset_flag, PANEL_RESET_NEED);
	atomic_set(&d->state.enter_lpwg, ENTER_LPWG_NEED);

	td4310_lpwg_mode(dev);
	td4310_set_configured(dev);
	td4310_rmidev_init(dev);

	if (!gpio_get_value(ts->int_pin)) {
		TOUCH_I("int_pin [LOW], need irq clear\n");
		td4310_irq_clear(dev);
	} else {
		TOUCH_I("int_pin [HIGH]\n");
	}

	return 0;

panel_reset:
	atomic_inc(&d->state.panel_reset_flag);
	mod_delayed_work(ts->wq, &ts->panel_reset_work, 0);
	return 0;
}

static int td4310_suspend(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int boot_mode = TOUCH_NORMAL_BOOT;
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_I("%s Start\n", __func__);

	boot_mode = touch_check_boot_mode(dev);

	if (boot_mode == TOUCH_CHARGER_MODE
			|| boot_mode == TOUCH_LAF_MODE
			|| boot_mode == TOUCH_RECOVERY_MODE) {
		TOUCH_I("%s : boot_mode = %d\n", __func__, boot_mode);
		ret = -EPERM;
		goto out;
	}

	if ((boot_mode >= TOUCH_MINIOS_MFTS_FOLDER) && !ts->mfts_lpwg) {
		TOUCH_I("%s : - MFTS\n", __func__);
		//TD4310_power(dev, POWER_OFF);
		touch_interrupt_control(dev, INTERRUPT_DISABLE);
		td4310_sleep_control(dev, 1);
		ret = -EPERM;
		goto out;
	}

	if (atomic_read(&d->state.init) == IC_INIT_DONE) {
		td4310_lpwg_mode(dev);
	} else {
		if (atomic_read(&d->state.esd_recovery) == ESD_RECOVERY_DONE) {
			ret = 1;
		} else {
			TOUCH_I("Global Reset in progress... skip init\n");
		}
	}

out:
	TOUCH_I("%s End\n", __func__);

	return ret;
}

static int td4310_resume(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int boot_mode = TOUCH_NORMAL_BOOT;
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_I("%s Start\n", __func__);

	boot_mode = touch_check_boot_mode(dev);

	if (boot_mode == TOUCH_CHARGER_MODE
			|| boot_mode == TOUCH_LAF_MODE
			|| boot_mode == TOUCH_RECOVERY_MODE) {
		TOUCH_I("%s: boot_mode = %d\n", __func__, boot_mode);
		touch_interrupt_control(dev, INTERRUPT_DISABLE);
		td4310_sleep_control(dev, 1);
		ret = -EPERM;
		goto out;
	}

	if ((boot_mode >= TOUCH_MINIOS_MFTS_FOLDER) && !ts->mfts_lpwg) {
		TOUCH_I("%s : - MFTS\n", __func__);
		//TD4310_power(dev, POWER_ON);
		//touch_msleep(ts->caps.hw_reset_delay);
		td4310_sleep_control(dev, 0);
		atomic_set(&d->state.scan_pdt, true);
		ret = td4310_ic_info(dev);
			if (ret < 0) {
				TOUCH_E("get ic info error\n");
				goto error;
			}
	} else {
		touch_interrupt_control(dev, INTERRUPT_DISABLE);
		//td4310_reset_ctrl(dev, SW_RESET);
	}

out:
error:
	TOUCH_I("%s End\n", __func__);

	return 0;
}

static int td4310_esd_recovery(struct device *dev)
{
#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY) || IS_ENABLED(CONFIG_LGE_TOUCH_PANEL_GLOBAL_RESET)
	struct touch_core_data *ts = to_touch_core(dev);
#endif

	TOUCH_TRACE();
	TOUCH_I("%s : Request panel reset !!\n", __func__);

#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY) || IS_ENABLED(CONFIG_LGE_TOUCH_PANEL_GLOBAL_RESET)
#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	lge_mdss_report_panel_dead(PANEL_HW_RESET);
#elif defined(CONFIG_LGE_TOUCH_CORE_MTK)
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	mtkfb_esd_recovery();
#endif
#endif

	return 0;
}

static int td4310_remove(struct device *dev)
{
	struct td4310_data *d = to_td4310_data(dev);

	TOUCH_TRACE();

	pm_qos_remove_request(&d->pm_qos_req);

	if (rmidev_fhandler.initialized
		&& rmidev_fhandler.insert) {
		rmidev_fhandler.exp_fn->remove(dev);
		rmidev_fhandler.initialized = false;
	}

	return 0;
}

static int td4310_shutdown(struct device *dev)
{
	TOUCH_TRACE();
	return 0;
}

static int td4310_init_pm(struct device *dev)
{
#if defined(CONFIG_FB)
	struct touch_core_data *ts = to_touch_core(dev);
#endif

	TOUCH_TRACE();

#if defined(CONFIG_DRM) && defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
	TOUCH_I("%s: drm_notif change\n", __func__);
	ts->drm_notif.notifier_call = td4310_drm_notifier_callback;
#endif
#elif defined(CONFIG_FB)
	TOUCH_I("%s: fb_notif change\n", __func__);
	fb_unregister_client(&ts->fb_notif);
	ts->fb_notif.notifier_call = td4310_fb_notifier_callback;
#endif

	return 0;
}

static ssize_t store_reg_ctrl(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u8 data[50] = {0};
	char command[6] = {0};
	int page = 0;
	u32 reg = 0;
	int offset = 0;
	u32 value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%5s %d %x %d %x ", command, &page, &reg, &offset, &value) <= 0)
		return count;

	if (offset < 0 || offset > 49) {
		TOUCH_E("invalid offset[%d] (range : 0 ~ 49)\n", offset);
		return count;
	}

	mutex_lock(&ts->lock);
	td4310_set_page(dev, page);

	if (!strcmp(command, "write")) {
		td4310_read(dev, reg, data, offset + 1);
		data[offset] = (u8)value;
		td4310_write(dev, reg, data, offset + 1);
	} else if (!strcmp(command, "read")) {
		td4310_read(dev, reg, data, offset + 1);
		TOUCH_I("page[%d] reg[%x] offset[%d] = 0x%x\n",
				page, reg, offset, data[offset]);
	} else {
		TOUCH_E("Usage\n");
		TOUCH_E("Write page reg offset value\n");
		TOUCH_E("Read page reg offset\n");
	}

	td4310_set_page(dev, DEFAULT_PAGE);
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t store_reset_ctrl(struct device *dev, const char *buf, size_t count)
{
	int value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value < POWER_HW_RESET_ASYNC || value > POWER_SW_RESET) {
		TOUCH_E("invalid input %d\n", value);
		return count;
	}
	td4310_reset_ctrl(dev, value);

	return count;
}

static ssize_t show_noise_log(struct device *dev, char *buf)
{
	struct td4310_data *d = to_td4310_data(dev);

	int offset = 0;

	TOUCH_TRACE();

	offset += snprintf(buf + offset, PAGE_SIZE - offset, "NOISE_LOG_%s\n",
			(d->noise.noise_log == NOISE_ENABLE) ? "ENABLE" : "DISABLE");

	return offset;
}

static ssize_t store_noise_log(struct device *dev, const char *buf, size_t count)
{
	struct td4310_data *d = to_td4310_data(dev);

	int value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (d->noise.noise_log < NOISE_DISABLE || d->noise.noise_log > NOISE_ENABLE) {
		TOUCH_I("invalid input. 0 or 1.\n");
		return count;
	}

	d->noise.noise_log = value;

	TOUCH_I("NOISE_LOG_%s\n", (d->noise.noise_log == NOISE_ENABLE)
			? "ENABLE" : "DISABLE");

	return count;
}

static ssize_t show_esd_test(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int ret = 0;

	if (atomic_read(&d->state.esd_recovery) == ESD_RECOVERY_DONE) {
		if(d->skip_global_reset) {
			TOUCH_I("%s : skip global reset\n", __func__);
		} else {
			TOUCH_I("###### ESD test - Call ESD Recovery ######\n");
			atomic_set(&d->state.scan_pdt, true);
			atomic_set(&d->state.init, IC_INIT_NEED);
			atomic_set(&d->state.esd_recovery, ESD_RECOVERY_NEED);
			queue_delayed_work(ts->wq, &ts->panel_reset_work, 0);
		}
	} else {
		TOUCH_I("###### ESD test - In Progress.....\n");
	}

	return ret;
}

static ssize_t store_esd_test(struct device *dev, const char *buf, size_t count)
{
	struct td4310_data *d = to_td4310_data(dev);

	int value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%d", &value) <= 0)
		return count;


	if (value < 0 || value > 1) {
		TOUCH_I("invalid input. 0 or 1.\n");
		return count;
	}

	d->skip_global_reset = (u8)value;

	TOUCH_I("skip_global_reset = %d\n", d->skip_global_reset);

	return count;
}


static ssize_t show_fw_recovery(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct td4310_data *d = to_td4310_data(dev);
	int offset = 0;

	TOUCH_TRACE();

	TOUCH_I("FirmwareRcovery Start\n");
	offset += snprintf(buf + offset, PAGE_SIZE - offset, "%s\n",
			"FirmwareRecovery Start");

	atomic_set(&ts->state.core, CORE_UPGRADE);
	mutex_lock(&ts->lock);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);

	td4310_recovery(dev);

	atomic_set(&d->state.scan_pdt,true);
	ts->force_fwup = 0;
	ts->test_fwpath[0] = '\0';

	td4310_init(dev);

	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	mutex_unlock(&ts->lock);
	atomic_set(&ts->state.core, CORE_NORMAL);

	TOUCH_I("FirmwareRcovery Finish\n");
	offset += snprintf(buf + offset, PAGE_SIZE - offset, "%s\n",
			"FirmwareRecovery Finish");

	return offset;
}

static TOUCH_ATTR(reg_ctrl, NULL, store_reg_ctrl);
static TOUCH_ATTR(reset_ctrl, NULL, store_reset_ctrl);
static TOUCH_ATTR(noise_log, show_noise_log, store_noise_log);
static TOUCH_ATTR(esd_test, show_esd_test, store_esd_test);
static TOUCH_ATTR(fw_recovery, show_fw_recovery, NULL);

static struct attribute *td4310_attribute_list[] = {
	&touch_attr_reg_ctrl.attr,
	&touch_attr_reset_ctrl.attr,
	&touch_attr_noise_log.attr,
	&touch_attr_esd_test.attr,
	&touch_attr_fw_recovery.attr,
	NULL,
};

static const struct attribute_group td4310_attribute_group = {
	.attrs = td4310_attribute_list,
};

static int td4310_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = sysfs_create_group(&ts->kobj, &td4310_attribute_group);
	if (ret < 0) {
		TOUCH_E("td4310 sysfs register failed\n");
		goto error;
	}

	ret = td4310_lpwg_register_sysfs(dev);
	if (ret < 0) {
		TOUCH_E("td4310 lpwg sysfs register failed\n");
		goto error;
	}

	ret = td4310_prd_register_sysfs(dev);
	if (ret < 0) {
		TOUCH_E("td4310 prd sysfs register failed\n");
		goto error;
	}

	return 0;

error:
	kobject_del(&ts->kobj);

	return ret;
}

static char *td4310_productcode_parse(unsigned char *product)
{
	const char *str_panel[] = { "ELK", "Suntel", "Tovis", "Innotek", "JDI", "LGD", "CDOT", };
	const char *str_ic[] = { "Synaptics", };

	static char str[128] = {0, };
	int i = 0;
	int len = 0;
	char data[2] = {0, };

	TOUCH_TRACE();

	i = ((product[0] & 0xF0) >> 4);
	len += snprintf(str + len, sizeof(str) - len, "%s\n",
			(i < 7) ? str_panel[i] : "Unknown panel vendor");

	i = (product[0] & 0x0F);
	data[0] = (char)i;
	if (i < 5 && i != 1)
		len += snprintf(str + len, sizeof(str) - len, "%d keys\n", data[0]);
	else
		len += snprintf(str + len, sizeof(str) - len, "Unknown keys\n");

	i = ((product[1] & 0xF0) >> 4);
	len += snprintf(str + len, sizeof(str) - len, "%s\n",
			(i < 1) ? str_ic[i] : "Unknown IC");

	data[0] = (product[1] & 0x0F);
	data[1] = ((product[2] & 0xF0) >> 4);
	len += snprintf(str + len, sizeof(str) - len, "%d.%d inch\n", data[0], data[1]);

	data[0] = (product[2] & 0x0F);
	len += snprintf(str + len, sizeof(str) - len, "PanelType %d\n", data[0]);

	data[0] = ((product[3] & 0x80) >> 7);
	data[1] = (product[3] & 0x7F);
	len += snprintf(str + len, sizeof(str) - len, "version : v%d.%02d\n", data[0], data[1]);

	return str;
}

static int td4310_get_cmd_version(struct device *dev, char *buf)
{
	struct td4310_data *d = to_td4310_data(dev);
	int offset = 0;

	TOUCH_TRACE();

	offset = snprintf(buf + offset, PAGE_SIZE - offset,
			"\n======== Firmware Info ========\n");

	/* IC version info */
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"ic_version RAW = %02X %02X %02X %02X\n",
			d->ic_info.raws[0],
			d->ic_info.raws[1],
			d->ic_info.raws[2],
			d->ic_info.raws[3]);

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"=== ic_fw_version info ===\n%s",
			td4310_productcode_parse(d->ic_info.raws));

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"IC_product_id[%s]\n", d->ic_info.product_id);

	if (!strncmp(d->ic_info.product_id, "PLG673", 6))
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Touch IC : TD4310\n\n");
	else
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Touch product ID read fail\n\n");

	/* image version info */
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"img_version RAW = %02X %02X %02X %02X\n",
			d->ic_info.img_raws[0],
			d->ic_info.img_raws[1],
			d->ic_info.img_raws[2],
			d->ic_info.img_raws[3]);

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"=== img_version info ===\n%s",
			td4310_productcode_parse(d->ic_info.img_raws));

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Img_product_id[%s]\n", d->ic_info.img_product_id);

	if (!strncmp(d->ic_info.img_product_id, "PLG673", 6))
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Touch IC : TD4310\n\n");
	else
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Touch product ID read fail\n");

	if(d->ic_info.bootloader_type)
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Bootloader Type : Optimized\n");
	else
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Bootloader Type : Non-Optimized\n");

	return offset;
}

static int td4310_get_cmd_atcmd_version(struct device *dev, char *buf)
{
	struct td4310_data *d = to_td4310_data(dev);
	int offset = 0;

	TOUCH_TRACE();

	offset = snprintf(buf + offset, PAGE_SIZE - offset,
			"v%d.%02d(0x%X/0x%X/0x%X/0x%X)\n",
			d->ic_info.version.major,
			d->ic_info.version.minor,
			d->ic_info.raws[0],
			d->ic_info.raws[1],
			d->ic_info.raws[2],
			d->ic_info.raws[3]);

	return offset;
}

static int td4310_get(struct device *dev, u32 cmd, void *input, void *output)
{
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_D(BASE_INFO, "%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
	case CMD_VERSION:
		ret = td4310_get_cmd_version(dev, (char *)output);
		break;

	case CMD_ATCMD_VERSION:
		ret = td4310_get_cmd_atcmd_version(dev, (char *)output);
		break;

	default:
		break;
	}

	return ret;
}

static int td4310_set(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_TRACE();

	return 0;
}

static struct touch_driver touch_driver = {
	.probe = td4310_probe,
	.remove = td4310_remove,
	.shutdown = td4310_shutdown,
	.suspend = td4310_suspend,
	.resume = td4310_resume,
	.init = td4310_init,
	.irq_handler = td4310_irq_handler,
	.power = td4310_power,
	.upgrade = td4310_upgrade,
	.esd_recovery = td4310_esd_recovery,
	.lpwg = td4310_lpwg,
	.swipe_enable = td4310_swipe_enable,
	.notify = td4310_notify,
	.init_pm = td4310_init_pm,
	.register_sysfs = td4310_register_sysfs,
	.set = td4310_set,
	.get = td4310_get,
};

#define MATCH_NAME	"synaptics,TD4310"
static struct of_device_id touch_match_ids[] = {
	{ .compatible = MATCH_NAME, },
	{},
};

static struct touch_hwif hwif = {
	.bus_type = HWIF_I2C,
	.name = LGE_TOUCH_NAME,
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(touch_match_ids),
};

static int __init touch_device_init(void)
{
	TOUCH_TRACE();

	TOUCH_I("%s, TD4310 found!\n", __func__);

	return touch_bus_device_init(&hwif, &touch_driver);
}

static void __exit touch_device_exit(void)
{
	TOUCH_TRACE();

	touch_bus_device_exit(&hwif);
}

module_init(touch_device_init);
module_exit(touch_device_exit);

MODULE_AUTHOR("BSP-TOUCH@lge.com");
MODULE_DESCRIPTION("LGE touch driver v3");
MODULE_LICENSE("GPL");
