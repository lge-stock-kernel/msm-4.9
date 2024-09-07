extern int msm_serial_set_uart_console(int enable);
extern int msm_serial_get_uart_console_status(void);

#ifdef CONFIG_LGE_PM_VENEER_PSY
static bool lge_is_factory_USB_cable(void)
{

	struct power_supply *psy;
	union power_supply_propval val = { .intval = 0 };
	int ret = 0;
	int cable_type = 0;

	psy = power_supply_get_by_name("usb");
	if (!psy) {
		pr_err("%s: usb psy doesn't prepared\n", __func__);
		return 0;
	}

	ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_RESISTANCE_ID, &val);
	if (ret) {
		pr_err("%s: Unable to read USB RESISTANCE ID: %d\n", __func__, ret);
		return ret;
	}
	cable_type = val.intval / 1000;
	
		if (cable_type == 56 ||
				cable_type == 130 ||
				cable_type == 910)
			return true;
		else
			return false;
}
#endif
static void usb_debugger_work(struct work_struct *w)
{
	struct hw_pd_dev *dev = container_of(w, struct hw_pd_dev,
					     usb_debugger_work);
#if defined (CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT)
	union lge_power_propval lge_val;
	int rc;

	if (!dev->lge_power_cd) {
		PRINT("%s: lge_power_cd is NULL\n", __func__);
		return;
	}
#endif

	if (dev->is_debug_accessory) {
#if defined (CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT)
		rc = dev->lge_power_cd->get_property(dev->lge_power_cd,
						     LGE_POWER_PROP_CHECK_ONLY_USB_ID,
						     &lge_val);
		if (rc != 0) {
			PRINT("%s: get_property CHECK_ONLY_USB_ID error %d\n",
			      __func__, rc);
			return;
		} else if (lge_val.intval == FACTORY_CABLE) {
			DEBUG("%s: factory cable connected\n", __func__);
			return;
		}
#elif defined (CONFIG_LGE_PM_FACTORY_CABLE)
				if (lge_is_factory_cable()) {
					DEBUG("%s: factory cable connected\n", __func__);
					return;
				}

#elif defined (CONFIG_LGE_PM_VENEER_PSY)
		
			if (dev->is_debug_accessory && lge_is_factory_USB_cable()) {

			DEBUG("%s: factory cable connected\n", __func__);
			return;
		}
#endif
		if(dev->sbu_en_gpio)
			gpiod_direction_output(dev->sbu_en_gpio,0);
		gpiod_direction_output(dev->sbu_sel_gpio, 0);

#ifdef CONFIG_SERIAL_MSM
#if defined(CONFIG_LGE_EARJACK_DEBUGGER) || defined(CONFIG_LGE_USB_DEBUGGER)
		//lge_uart_console_on_earjack_debugger_in();
		msm_serial_set_uart_console(1);
#endif
#endif
		DEBUG("%s: uart debugger in\n", __func__);
	} else {
		if(dev->sbu_en_gpio)
			gpiod_direction_output(dev->sbu_en_gpio, 1);
#ifdef CONFIG_SERIAL_MSM
#if defined(CONFIG_LGE_EARJACK_DEBUGGER) || defined(CONFIG_LGE_USB_DEBUGGER)
		//lge_uart_console_on_earjack_debugger_out();
		msm_serial_set_uart_console(0);
#endif
#endif
		DEBUG("%s: uart debugger out\n", __func__);
	}

	return;
}

static int usb_debugger_init(struct hw_pd_dev *dev)
{


	INIT_WORK(&dev->usb_debugger_work, usb_debugger_work);
#if defined (CONFIG_LGE_PM_LGE_POWER_CLASS_CABLE_DETECT)
	dev->lge_power_cd = lge_power_get_by_name("lge_cable_detect");
#endif
#if defined(CONFIG_LGE_EARJACK_DEBUGGER) || defined(CONFIG_LGE_USB_DEBUGGER)
   if (!dev->is_debug_accessory)
	{
#ifdef CONFIG_SERIAL_MSM
         //msm_serial_set_uart_console(0);
#endif
    }
#endif

	return 0;
}
