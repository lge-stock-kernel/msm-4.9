#include "hw_pd_dev.h"
#include "tcpm.h"
#include "usb_pd.h"
#include "usb_pd_policy_engine.h"

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if defined(CONFIG_MACH_SDM450_CV7AS)
#include <soc/qcom/lge/board_lge.h>
#endif
#include "charger.c"
#ifdef CONFIG_LGE_USB_DEBUGGER
#include "usb_debugger.c"
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#include "cc_protect.c"
#endif


#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
extern void tusb422_set_dp_notify_node(int val);
#endif

static struct hw_pd_dev _hw_pd_dev;

static void notify_pd_contract_status(struct hw_pd_dev *dev)
{
	int rc = 0;
	union extcon_property_value val = {0};

	if (!dev)
		return;

	INFO("%s: pd_contract(%d)\n", __func__, dev->in_explicit_contract);

	rc = extcon_get_property(dev->extcon, EXTCON_USB,
				 EXTCON_PROP_USB_PD_CONTRACT, &val);
	if (rc)
		return;

	if (val.intval == dev->in_explicit_contract)
		return;

	val.intval = dev->in_explicit_contract;
	extcon_set_property(dev->extcon, EXTCON_USB,
			    EXTCON_PROP_USB_PD_CONTRACT, val);
	rc = extcon_blocking_sync(dev->extcon, EXTCON_USB, 0);
	if (rc)
		PRINT("err(%d) while notifying pd status", rc);
}

void stop_usb_host(struct hw_pd_dev *dev)
{
	INFO("%s\n", __func__);

	if (!extcon_get_state(dev->extcon, EXTCON_USB_HOST))
		return;

	extcon_set_state_sync(dev->extcon, EXTCON_USB_HOST, 0);
}

void start_usb_host(struct hw_pd_dev *dev)
{
	INFO("%s\n", __func__);

	if (extcon_get_state(dev->extcon, EXTCON_USB_HOST))
		return;

	extcon_set_state_sync(dev->extcon, EXTCON_USB_HOST, 1);
}

void stop_usb_peripheral(struct hw_pd_dev *dev)
{
	INFO("%s\n", __func__);

	if (!extcon_get_state(dev->extcon, EXTCON_USB))
		return;

	extcon_set_state_sync(dev->extcon, EXTCON_USB, 0);
}

void start_usb_peripheral(struct hw_pd_dev *dev)
{
	INFO("%s\n", __func__);

	if (extcon_get_state(dev->extcon, EXTCON_USB))
		return;

	extcon_set_state_sync(dev->extcon, EXTCON_USB, 1);
}

static int enable_vbus(struct hw_pd_dev *dev)
{
	int rc;

	if (!dev->vbus_reg) {
		dev->vbus_reg = devm_regulator_get(dev->dev, "vbus");
		if (IS_ERR(dev->vbus_reg)) {
			PRINT("vbus regulator doesn't preapred\n");
			dev->vbus_reg = NULL;
			return -EAGAIN;
		}
	}

	rc = regulator_enable(dev->vbus_reg);
	if (rc) {
		PRINT("unable to enable vbus\n");
		return rc;
	}

	dev->is_otg = true;
	INFO("enable vbus\n");

	return 0;
}

#if defined(CONFIG_LGE_USB_DEBUGGER) || defined(CONFIG_LGE_USB_MOISTURE_DETECT)
void handle_sbu_switch(bool enable, bool moisture_on)
{
	int rc;
	if(enable) {
		/* SBU_EN low, SBU_SEL high */
		if(_hw_pd_dev.sbu_en_gpio) {
			gpiod_direction_output(_hw_pd_dev.sbu_en_gpio,0);
			usleep_range(40000, 41000); /* Turn-On Time /OE */
		}
		if(_hw_pd_dev.sbu_sel_gpio) {
			gpiod_direction_output(_hw_pd_dev.sbu_sel_gpio, 1);
			usleep_range(40000, 41000);
		}
	} else {
		/* SBU_EN high */
		if(moisture_on) {
			/* SBU_EN low, SBU_SEL high */
			if(_hw_pd_dev.sbu_en_gpio) {
				gpiod_direction_output(_hw_pd_dev.sbu_en_gpio,0);
				usleep_range(40000, 41000); /* Turn-On Time /OE */
			}
			if(_hw_pd_dev.sbu_sel_gpio) {
				rc  = gpiod_direction_output(_hw_pd_dev.sbu_sel_gpio, 0);
				PRINT("%s: _hw_pd_dev.sbu_sel_gpio set 0 return %d \n", __func__, rc);
				usleep_range(40000, 41000);

			}
			if(_hw_pd_dev.edge_sel_gpio) {

				rc = gpiod_direction_output(_hw_pd_dev.edge_sel_gpio, 1);

				PRINT("%s: _hw_pd_dev.edge_sel_gpio set 1 return %d \n", __func__, rc);
				usleep_range(40000, 41000);
			}

		}
		else {
			if(_hw_pd_dev.sbu_en_gpio) {
				gpiod_direction_output(_hw_pd_dev.sbu_en_gpio,1);
				usleep_range(4000, 4100); /* Turn-Off Time /OE */
			}
		}

	}
}
EXPORT_SYMBOL(handle_sbu_switch);
#endif

int set_mode(struct hw_pd_dev *dev, int mode)
{
	static const char *const strings[] = {
		[DUAL_ROLE_PROP_MODE_UFP]	= "UFP",
		[DUAL_ROLE_PROP_MODE_DFP]	= "DFP",
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		[DUAL_ROLE_PROP_MODE_FAULT]	= "FAULT",
		[DUAL_ROLE_PROP_MODE_FAULT_NO_UX] = "FAULT No UX",
#endif
		[DUAL_ROLE_PROP_MODE_NONE]	= "None",
	};

	if (dev->mode == mode)
		return 0;

	switch (mode) {
	case DUAL_ROLE_PROP_MODE_UFP:
	case DUAL_ROLE_PROP_MODE_DFP:
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	case DUAL_ROLE_PROP_MODE_FAULT:
	case DUAL_ROLE_PROP_MODE_FAULT_NO_UX:
#endif
		break;
	case DUAL_ROLE_PROP_MODE_NONE:
#ifdef CONFIG_LGE_DP_UNSUPPORT_NOTIFY
		tusb422_set_dp_notify_node(0);
#endif
		break;

	default:
		PRINT("%s: unknown mode %d\n", __func__, mode);
		return -1;
	}

	dev->mode = mode;

	PRINT("%s(%s)\n", __func__, strings[mode]);
	return 0;

}

int set_pr(struct hw_pd_dev *dev, int pr)
{
	static const char *const strings[] = {
		[DUAL_ROLE_PROP_PR_SRC]		= "Source",
		[DUAL_ROLE_PROP_PR_SNK]		= "Sink",
		[DUAL_ROLE_PROP_PR_NONE]	= "None",
	};
	int rc;

	if (dev->pr == pr)
		return 0;

	switch (pr) {
	case DUAL_ROLE_PROP_PR_SRC:
		enable_vbus(dev);
		break;

	case DUAL_ROLE_PROP_PR_SNK:
	case DUAL_ROLE_PROP_PR_NONE:
		if (dev->is_otg) {
			dev->is_otg = false;
			rc = regulator_disable(dev->vbus_reg);
			if (rc)
				PRINT("unable to disable vbus\n");
			else
				INFO("disable vbus\n");
		}
		break;

	default:
		PRINT("%s: unknown pr %d\n", __func__, pr);
		return -1;
	}

	dev->pr = pr;

	PRINT("%s(%s)\n", __func__, strings[pr]);
	return 0;

}

int set_dr(struct hw_pd_dev *dev, int dr)
{
	static const char *const strings[] = {
		[DUAL_ROLE_PROP_DR_HOST]	= "Host",
		[DUAL_ROLE_PROP_DR_DEVICE]	= "Device",
		[DUAL_ROLE_PROP_DR_NONE]	= "None",
	};

	if (dev->dr == dr)
		return 0;

	switch (dr) {
	case DUAL_ROLE_PROP_DR_HOST:
		set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
		start_usb_host(dev);
		break;

	case DUAL_ROLE_PROP_DR_DEVICE:
		set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
		if (dev->in_explicit_contract)
			start_usb_peripheral(dev);
		break;

	case DUAL_ROLE_PROP_DR_NONE:
		if (dev->dr == DUAL_ROLE_PROP_DR_HOST)
			stop_usb_host(dev);
		if (dev->dr == DUAL_ROLE_PROP_DR_DEVICE)
			stop_usb_peripheral(dev);
		break;

	default:
		PRINT("%s: unknown dr %d\n", __func__, dr);
		return -1;
	}

	dev->dr = dr;

	PRINT("%s(%s)\n", __func__, strings[dr]);
	return 0;
}

static const char *event_to_string(enum pd_dpm_pe_evt event)
{
	static const char *const names[] = {
		[PD_DPM_PE_EVT_SOURCE_VBUS]	= "Source VBUS",
		[PD_DPM_PE_EVT_DIS_VBUS_CTRL]	= "Disable VBUS",
		[PD_DPM_PE_EVT_SINK_VBUS]	= "Sink VBUS",
		[PD_DPM_PE_EVT_PD_STATE]	= "PD State",
		[PD_DPM_PE_EVT_TYPEC_STATE]	= "TypeC State",
		[PD_DPM_PE_EVT_DR_SWAP]		= "DataRole Swap",
		[PD_DPM_PE_EVT_PR_SWAP]		= "PowerRole Swap",
#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
		[PD_DPM_PE_EVT_DEBUG_ACCESSORY]	= "Debug Accessory",
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		[PD_DPM_PE_EVENT_GET_SBU_ADC]	= "Get SBU ADC",
		[PD_DPM_PE_EVENT_SET_MOISTURE_DETECT_USE_SBU] = "Set Moisture Detect Use SBU",
#endif
	};

	if (event < 0 || event >= ARRAY_SIZE(names))
		return "Undefined";

	return names[event];
}
EXPORT_SYMBOL(event_to_string);

int pd_dpm_handle_pe_event(enum pd_dpm_pe_evt event, void *state)
{
	struct hw_pd_dev *dev = &_hw_pd_dev;
	union power_supply_propval prop;

	CRIT("%s: event: %s\n", __func__, event_to_string(event));

	switch (event) {
	case PD_DPM_PE_EVT_SOURCE_VBUS:
		set_pr(dev, DUAL_ROLE_PROP_PR_SRC);
		break;

	case PD_DPM_PE_EVT_DIS_VBUS_CTRL:
		if (dev->mode != DUAL_ROLE_PROP_MODE_NONE) {
			set_pr(dev, DUAL_ROLE_PROP_PR_SNK);
		} else {
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
		}

		dev->curr_max = 0;

		prop.intval = 0;
		power_supply_set_property(dev->usb_psy,
				POWER_SUPPLY_PROP_PD_CURRENT_MAX,
				&prop);

		break;

	case PD_DPM_PE_EVT_SINK_VBUS:
	{
		struct pd_dpm_vbus_state *vbus_state =
			(struct pd_dpm_vbus_state *)state;

		DEBUG("vbus_type(%d), mv(%d), ma(%d)\n",
		      vbus_state->vbus_type, vbus_state->mv, vbus_state->ma);

		set_pr(dev, DUAL_ROLE_PROP_PR_SNK);

		if (dev->volt_max == vbus_state->mv &&
		    dev->curr_max == vbus_state->ma)
			goto print_vbus_state;

		dev->volt_max = vbus_state->mv;
		dev->curr_max = vbus_state->ma;

		if (vbus_state->vbus_type) {
			// PD
#if defined(CONFIG_MACH_SDM450_CV7AS)
			if (get_pd_disable_high_voltage()) {
				PRINT("pd_disable_high_voltage\n");
				goto print_vbus_state;
			}
#endif

			if (!dev->in_explicit_contract) {
				prop.intval = POWER_SUPPLY_PD_ACTIVE;
				power_supply_set_property(dev->usb_psy,
						POWER_SUPPLY_PROP_PD_ACTIVE,
						&prop);

				dev->in_explicit_contract = true;
			}

			prop.intval = dev->volt_max * 1000;
			power_supply_set_property(dev->usb_psy,
					POWER_SUPPLY_PROP_PD_VOLTAGE_MAX,
					&prop);

			prop.intval = dev->curr_max * 1000;
			power_supply_set_property(dev->usb_psy,
					POWER_SUPPLY_PROP_PD_CURRENT_MAX,
					&prop);
		} else {
			if (dev->in_explicit_contract) {
				prop.intval = POWER_SUPPLY_PD_INACTIVE;
				power_supply_set_property(dev->usb_psy,
						POWER_SUPPLY_PROP_PD_ACTIVE,
						&prop);

				dev->in_explicit_contract = false;
			}
		}

print_vbus_state:
		PRINT("%s: %s%dmV, %dmA\n", __func__,
		      (vbus_state->vbus_type ? "PD " : ""),
		      vbus_state->mv, vbus_state->ma);
		break;
	}

	case PD_DPM_PE_EVT_PD_STATE:
	{
		struct pd_dpm_pd_state *pd_state =
			(struct pd_dpm_pd_state *)state;

		PRINT("connected(%d), usb_comm(%d)\n",
		      pd_state->connected, pd_state->usb_comm);

		switch (pd_state->connected) {
		case PD_CONNECT_PE_READY_SRC:
			if (dev->dr == DUAL_ROLE_PROP_DR_HOST &&
			    !pd_state->usb_comm) {
				stop_usb_host(dev);
			}
			break;

		case PD_CONNECT_PE_READY_SNK:
			if (dev->dr == DUAL_ROLE_PROP_DR_DEVICE &&
			    pd_state->usb_comm) {
				start_usb_peripheral(dev);
			}
			break;

		default:
			PRINT("unknown pd_state(%d)\n", pd_state->connected);
			break;
		}

		if (!dev->in_explicit_contract) {
			prop.intval = POWER_SUPPLY_PD_ACTIVE;
			power_supply_set_property(dev->usb_psy,
						  POWER_SUPPLY_PROP_PD_ACTIVE,
						  &prop);
			dev->in_explicit_contract = true;
		}
		notify_pd_contract_status(dev);

		break;
	}

	case PD_DPM_PE_EVT_TYPEC_STATE:
	{
		struct pd_dpm_typec_state *tc_state =
			(struct pd_dpm_typec_state *)state;

		DEBUG("polarity(%d), new_state(%d), typec_mode(%d)\n",
		      tc_state->polarity, tc_state->new_state, tc_state->typec_mode);

		prop.intval = tc_state->typec_mode;
		power_supply_set_property(dev->usb_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &prop);

		/* new_state */
		switch (tc_state->new_state) {
		case PD_DPM_TYPEC_UNATTACHED:
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
			if (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) {
				prop.intval = 0;
				power_supply_set_property(dev->usb_psy,
						POWER_SUPPLY_PROP_MOISTURE_DETECTED,
						&prop);
			}

			if (dev->is_sbu_ov) {
				enable_irq(dev->cc_protect_irq);
				dev->is_sbu_ov = false;
			}
#endif

			dev->in_explicit_contract = false;
			notify_pd_contract_status(dev);

			prop.intval = POWER_SUPPLY_PD_INACTIVE;
			power_supply_set_property(dev->usb_psy,
					POWER_SUPPLY_PROP_PD_ACTIVE, &prop);

			set_mode(dev, DUAL_ROLE_PROP_MODE_NONE);
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
			set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
			break;

		case PD_DPM_TYPEC_ATTACHED_SRC:
			if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
				set_mode(dev, DUAL_ROLE_PROP_MODE_DFP);
				set_pr(dev, DUAL_ROLE_PROP_PR_SRC);
				set_dr(dev, DUAL_ROLE_PROP_DR_HOST);
			}
			break;

		case PD_DPM_TYPEC_ATTACHED_SNK:
			if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
				set_mode(dev, DUAL_ROLE_PROP_MODE_UFP);
				set_pr(dev, DUAL_ROLE_PROP_PR_SNK);
				set_dr(dev, DUAL_ROLE_PROP_DR_DEVICE);
			}
			break;

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		case PD_DPM_TYPEC_CC_FAULT:
			PRINT("%s: PD_DPM_TYPEC_CC_FAULT!!!\n", __func__);
			set_mode(dev, DUAL_ROLE_PROP_MODE_FAULT);
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
			set_dr(dev, DUAL_ROLE_PROP_DR_NONE);

			dev->curr_max = 0;
			dev->volt_max = 0;

			prop.intval = 1;
			power_supply_set_property(dev->usb_psy,
					POWER_SUPPLY_PROP_MOISTURE_DETECTED,
					&prop);
                        power_supply_changed(dev->usb_psy);

			break;
#endif
		}
		break;
	}

	case PD_DPM_PE_EVT_DR_SWAP:
	{
		struct pd_dpm_swap_state *swap_state =
			(struct pd_dpm_swap_state *)state;

		switch (swap_state->new_role) {
		case PD_DATA_ROLE_UFP:
			set_dr(dev, DUAL_ROLE_PROP_DR_DEVICE);
			break;

		case PD_DATA_ROLE_DFP:
			set_dr(dev, DUAL_ROLE_PROP_DR_HOST);
			break;
		}
		break;
	}

	case PD_DPM_PE_EVT_PR_SWAP:
		break;

#if defined(CONFIG_LGE_USB_FACTORY) || defined(CONFIG_LGE_USB_DEBUGGER)
	case PD_DPM_PE_EVT_DEBUG_ACCESSORY:
	{
		bool is_debug_accessory = *(bool *)state;

		if (dev->is_debug_accessory == is_debug_accessory)
			break;

		dev->is_debug_accessory = is_debug_accessory;

#ifdef CONFIG_LGE_USB_DEBUGGER
		schedule_work(&dev->usb_debugger_work);
#endif
		break;
	}
#endif

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	case PD_DPM_PE_EVENT_GET_SBU_ADC:
	{
		int sbu_num = *(int *)state;

		return chg_get_sbu_adc(dev, sbu_num);
	}

	case PD_DPM_PE_EVENT_SET_MOISTURE_DETECT_USE_SBU:
		if (!(dev->moisture_detect_use_sbu && IS_CHARGERLOGO))
			break;

		if (dev->is_present) {
			int sbu1_adc = chg_get_sbu_adc(dev, 1);
			int sbu2_adc = chg_get_sbu_adc(dev, 2);

			if (sbu1_adc > SBU_VBUS_SHORT_THRESHOLD ||
			    sbu2_adc > SBU_VBUS_SHORT_THRESHOLD) {
				PRINT("%s: VBUS/SBU SHORT!!! SBU1=%d SBU2=%d\n",
				      __func__, sbu1_adc, sbu2_adc);
 			    //if (dev->moisture_detect_ux)
				{
 				    tcpm_cc_fault_set(0, TCPC_STATE_CC_FAULT_SBU_ADC);
 				    tcpm_cc_fault_timer(0, false);
 			    }
			}
		}

		enable_irq(dev->cc_protect_irq);
		break;
#endif

	default:
		PRINT("%s: Unknown event: %d\n", __func__, event);
		return -EINVAL;
	}

	return 0;
}

#if defined(CONFIG_MACH_SDM450_CV7AS) && defined(CONFIG_LGE_USB_MOISTURE_DETECT)
bool support_MoistureDetect(void)
{
	enum lge_laop_operator_type lge_laop_operator;
	lge_laop_operator = lge_get_laop_operator();

	 PRINT(" %s lge_laop_operator = %d \n",__func__, lge_laop_operator);
     // CV7AS AT&T support moisture detect
    	return (lge_laop_operator == OP_ATT_US)?  true : false;
}

#endif


int hw_pd_dev_init(struct device *dev)
{
	int rc = 0;

	_hw_pd_dev.dev = dev;
	_hw_pd_dev.mode = DUAL_ROLE_PROP_MODE_NONE;
	_hw_pd_dev.pr = DUAL_ROLE_PROP_PR_NONE;
	_hw_pd_dev.dr = DUAL_ROLE_PROP_DR_NONE;
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#ifdef CONFIG_LGE_USB_FACTORY
	if (!IS_FACTORY_MODE)
#endif
#if defined(CONFIG_MACH_SDM450_CV7AS)
    if(support_MoistureDetect()) // CV7A Canada support moisture detect
#endif
	_hw_pd_dev.moisture_detect_use_sbu = true;
	PRINT("%s : moisture_detect_use_sbu %d   \n", __func__,
	      _hw_pd_dev.moisture_detect_use_sbu);
#ifdef CONFIG_LGE_USB_MOISTURE_DETECTION_NO_UX
	_hw_pd_dev.moisture_detect_ux = false;
#else
	_hw_pd_dev.moisture_detect_ux = true;
#endif
#endif /* CONFIG_LGE_USB_MOISTURE_DETECT */

	dev_set_drvdata(dev, &_hw_pd_dev);

	rc = charger_init(&_hw_pd_dev);
	if (rc)
		return rc;

	if (!of_property_read_bool(dev->of_node, "extcon")) {
		PRINT("%s: extcon is not defined.\n", __func__);
		return -ENODEV;
	}

	_hw_pd_dev.extcon = extcon_get_edev_by_phandle(dev, 0);
	if (IS_ERR(_hw_pd_dev.extcon) && PTR_ERR(_hw_pd_dev.extcon) != -ENODEV)
		return PTR_ERR(_hw_pd_dev.extcon);

	extcon_set_property_capability(_hw_pd_dev.extcon, EXTCON_USB,
			EXTCON_PROP_USB_PD_CONTRACT);

#if defined(CONFIG_LGE_USB_DEBUGGER) || defined(CONFIG_LGE_USB_MOISTURE_DETECT)
	_hw_pd_dev.sbu_sel_gpio = devm_gpiod_get(dev, "ti,sbu-sel",
						 GPIOD_OUT_LOW);
	if (IS_ERR( _hw_pd_dev.sbu_sel_gpio)) {
		PRINT("failed to allocate sbu_sel gpio\n");
		_hw_pd_dev.sbu_sel_gpio = NULL;
	}

	_hw_pd_dev.sbu_en_gpio = devm_gpiod_get(dev, "ti,sbu-en",
						GPIOD_OUT_HIGH);
	if (IS_ERR( _hw_pd_dev.sbu_en_gpio)) {
		PRINT("failed to allocate sbu_en gpio\n");
		_hw_pd_dev.sbu_en_gpio = NULL;
	}

	_hw_pd_dev.edge_sel_gpio = devm_gpiod_get(dev, "ti,edge-sel",
						  GPIOD_OUT_LOW);
	if (IS_ERR( _hw_pd_dev.edge_sel_gpio )) {
		PRINT("failed to allocate edge-sel gpio\n");
		_hw_pd_dev.edge_sel_gpio = NULL;
	}
#endif

#ifdef CONFIG_LGE_USB_DEBUGGER
	usb_debugger_init(&_hw_pd_dev);
#endif
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	cc_protect_init(&_hw_pd_dev);
#endif
	return 0;
}
