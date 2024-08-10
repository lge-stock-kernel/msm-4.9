#ifndef __HW_PD_DEV_H__
#define __HW_PD_DEV_H__

#include "tusb422_linux.h"

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/extcon.h>
#include <linux/usb/class-dual-role.h>

#include <soc/qcom/lge/board_lge.h>

#if defined(CONFIG_LGE_USB_DEBUGGER) || defined(CONFIG_LGE_USB_MOISTURE_DETECT)
#include <linux/qpnp/qpnp-adc.h>
#endif

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#include <linux/interrupt.h>

/* must uncomment MOISTURE_DETECT_USE_SBU_TEST */
//#define MOISTURE_DETECT_USE_SBU_TEST

#define SBU_VBUS_SHORT_THRESHOLD 1200000	/* uV */
#endif

#ifdef CONFIG_LGE_USB_FACTORY
#define IS_FACTORY_MODE \
	(lge_get_factory_boot() || \
	 lge_get_laf_mode() || \
	 lge_get_laf_mid())
#endif
#define IS_CHARGERLOGO (lge_get_boot_mode() == LGE_BOOT_MODE_CHARGERLOGO)
struct hw_pd_dev {
	struct device *dev;

	/* usb */
	struct power_supply *usb_psy;
	struct notifier_block psy_nb;

	int mode;
	int pr;
	int dr;

	bool in_explicit_contract;

	enum power_supply_type psy_type;
	struct work_struct start_periph_work;
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	struct work_struct start_moisture_detect_work;
#endif
	struct gpio_desc *redriver_sel_gpio;
#ifdef CONFIG_MACH_SDM450_WIDEPD
	struct gpio_desc *redriver_en_gpio;
	struct gpio_desc *sbu_sw_sel1_gpio;
	struct gpio_desc *sbu_sw_sel2_gpio;
#endif
	struct gpio_desc *usb_ss_en_gpio;

	struct extcon_dev *extcon;

#if defined(CONFIG_MACH_SDM450_WIDEPD)
	struct regulator *vreg_fusb_en;		// vreg18 for fusb340
#endif
	/* charger */
	struct regulator *vbus_reg;

	bool is_otg;
	bool is_present;
	int curr_max;
	int volt_max;

#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
	bool is_debug_accessory;
#endif
#ifdef CONFIG_LGE_USB_DEBUGGER
	struct work_struct usb_debugger_work;
#endif
#if defined(CONFIG_LGE_USB_DEBUGGER) || defined(CONFIG_LGE_USB_MOISTURE_DETECT) || defined(CONFIG_MACH_SDM450_WIDEPD)
	struct gpio_desc *sbu_sel_gpio;
	struct gpio_desc *sbu_en_gpio;
	struct gpio_desc *edge_sel_gpio;
#endif

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	bool moisture_detect_use_sbu;
	struct gpio_desc *cc_protect_gpio;
	int cc_protect_irq;
	struct qpnp_vadc_chip	*vadc;
	struct delayed_work init_sbu_adc_work;
	bool adc_initialized;
	int sbu_ov_cnt;
	bool is_sbu_ov;
	bool moisture_detect_ux;
#endif
};

enum pd_dpm_pe_evt {
	PD_DPM_PE_EVT_SOURCE_VBUS,
	PD_DPM_PE_EVT_DIS_VBUS_CTRL,
	PD_DPM_PE_EVT_SINK_VBUS,
	PD_DPM_PE_EVT_PD_STATE,
	PD_DPM_PE_EVT_TYPEC_STATE,
	PD_DPM_PE_EVT_DR_SWAP,
	PD_DPM_PE_EVT_PR_SWAP,
#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
	PD_DPM_PE_EVT_DEBUG_ACCESSORY,
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	PD_DPM_PE_EVENT_GET_SBU_ADC,
	PD_DPM_PE_EVENT_SET_MOISTURE_DETECT_USE_SBU,
#endif
};

struct pd_dpm_vbus_state {
	uint8_t vbus_type;
	uint16_t mv;
	uint16_t ma;
	bool ext_power;
};

enum pd_connect {
	PD_CONNECT_PE_READY_SRC,
	PD_CONNECT_PE_READY_SNK,
};

struct pd_dpm_pd_state {
	enum pd_connect connected;
	bool usb_comm;
};

enum pd_dpm_typec {
	PD_DPM_TYPEC_UNATTACHED,
	PD_DPM_TYPEC_ATTACHED_SRC,
	PD_DPM_TYPEC_ATTACHED_SNK,
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	PD_DPM_TYPEC_CC_FAULT,
#endif
};

struct pd_dpm_typec_state {
	bool polarity;
	enum pd_dpm_typec new_state;
	enum power_supply_typec_mode typec_mode;
};

struct pd_dpm_swap_state {
	uint8_t new_role;
};

int pd_dpm_handle_pe_event(enum pd_dpm_pe_evt event, void *state);
int hw_pd_dev_init(struct device *dev);

#ifdef CONFIG_MACH_SDM450_WIDEPD
void handle_sbu_switch(bool enable, bool moisture_en);
void handle_uart_switch(bool enable);
void handle_aux_mux_switch(int polarity);
void handle_dp_connection(int status);
enum polarity {
	UNFLIPPED,
	FLIPPED,
};
#endif

void stop_usb_host(struct hw_pd_dev *dev);
void start_usb_host(struct hw_pd_dev *dev);
void stop_usb_peripheral(struct hw_pd_dev *dev);
void start_usb_peripheral(struct hw_pd_dev *dev);

int set_mode(struct hw_pd_dev *dev, int mode);
int set_pr(struct hw_pd_dev *dev, int pr);
int set_dr(struct hw_pd_dev *dev, int dr);
#if defined(CONFIG_MACH_SDM450_CV7AS) && defined(CONFIG_LGE_USB_MOISTURE_DETECT)
bool support_MoistureDetect(void);
#endif

#endif /* __HW_PD_DEV_H__ */
