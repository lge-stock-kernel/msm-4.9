static char *chg_supplicants[] = {
	"battery",
};

static enum power_supply_property chg_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
	POWER_SUPPLY_PROP_MOISTURE_DETECTED,
};

static const char *chg_to_string(enum power_supply_type type)
{
	switch (type) {
	case POWER_SUPPLY_TYPE_CTYPE:
		return "USB Type-C Charger";
	case POWER_SUPPLY_TYPE_CTYPE_PD:
		return "USB Type-C PD Charger";
	default:
		return "Unknown Charger";
	}
}
#if 0
static void set_property_to_battery(struct hw_pd_dev *dev,
				    enum power_supply_property property,
				    union power_supply_propval *prop)
{
	int rc;

	if (!dev->batt_psy) {
		dev->batt_psy = power_supply_get_by_name("battery");
		if (!dev->batt_psy) {
			PRINT("battery psy doesn't preapred\n");
			dev->batt_psy = 0;
			return;
		}
	}

	rc = power_supply_set_property(dev->batt_psy, property, prop);
	if (rc < 0)
		PRINT("battery psy doesn't support reading prop %d rc = %d\n",
		      property, rc);
}
#endif
#define OTG_WORK_DELAY 1000
static void otg_work(struct work_struct *w)
{
	struct hw_pd_dev *dev = container_of(w, struct hw_pd_dev,
					     otg_work.work);
	struct device *cdev = dev->dev;
	union power_supply_propval prop;
	int rc;

	if (!dev->vbus_reg) {
		dev->vbus_reg = devm_regulator_get(cdev, "vbus");
		if (IS_ERR(dev->vbus_reg)) {
			PRINT("vbus regulator doesn't preapred\n");
			dev->vbus_reg = 0;
			schedule_delayed_work(&dev->otg_work,
					      msecs_to_jiffies(OTG_WORK_DELAY));
			return;
		}
	}

	if (dev->is_otg) {
		rc = regulator_enable(dev->vbus_reg);
		if (rc)
			PRINT("unable to enable vbus\n");

		prop.intval = POWER_SUPPLY_TYPE_DFP;
		power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_TYPEC_MODE, &prop);
		//set_property_to_battery(dev, POWER_SUPPLY_PROP_TYPEC_MODE,
		//		       &prop);
	} else {
		rc = regulator_disable(dev->vbus_reg);
		if (rc)
			PRINT("unable to disable vbus\n");

		prop.intval = POWER_SUPPLY_TYPE_UFP;
		power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_TYPEC_MODE, &prop);

		//set_property_to_battery(dev, POWER_SUPPLY_PROP_TYPEC_MODE,
		//		       &prop);
	}
}

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
static int chg_get_sbu_adc(struct hw_pd_dev *dev, int num)
{
#if defined (CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT)
	union lge_power_propval lge_val;
	int rc;

	if (!dev->lge_adc_lpc) {
		dev_err(dev->dev, "%s: lge_adc_lpc is NULL\n", __func__);
		return -ENODEV;
	}

	rc = dev->lge_adc_lpc->get_property(dev->lge_adc_lpc,
					    LGE_POWER_PROP_USB_ID_PHY,
					    &lge_val);
	if (rc) {
		dev_err(dev->dev, "failed to get sbu_adc %d\n", rc);
		return rc;
	}

	PRINT("SBU_ADC: %d\n", (int)lge_val.int64val);

	return (int)lge_val.int64val;
#elif defined(CONFIG_LGE_PM_FACTORY_CABLE)
	struct qpnp_vadc_result results;
	int rc;

	if(dev->adc_initialized) {
		rc = qpnp_vadc_read(dev->vadc, P_MUX1_1_1, &results);
		if (rc < 0) {
			dev_err(dev->dev, "failed to get sbu_adc %d\n", rc);
			return rc;
		}

		PRINT("SBU_ADC: %d\n", (int)results.physical);
		return (int)results.physical;
	} else {
		dev_err(dev->dev, "failed to set adc_initialized \n");
	}

	return 0;
#elif defined(CONFIG_LGE_PM_VENEER_PSY)
	struct qpnp_vadc_result results;
	int rc;

	if(dev->adc_initialized) {

     if(num == 1)  //SBU1_MOISTURE
     {
		rc =qpnp_vadc_read(dev->vadc,VADC_AMUX2_GPIO,&results);
		if (rc < 0) {
			dev_err(dev->dev, "failed to get sbu_adc %d\n", rc);
			return rc;
		}

		PRINT("SBU1_MOISTURE: %d\n", (int)results.physical);
     }
	 else if(num == 2) //SBU2_MOISTURE
	 {
		rc =qpnp_vadc_read(dev->vadc,VADC_AMUX1_GPIO,&results);
		if (rc < 0) {
			dev_err(dev->dev, "failed to get sbu_adc %d\n", rc);
			return rc;
		}

		PRINT("SBU2_MOISTURE: %d\n", (int)results.physical);
		
		rc =qpnp_vadc_read(dev->vadc,VADC_AMUX3_GPIO,&results);
		if (rc < 0) {
			dev_err(dev->dev, "failed to get sbu_adc %d\n", rc);
			return rc;
		}

		PRINT("USB_ID: %d\n", (int)results.physical);
	 }
	 else  //USB_ID
	 {
		rc =qpnp_vadc_read(dev->vadc,VADC_AMUX3_GPIO,&results);
		if (rc < 0) {
			dev_err(dev->dev, "failed to get sbu_adc %d\n", rc);
			return rc;
		}

		PRINT("USB_ID: %d\n", (int)results.physical);
	 }
	 
		return (int)results.physical;  // from REv 1.0 , check sbu1 and sbu2
	} else {
		dev_err(dev->dev, "failed to set adc_initialized \n");
	}

	return 0;
#else
	return -EINVAL;
#endif
}
#endif

static int chg_get_property(struct power_supply *psy,
			    enum power_supply_property prop,
			    union power_supply_propval *val)
{


	//struct hw_pd_dev *dev = container_of(psy_desc, struct hw_pd_dev, chg_psy_desc);
	struct hw_pd_dev *dev = power_supply_get_drvdata(psy);

	switch(prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		DEBUG("%s: is_otg(%d)\n", __func__,
			dev->is_otg);
		val->intval = dev->is_otg;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		DEBUG("%s: is_present(%d)\n", __func__,
			dev->is_present);
		val->intval = dev->is_present;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		DEBUG("%s: volt_max(%dmV)\n", __func__, dev->volt_max);
		val->intval = dev->volt_max;
		break;

	case POWER_SUPPLY_PROP_CURRENT_MAX:
		DEBUG("%s: curr_max(%dmA)\n", __func__, dev->curr_max);
		val->intval = dev->curr_max;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		DEBUG("%s: type(%s)\n", __func__,
			chg_to_string(dev->chg_psy_desc.type));
		val->intval = dev->chg_psy_desc.type;
		break;

	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		DEBUG("%s: current_capability(%dmA)\n", __func__,
		      dev->curr_max);
		val->intval = dev->curr_max;
		break;

	case POWER_SUPPLY_PROP_TYPEC_MODE:
		DEBUG("%s: typec_mode(%d)\n", __func__,
		      dev->typec_mode);
		val->intval = dev->typec_mode;
		break;
#if defined(CONFIG_LGE_USB_MOISTURE_DETECT) && defined(CONFIG_LGE_PM_VENEER_PSY)
	case POWER_SUPPLY_PROP_MOISTURE_DETECTED:
		dev->sbu_ov_cnt = 0;

		val->intval = (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) ? 1 : 0;
		DEBUG("%s: input_suspend(%d)\n", __func__, val->intval);
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int chg_set_property(struct power_supply *psy,
			    enum power_supply_property prop,
			    const union power_supply_propval *val)
{
	//struct hw_pd_dev *dev = container_of(psy_desc, struct hw_pd_dev, chg_psy_desc);
	struct hw_pd_dev *dev = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
		if (dev->is_otg == val->intval)
			break;
		dev->is_otg = val->intval;
		DEBUG("%s: is_otg(%d)\n", __func__, dev->is_otg);

		cancel_delayed_work(&dev->otg_work);
		otg_work(&dev->otg_work.work);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		if (dev->is_present == val->intval)
			break;
		dev->is_present = val->intval;
		DEBUG("%s: is_present(%d)\n", __func__, dev->is_present);

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
		if (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) {
			PRINT("%d %s present in fault mode \n",dev->is_present,__func__);
			power_supply_set_property(dev->usb_psy, POWER_SUPPLY_PROP_PRESENT, val);
			tcpm_cc_fault_timer(0, dev->is_present ? false : true);
			break;
		}
		 PRINT("##eunmi %s: check VBUS/SBU SHORT is_chargerlogo = %d\n", __func__, lge_get_boot_mode());

		if (dev->moisture_detect_use_sbu && IS_CHARGERLOGO && val->intval) {
			int sbu1_adc = chg_get_sbu_adc(dev, 1);
			int sbu2_adc = chg_get_sbu_adc(dev, 2);
			if (sbu1_adc > SBU_VBUS_SHORT_THRESHOLD || sbu2_adc > SBU_VBUS_SHORT_THRESHOLD) {
				PRINT("%s: VBUS/SBU SHORT!!! SBU1 = %d  SBU2 = %d is_chargerlogo = %d\n", __func__, sbu1_adc, sbu2_adc, lge_get_boot_mode());
				tcpm_cc_fault_set(0, TCPC_STATE_CC_FAULT_SBU_ADC);
				tcpm_cc_fault_timer(0, false);
				break;
			}
		}
#endif

		if (dev->mode == DUAL_ROLE_PROP_MODE_NONE) {
			if (val->intval) {
				PRINT("power on by charger\n");
				set_dr(dev, DUAL_ROLE_PROP_DR_DEVICE);
			} else {
				if (dev->dr == DUAL_ROLE_PROP_DR_DEVICE) {
					PRINT("power down by charger\n");
					set_dr(dev, DUAL_ROLE_PROP_DR_NONE);
				}
			}
		}

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int chg_is_writeable(struct power_supply *psy,
			    enum power_supply_property prop)
{
	int rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_USB_OTG:
	case POWER_SUPPLY_PROP_PRESENT:
		rc = 1;
		break;
	default:
		rc = 0;
		break;
	}

	return rc;
}

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#if defined(CONFIG_LGE_PM_VENEER_PSY) || defined(CONFIG_LGE_PM_FACTORY_CABLE)
static void init_sbu_adc_work(struct work_struct *w)
{
	struct hw_pd_dev *pd = container_of(w, struct hw_pd_dev, init_sbu_adc_work.work);

	if (IS_ERR_OR_NULL(pd->vadc)) {
		 pd->vadc = qpnp_get_vadc(pd->dev, "moisture-detection");
		 if (IS_ERR(pd->vadc)) {
			 if (PTR_ERR(pd->vadc) == -EPROBE_DEFER) {
				 schedule_delayed_work(&pd->init_sbu_adc_work,
						 msecs_to_jiffies(200));
				 goto out;
			 }
		 }
	}
	pd->adc_initialized = true;
	if(pd->moisture_detect_use_sbu && IS_CHARGERLOGO)
		pd_dpm_handle_pe_event(PD_DPM_PE_EVENT_SET_MOISTURE_DETECT_USE_SBU, NULL);
out:
	if(!pd->adc_initialized)
		PRINT("vadc get failed, not yet probed.\n");
}
#endif
#endif

int charger_init(struct hw_pd_dev *dev)
{
	struct power_supply_config usb_cfg = {0,};
	struct device *cdev = dev->dev;
	union power_supply_propval val = {0,};

	usb_cfg.drv_data = dev;
	usb_cfg.of_node = dev->dev->of_node;

	dev->usb_psy = power_supply_get_by_name("usb");
	if (!dev->usb_psy) {
		PRINT("usb power_supply_get failed\n");
		return -EPROBE_DEFER;
	}

	power_supply_get_property(dev->usb_psy, POWER_SUPPLY_PROP_PRESENT, &val);
	dev->is_present = val.intval;
	dev->chg_psy_desc.name = "usb_pd";
	dev->chg_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	dev->chg_psy_desc.get_property = chg_get_property;
	dev->chg_psy_desc.set_property = chg_set_property;
	dev->chg_psy_desc.property_is_writeable = chg_is_writeable;
	dev->chg_psy_desc.properties = chg_properties;
	dev->chg_psy_desc.num_properties = ARRAY_SIZE(chg_properties);
	usb_cfg.supplied_to = chg_supplicants;
	usb_cfg.num_supplicants = ARRAY_SIZE(chg_supplicants);
	dev->chg_psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN ;

	INIT_DELAYED_WORK(&dev->otg_work, otg_work);


	dev->chg_psy = power_supply_register(cdev, &dev->chg_psy_desc, &usb_cfg);
	if (IS_ERR(dev->chg_psy)) {
		PRINT("Unalbe to register ctype_psy \n");
		return PTR_ERR(dev->chg_psy);
	}

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
#if defined (CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT)
	dev->lge_adc_lpc = lge_power_get_by_name("lge_adc");
#elif defined(CONFIG_LGE_PM_VENEER_PSY) || defined(CONFIG_LGE_PM_FACTORY_CABLE)
	INIT_DELAYED_WORK(&dev->init_sbu_adc_work, init_sbu_adc_work);
	schedule_delayed_work(&dev->init_sbu_adc_work, 0);
#endif
#endif

	return 0;
}
