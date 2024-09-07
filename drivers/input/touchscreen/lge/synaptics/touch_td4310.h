/* touch_synaptics.h
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

#ifndef LGE_TOUCH_SYNAPTICS_H
#define LGE_TOUCH_SYNAPTICS_H

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/firmware.h>
#include <linux/input/lge_touch_notify.h>
#include <linux/spi/spi.h>
#include <linux/pm_qos.h>

#define PLATFORM_DRIVER_NAME 		"synaptics_dsx"

#define PAGE_SELECT_REG			0xff
#define PAGES_TO_SERVICE		10

#define DEFAULT_PAGE			0x00
//#define COMMON_PAGE			(d->f01.page)
//#define FINGER_PAGE			(d->f12.page)
//#define FLASH_PAGE			(d->f34.page)
#define LPWG_PAGE			(d->f51.page)
#define ANALOG_PAGE			(d->f54.page)

#define PDT_START_ADDR			0x00E9
#define PDT_END_ADDR			0x00DD

#define TD4310_FUNC_01			(u8)(1U << 0)
#define TD4310_FUNC_12			(u8)(1U << 1)
#define TD4310_FUNC_34			(u8)(1U << 2)
#define TD4310_FUNC_35			(u8)(1U << 3)
#define TD4310_FUNC_51			(u8)(1U << 4)
#define TD4310_FUNC_54			(u8)(1U << 5)
#define TD4310_FUNC_55			(u8)(1U << 6)
#define TD4310_FUNC_DC			(u8)(1U << 7)

/* -- DEVICE -- */
#define DEVICE_STATUS_REG		(d->f01.dsc.data_base)
#define DEVICE_CONTROL_REG		(d->f01.dsc.control_base)
#define DEVICE_COMMAND_REG		(d->f01.dsc.command_base)
#define DEV_SW_RESET_REG		DEVICE_COMMAND_REG
#define DEV_SW_RESET_BIT		0x01
#define DEV_SLEEP_CTRL_CLEAR_MASK	0xf8

/* DEVICE STATUS */
#define DEV_STATUS_MASK			0x0F
#define DEV_STS_NORMAL_MASK		0x00
#define DEV_RESET_MASK			0x01
#define DEV_INVALID_CONF_MASK		0x02
#define DEV_DEVICE_FAIL_MASK		0x03
#define DEV_CONF_CRC_FAIL_MASK		0x04
#define DEV_FW_CRC_FAIL_MASK		0x05
#define DEV_CRC_PROGRESS_MASK		0x06
#define DEV_GUEST_CRC_FAIL_MASK		0x07
#define DEV_EXT_AFE_FAIL_MASK		0x08
#define DEV_DISPLAY_FAIL_MASK		0x09

#define DEV_FLASH_PROGRESS_MASK		(1U << 6)
#define DEV_DEVICE_UNCONF_MASK		(1U << 7)

/* GLOBAL RESET CONDITIONS */
#define GR_ESD_AFTER_CONFIGURED		0x09
#define GR_FAIL_TO_FW_LOADING		0x45
#define GR_ABNORMAL_RESET		0x81
#define GR_ESD_UNCONFIGURED		0x89

/* ANALOG COMMAND */
#define ANALOG_COMMAND_REG		(d->f54.dsc.command_base)
#define FORCE_UPDATE_REG		ANALOG_COMMAND_REG
#define GET_REPORT_REG			ANALOG_COMMAND_REG
#define FORCE_UPDATE_BIT		0X04
#define FORCE_UPDATE_CLEAR_BIT		0X00

/* IC INFO */
#define IC_FW_VERSION_REG		(d->f34.dsc.control_base)
#define CUSTOMER_FAMILY_REG		(d->f01.dsc.query_base + 2)
#define FW_REVISION_REG			(d->f01.dsc.query_base + 3)
#define PRODUCT_ID_REG			(d->f01.dsc.query_base + 11)

/* F12 FUNCTION */
#define F12_SIZE_OF_CTRL_PRESENCE_REG	(d->f12.dsc.query_base + 4)
#define F12_CTRL_PRESENCE_REG		(d->f12.dsc.query_base + 5)
#define F12_SIZE_OF_DATA_PRESENCE_REG	(d->f12.dsc.query_base + 7)
#define F12_DATA_PRESENCE_REG		(d->f12.dsc.query_base + 8)
#define F12_OBJECT_REPORT_ENABLE_REG	(d->f12_reg.ctrl[23])
#define F12_MAX_XY_COORDINATE_REG	(d->f12_reg.ctrl[8])

/* SLEEP CONTROL */
#define DEV_CTRL_NORMAL_OP		0x00
#define DEV_CTRL_SLEEP			0x01
//#define DEV_CTRL_NOSLEEP		0x04

/* SET CONFIGURED */
#define DEV_CONFIFUTED			0x00
#define DEV_REQUEST_CONF		0x80
#define DEV_NEED_CONF_AFTER_RST		0x81


/* -- INTERRUPT -- */
//#define INT_STATUS_REG		(d->f01.dsc.data_base + 1)
//#define INT_FLASH_MASK		(1U << 0)
//#define INT_STATUS_MASK		(1U << 1)
#define INT_ABS_MASK			(1U << 2)
//#define INT_BUTTON_MASK		(1U << 4)
#define INT_LPWG_MASK			(1U << 5)

/* ABS */
#define ABS_DATA_REG			(d->f12.dsc.data_base)
#define ABS_ATTN_REG			(d->f12.dsc.data_base + 2)//(d->f12_reg.data[15])

#define ABS_NO_OBJECT			0x00
#define ABS_FINGER			0x01
//#define ABS_STYLUS			0x02
#define ABS_PALM			0x03
//#define ABS_HOVER_FINGER		0x05
//#define ABS_GLOVED_FINGER		0x06
#define ABS_MAX_OBJECT			0x06

/* -- NOISE MODE -- */
#define INTERFERENCE_METRIC_LSB_REG	(d->f54.dsc.data_base + 4)
#define INTERFERENCE_METRIC_MSB_REG	(d->f54.dsc.data_base + 5)
#define CURRENT_NOISE_STATUS_REG	(d->f54.dsc.data_base + 8)
#define CID_IM_REG			(d->f54.dsc.data_base + 10)
#define FREQ_SCAN_IM_REG		(d->f54.dsc.data_base + 11)


/* -- PRD -- */
#define SET_REPORT_TYPE_REG		(d->f54.dsc.data_base)
#define RESET_REPORT_INDEX_LSB_REG	(d->f54.dsc.data_base + 1)
#define RESET_REPORT_INDEX_MSB_REG	(d->f54.dsc.data_base + 2)
#define READ_REPORT_DATA_REG		(d->f54.dsc.data_base + 3)

/* PRD - ELECTRODE OPEN */
#define ANALOG_CONTROL2_REG		(d->f54.dsc.control_base + 36)// Transcap/Feedback Capacitance
#define WAVEFORM_DURATION_CTRL_REG	(d->f54.dsc.control_base + 41)
#define CBC_TIMING_CONTROL_REG		(d->f54.dsc.control_base + 46)


/* -- FW UPGRADE -- */
#define F35_ERROR_CODE_OFFSET		0
#define F35_FLASH_STATUS_OFFSET  	5
#define F35_CHUNK_NUM_LSB_OFFSET	0
//#define F35_CHUNK_NUM_MSB_OFFSET	1
#define F35_CHUNK_DATA_OFFSET		2
#define F35_CHUNK_COMMAND_OFFSET	18
#define F35_CHUNK_SIZE			16
#define F35_ERASE_ALL_WAIT_MS 		8000
#define F35_RESET_WAIT_MS		250


enum{
	NOISE_DISABLE = 0,
	NOISE_ENABLE,
};

enum{
	CONNECT_NONE = 0,
	CONNECT_USB,
	CONNECT_TA,
	CONNECT_OTG,
};

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
enum {
	CONNECT_INVALID = 0,
	CONNECT_SDP,
	CONNECT_DCP,
	CONNECT_CDP,
	CONNECT_PROPRIETARY,
	CONNECT_FLOATED,
	CONNECT_HUB,
};
#elif defined(CONFIG_LGE_TOUCH_CORE_MTK)
enum {
	CONNECT_CHARGER_UNKNOWN = 0,
	CONNECT_STANDARD_HOST,		/* USB : 450mA */
	CONNECT_CHARGING_HOST,
	CONNECT_NONSTANDARD_CHARGER,	/* AC : 450mA~1A */
	CONNECT_STANDARD_CHARGER,	/* AC : ~1A */
	CONNECT_APPLE_2_1A_CHARGER,	/* 2.1A apple charger */
	CONNECT_APPLE_1_0A_CHARGER,	/* 1A apple charger */
	CONNECT_APPLE_0_5A_CHARGER,	/* 0.5A apple charger */
	CONNECT_WIRELESS_CHARGER,
	CONNECT_DISCONNECTED,
};
#endif

enum {
	IC_INIT_NEED = 0,
	IC_INIT_DONE,
};

enum {
	IC_CONFIGURED_NEED = 0,
	IC_CONFIGURED_DONE,
};

enum {
	ESD_RECOVERY_NEED = 0,
	ESD_RECOVERY_DONE,
};

enum {
	PANEL_RESET_NEED = 0,
	PANEL_RESET_MAX = 3,
};

enum {
	LCD_MODE_U0 = 0,
	LCD_MODE_U2_UNBLANK,
	LCD_MODE_U2,
	LCD_MODE_U3,
	LCD_MODE_U3_PARTIAL,
	LCD_MODE_U3_QUICKCOVER,
	LCD_MODE_STOP,
};

enum {
	ENTER_LPWG_NEED = 0,
	ENTER_LPWG_DONE,
};

struct function_descriptor {
	u8 query_base;
	u8 command_base;
	u8 control_base;
	u8 data_base;
	u8 int_source_count;
	u8 fn_number;
};

struct td4310_function {
	struct function_descriptor dsc;
	u8 page;
};

struct td4310_f12_reg {
	u8 *ctrl;
	u8 *data;
};

struct td4310_version {
	u8 build : 4;
	u8 major : 4;
	u8 minor;
};

struct td4310_ic_info {
	u8 family;
	u8 revision;
	u8 product_id[10];
	u8 raws[4];
	struct td4310_version version;

	u32 fw_pid_addr;
	u8 img_product_id[10];
	u32 fw_ver_addr;
	u8 img_raws[4];
	struct td4310_version img_version;

	u8 bootloader_type;
};

struct td4310_touch_data {
	u8 type;
	u8 x_lsb;
	u8 x_msb;
	u8 y_lsb;
	u8 y_msb;
	u8 z;
	u8 wx;
	u8 wy;
} __packed;

struct td4310_touch_info {
	u8 device_status;
	u8 irq_status;
	u8 touch_cnt:4;
	struct td4310_touch_data data[10];
} __packed;

struct td4310_state_info {
	atomic_t init;
	atomic_t config;
	atomic_t scan_pdt;
	atomic_t fw_recovery;
	atomic_t esd_recovery;
	atomic_t panel_reset_flag;
	atomic_t charger;
	atomic_t enter_lpwg;
};

struct td4310_noise_info {
	unsigned long noise_log;
	unsigned long cnt;

	unsigned long im_sum;
	unsigned long cns_sum;
	unsigned long cid_im_sum;
	unsigned long freq_scan_im_sum;
};

struct td4310_data {
	/* change u8 to u16, if more than 8 functions */
	u8 td4310_function_bits;
	struct td4310_function f01;
	struct td4310_function f12;
	struct td4310_function f34;
	struct td4310_function f35;
	struct td4310_function f51;
	struct td4310_function f54;
	struct td4310_function f55;
	struct td4310_function fdc;

	struct td4310_f12_reg f12_reg;

	struct td4310_ic_info ic_info;
	struct td4310_touch_info info;
	struct td4310_state_info state;
	struct td4310_noise_info noise;

	u8 uBL_addr;
	u8 lpwg_fail_reason;
	u8 is_palm;
	u8 lcd_mode;
	u8 skip_global_reset;

	struct delayed_work fb_notify_work;
	struct pm_qos_request pm_qos_req;

};

struct td4310_rmidev_exp_fn {
	int (*init)(struct device *dev);
	void (*remove)(struct device *dev);
	void (*reset)(struct device *dev);
	void (*reinit)(struct device *dev);
	void (*early_suspend)(struct device *dev);
	void (*suspend)(struct device *dev);
	void (*resume)(struct device *dev);
	void (*late_resume)(struct device *dev);
	void (*attn)(struct device *dev, unsigned char intr_mask);
};

struct td4310_rmidev_exp_fhandler {
	struct td4310_rmidev_exp_fn *exp_fn;
	bool insert;
	bool initialized;
	bool remove;
};

#define SYNA_READ(dev, reg, data, size, ret, error)		\
	do {							\
		ret = td4310_read(dev, reg, data, size);	\
		if (ret < 0) {					\
			TOUCH_E("read %s error\n", #reg);	\
			goto error;				\
		}						\
	} while (0)

#define SYNA_WRITE(dev, reg, data, size, ret, error)		\
	do {							\
		ret = td4310_write(dev, reg, data, size);	\
		if (ret < 0) {					\
			TOUCH_E("write %s error\n", #reg);	\
			goto error;				\
		}						\
	} while (0)

static inline struct td4310_data *to_td4310_data(struct device *dev)
{
	return (struct td4310_data *)touch_get_device(to_touch_core(dev));
}

void td4310_rmidev_function(struct td4310_rmidev_exp_fn *exp_fn, bool insert);
int td4310_force_update(struct device *dev);
int td4310_read(struct device *dev, u8 addr, void *data, int size);
int td4310_write(struct device *dev, u8 addr, void *data, int size);
int td4310_set_page(struct device *dev, u8 page);
void td4310_reset_ctrl(struct device *dev, int ctrl);
int td4310_sleep_control(struct device *dev, u8 mode);

/* extern function */
extern int FirmwareUpgrade(struct device *dev, const struct firmware *fw);
extern int FirmwareRecovery(struct device *dev, const struct firmware *fw);

#endif /* LGE_TOUCH_SYNAPTICS_H */
