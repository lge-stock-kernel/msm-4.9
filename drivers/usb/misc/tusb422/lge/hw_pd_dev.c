#include "hw_pd_dev.h"
#include "tcpm.h"
#include "usb_pd.h"
#include "usb_pd_policy_engine.h"

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if defined(CONFIG_MACH_SDM450_CV7A_LAO_COM)
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
					usleep_range(400000, 410000);
					
				}
				if(_hw_pd_dev.edge_sel_gpio) {
					
					rc = gpiod_direction_output(_hw_pd_dev.edge_sel_gpio, 1);
					
					PRINT("%s: _hw_pd_dev.edge_sel_gpio set 1 return %d \n", __func__, rc);
					usleep_range(400000, 410000);
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
	union power_supply_propval val = {0};

	if (dev->pr == pr)
		return 0;

	switch (pr) {
	case DUAL_ROLE_PROP_PR_SRC:
		val.intval = 1;
		power_supply_set_property(dev->chg_psy, POWER_SUPPLY_PROP_USB_OTG, &val);
		break;

	case DUAL_ROLE_PROP_PR_SNK:
	case DUAL_ROLE_PROP_PR_NONE:
		val.intval = 0;
		power_supply_set_property(dev->chg_psy, POWER_SUPPLY_PROP_USB_OTG, &val);
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

	union power_supply_propval val = {0};
	

	if (dev->dr == dr)
		return 0;

	switch (dr) {
	case DUAL_ROLE_PROP_DR_HOST:
		set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
		val.intval = 1;
		power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_USB_OTG, &val);
		break;

	case DUAL_ROLE_PROP_DR_DEVICE:
		set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
		val.intval = 1;
		power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_PRESENT, &val);
		break;

	case DUAL_ROLE_PROP_DR_NONE:
		val.intval = 0;
		if (dev->dr == DUAL_ROLE_PROP_DR_HOST)
			power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_USB_OTG, &val);
		if (dev->dr == DUAL_ROLE_PROP_DR_DEVICE)
			power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_PRESENT, &val);
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
		dev->chg_psy_desc.type = POWER_SUPPLY_TYPE_DFP;
		break;

	case PD_DPM_PE_EVT_DIS_VBUS_CTRL:
		if (dev->mode != DUAL_ROLE_PROP_MODE_NONE) {
			set_pr(dev, DUAL_ROLE_PROP_PR_SNK);
		} else {
			set_pr(dev, DUAL_ROLE_PROP_PR_NONE);
		}

		dev->chg_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		dev->curr_max = 0;
		dev->volt_max = 0;

		if (dev->rp) {
			dev->rp = 0;
			prop.intval = 0;
			power_supply_set_property(dev->usb_psy,
						POWER_SUPPLY_PROP_CTYPE_RP,
						&prop);
		}
#if defined (CONFIG_LGE_PM_VENEER_PSY)
		prop.intval = POWER_SUPPLY_PD_INACTIVE;
		power_supply_set_property(dev->usb_psy,POWER_SUPPLY_PROP_PD_ACTIVE, &prop);
#endif
		break;

	case PD_DPM_PE_EVT_SINK_VBUS:
	{
		struct pd_dpm_vbus_state *vbus_state =
			(struct pd_dpm_vbus_state *)state;

		DEBUG("vbus_type(%d), mv(%d), ma(%d)\n",
		      vbus_state->vbus_type, vbus_state->mv, vbus_state->ma);

		set_pr(dev, DUAL_ROLE_PROP_PR_SNK);

		if (vbus_state->vbus_type) {
			if (dev->chg_psy_desc.type == POWER_SUPPLY_TYPE_CTYPE_PD &&
			    dev->volt_max == vbus_state->mv &&
			    dev->curr_max == vbus_state->ma)
				goto print_vbus_state;

			dev->chg_psy_desc.type = POWER_SUPPLY_TYPE_CTYPE_PD;
			dev->volt_max = vbus_state->mv;
			dev->curr_max = vbus_state->ma;



#if defined (CONFIG_LGE_PM_VENEER_PSY)
			if(dev->curr_max > 500
#ifdef CONFIG_LGE_USB_TYPE_C
#if defined(CONFIG_MACH_SDM450_CV7AS)
             && !get_pd_disable_high_voltage()
#endif
#endif
             )
			{
                prop.intval = POWER_SUPPLY_PD_ACTIVE;
        		power_supply_set_property(dev->usb_psy,POWER_SUPPLY_PROP_PD_ACTIVE, &prop);

			prop.intval = (dev->volt_max*1000);
			power_supply_set_property(dev->usb_psy,POWER_SUPPLY_PROP_PD_VOLTAGE_MAX, &prop);

			prop.intval = (dev->curr_max > 500) ? (500 * 1000) : (dev->curr_max * 1000);

			PRINT("POWER_SUPPLY_PD_ACTIVE dev->curr_max %d. prop.intval %d not set current \n", dev->curr_max  , prop.intval);
			power_supply_set_property(dev->usb_psy,POWER_SUPPLY_PROP_PD_CURRENT_MAX, &prop);
			}
#else
			prop.intval = (dev->curr_max > 500) ? 500 : dev->curr_max;
            set_property_to_battery(dev,
			            POWER_SUPPLY_PROP_PD_CURRENT_MAX,
			            &prop);
#endif
		} else {
			uint16_t ma = vbus_state->ma;
			int rp = 0;

			power_supply_get_property(dev->usb_psy,POWER_SUPPLY_PROP_REAL_TYPE, &prop);
			dev->chg_psy_desc.type = prop.intval;

			if (dev->chg_psy_desc.type == POWER_SUPPLY_TYPE_USB_HVDCP ||
			    dev->chg_psy_desc.type == POWER_SUPPLY_TYPE_USB_HVDCP_3) {
				PRINT("HVDCP is present %d. ignore Rp advertisement\n", dev->chg_psy_desc.type);
#if !defined (CONFIG_LGE_PM_VENEER_PSY)
				if (dev->curr_max) {
					dev->curr_max = 0;
					dev->volt_max = 0;

					prop.intval = dev->curr_max;


				set_property_to_battery(dev,
							POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
							&prop);
				}
#endif

				break;
			}

			switch (ma) {
			case 3000: // Rp10K
				ma = 2000;
				rp = 10;
				break;
			case 1500: // Rp22K
				rp = 22;
				break;
			case 500:  // Rp56K
				ma = 0;
				rp = 56;
				break;
			default:
				ma = 0;
				rp = 0;
				break;
			}

			if (rp && !dev->rp) {
				dev->rp = rp;
				prop.intval = dev->rp;
				power_supply_set_property(dev->usb_psy,
							POWER_SUPPLY_PROP_CTYPE_RP,
							&prop);
			}

			if (dev->volt_max == vbus_state->mv &&
			    dev->curr_max == ma)
			{
				PRINT("dev->curr_max %d. dev->curr_max %d not set current \n", dev->curr_max  , ma);

				goto print_vbus_state;
                        }
			dev->volt_max = vbus_state->mv;
			dev->curr_max = ma;

#if defined (CONFIG_LGE_PM_VENEER_PSY)
       if(dev->curr_max)
       	{

			PRINT("## eunmi dev->curr_maxt %d.  set current\n", dev->curr_max);
		    prop.intval = 1000 * dev->curr_max;
			//		set_property_to_battery(dev,
			//		POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED,
			//		&prop);
			power_supply_set_property(dev->usb_psy,POWER_SUPPLY_PROP_CURRENT_MAX, &prop);
       	}

#else
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
						&prop);
#endif
		}

print_vbus_state:
		PRINT("%s: %s, %dmV, %dmA\n", __func__,
		      chg_to_string(vbus_state->vbus_type ?
				    POWER_SUPPLY_TYPE_CTYPE_PD :
				    POWER_SUPPLY_TYPE_CTYPE),
		      vbus_state->mv,
		      vbus_state->ma);
		break;
	}

	case PD_DPM_PE_EVT_PD_STATE:
	{
		struct pd_dpm_pd_state *pd_state =
			(struct pd_dpm_pd_state *)state;

		PRINT("connected(%d)\n", pd_state->connected);

		if (pd_state->connected == PD_CONNECT_PE_READY_SNK) {

#if defined (CONFIG_LGE_PM_VENEER_PSY)
			if(dev->curr_max > 500)
			{
			PRINT("PD_CONNECT_PE_READY_SNK dev->curr_max %d. set current \n", dev->curr_max);

			prop.intval = (dev->volt_max < 9000) ? (dev->curr_max * 1000):(1800 * 1000) ;
			power_supply_set_property(dev->usb_psy,POWER_SUPPLY_PROP_PD_CURRENT_MAX, &prop);
			}

#else
			set_property_to_battery(dev,
						POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
						&prop);
#endif
		}
		break;
	}

	case PD_DPM_PE_EVT_TYPEC_STATE:
	{
		struct pd_dpm_typec_state *tc_state =
			(struct pd_dpm_typec_state *)state;

		DEBUG("polarity(%d), new_state(%d)\n",
		      tc_state->polarity, tc_state->new_state);

		/* new_state */
		switch (tc_state->new_state) {
		case PD_DPM_TYPEC_UNATTACHED:
			dev->typec_mode = POWER_SUPPLY_TYPE_UNKNOWN;

			if (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) {
#if defined (CONFIG_LGE_PM_VENEER_PSY)   
				prop.intval = 0;
				power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_MOISTURE_DETECTED, &prop);

#endif
			}

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
			if (dev->is_sbu_ov) {
				enable_irq(dev->cc_protect_irq);
				dev->is_sbu_ov = false;
			}
#endif
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

			dev->chg_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			dev->curr_max = 0;
			dev->volt_max = 0;

			if (dev->rp) {
				dev->rp = 0;
				prop.intval = 0;
				power_supply_set_property(dev->usb_psy,
							POWER_SUPPLY_PROP_CTYPE_RP,
							&prop);
			}

#if defined (CONFIG_LGE_PM_VENEER_PSY) 
			prop.intval = 1;
			power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_MOISTURE_DETECTED, &prop);
#endif
			break;
#endif
		}
		break;
	}

	case PD_DPM_PE_EVT_DR_SWAP:
	{  
		struct pd_dpm_swap_state *swap_state =
			(struct pd_dpm_swap_state *)state;
		union power_supply_propval val = {0};

		switch (swap_state->new_role) {
		case PD_DATA_ROLE_UFP:

			val.intval = POWER_SUPPLY_TYPE_USB;
			power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_TYPE, &val);
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

#ifdef CONFIG_LGE_USB_FACTORY
		dev->typec_mode = is_debug_accessory ?
			POWER_SUPPLY_TYPE_CTYPE_DEBUG_ACCESSORY :
			POWER_SUPPLY_TYPE_UNKNOWN;
#endif

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

			if (sbu1_adc > SBU_VBUS_SHORT_THRESHOLD || sbu2_adc > SBU_VBUS_SHORT_THRESHOLD) {
				PRINT("%s: VBUS/SBU SHORT!!! SBU1 = %d  SBU2 = %d\n", __func__, sbu1_adc, sbu2_adc);
				tcpm_cc_fault_set(0, TCPC_STATE_CC_FAULT_SBU_ADC);
				tcpm_cc_fault_timer(0, false);
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

#if defined(CONFIG_MACH_SDM450_CV7A_LAO_COM) && defined(CONFIG_LGE_USB_MOISTURE_DETECT)
bool support_MoistureDetect(void)
{
    static int lge_gpio_value;
    lge_gpio_value = lge_get_gpio_value();

	 PRINT(" %s lge_gpio_value = %d \n",__func__, lge_gpio_value);
 // CV7A Canada support moisture detect
    	return (lge_gpio_value == 3)?  true : false;

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
#if defined(CONFIG_MACH_SDM450_CV7A_LAO_COM)
    if(support_MoistureDetect()) // CV7A Canada support moisture detect
#endif
	_hw_pd_dev.moisture_detect_use_sbu = true;

    PRINT("%s : moisture_detect_use_sbu %d   \n", __func__, _hw_pd_dev.moisture_detect_use_sbu);

#endif /* CONFIG_LGE_USB_MOISTURE_DETECT */

	dev_set_drvdata(dev, &_hw_pd_dev);

	rc = charger_init(&_hw_pd_dev);
	if (rc)
		return rc;

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
