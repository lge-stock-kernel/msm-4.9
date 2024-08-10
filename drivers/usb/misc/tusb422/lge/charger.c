#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
static int chg_get_sbu_adc(struct hw_pd_dev *dev, int num)
{
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
}

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

static void start_usb_peripheral_work(struct work_struct *w)
{
	struct hw_pd_dev *dev = container_of(w, struct hw_pd_dev,
					     start_periph_work);
	start_usb_peripheral(dev);
}
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
static void start_usb_moisture_detect_work(struct work_struct *w)
{
	struct hw_pd_dev *dev = container_of(w, struct hw_pd_dev,
					     start_moisture_detect_work);

	if (dev->mode == DUAL_ROLE_PROP_MODE_FAULT) {
		PRINT("present:%d in fault mode\n", dev->is_present);

		tcpm_cc_fault_timer(0, dev->is_present ? false : true);
		return;
	}

	if (IS_CHARGERLOGO && dev->is_present) {
		int sbu1_adc = chg_get_sbu_adc(dev, 1);
		int sbu2_adc = chg_get_sbu_adc(dev, 2);
		if (sbu1_adc > SBU_VBUS_SHORT_THRESHOLD ||
			sbu2_adc > SBU_VBUS_SHORT_THRESHOLD) {
			PRINT("%s: VBUS/SBU SHORT!!! SBU1=%d SBU2=%d is_chargerlogo=%d\n",
				  __func__, sbu1_adc, sbu2_adc, lge_get_boot_mode());
			//if (dev->moisture_detect_ux)
			{
			    tcpm_cc_fault_set(0, TCPC_STATE_CC_FAULT_SBU_ADC);
			    tcpm_cc_fault_timer(0, false);
			    return;
			}
		}
	}
}
#endif
static int psy_changed(struct notifier_block *nb, unsigned long evt, void *ptr)
{
	struct hw_pd_dev *dev = container_of(nb, struct hw_pd_dev, psy_nb);
	union power_supply_propval val;
	tcpc_device_t *typec_dev = tcpm_get_device(0);
	int rc;

	if (ptr != dev->usb_psy || evt != PSY_EVENT_PROP_CHANGED)
		return 0;

	rc = power_supply_get_property(dev->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (rc) {
		PRINT("Unable to read USB PRESENT: %d\n", rc);
		return rc;
	}

	dev->is_present = val.intval;

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	if(dev->moisture_detect_use_sbu)
		if((dev->mode == DUAL_ROLE_PROP_MODE_FAULT) || (IS_CHARGERLOGO && dev->is_present))
		{
			PRINT("typec present:%d dev->mode:%d\n", dev->is_present, dev->mode);
			schedule_work(&dev->start_moisture_detect_work);

			if (dev->moisture_detect_ux)
			return 0;

		}
#endif

	rc = power_supply_get_property(dev->usb_psy,
			POWER_SUPPLY_PROP_REAL_TYPE, &val);
	if (rc) {
		PRINT("Unable to read USB TYPE: %d\n", rc);
		return rc;
	}

	dev->psy_type = val.intval;

	if ((dev->dr == DUAL_ROLE_PROP_DR_DEVICE) || (dev->is_present))
		if (dev->psy_type == POWER_SUPPLY_TYPE_USB ||
		    dev->psy_type == POWER_SUPPLY_TYPE_USB_CDP ||
		    dev->psy_type == POWER_SUPPLY_TYPE_USB_FLOAT ||
		    (typec_dev &&
		     typec_dev->usb_compliance_mode &&
		     *typec_dev->usb_compliance_mode)) {
			schedule_work(&dev->start_periph_work);
	}

	PRINT("typec present:%d type:%d\n", dev->is_present, dev->psy_type);

	return 0;
}

int charger_init(struct hw_pd_dev *dev)
{
	int rc;

	dev->usb_psy = power_supply_get_by_name("usb");
	if (!dev->usb_psy) {
		PRINT("usb power_supply_get failed\n");
		return -EPROBE_DEFER;
	}

	dev->psy_nb.notifier_call = psy_changed;
	rc = power_supply_reg_notifier(&dev->psy_nb);
	if (rc) {
		PRINT("power_supply_reg_notifier failed\n");
		power_supply_put(dev->usb_psy);
		return rc;
	}

	INIT_WORK(&dev->start_periph_work, start_usb_peripheral_work);
#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	if(dev->moisture_detect_use_sbu)
		INIT_WORK(&dev->start_moisture_detect_work, start_usb_moisture_detect_work);
#endif

	/* force read initial power_supply values */
	psy_changed(&dev->psy_nb, PSY_EVENT_PROP_CHANGED, dev->usb_psy);

#ifdef CONFIG_LGE_USB_MOISTURE_DETECT
	if(dev->moisture_detect_use_sbu){
	    INIT_DELAYED_WORK(&dev->init_sbu_adc_work, init_sbu_adc_work);
	    schedule_delayed_work(&dev->init_sbu_adc_work, 0);
	}
#endif

	return 0;
}
