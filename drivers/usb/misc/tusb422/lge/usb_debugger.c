extern int msm_serial_set_uart_console(int enable);
extern int msm_serial_get_uart_console_status(void);

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

	if (cable_type == 56 || cable_type == 130 || cable_type == 910)
		return true;
	else
		return false;
}

static void usb_debugger_work(struct work_struct *w)
{
	struct hw_pd_dev *dev = container_of(w, struct hw_pd_dev,
					     usb_debugger_work);

	if (dev->is_debug_accessory) {
		if (dev->is_debug_accessory && lge_is_factory_USB_cable()) {
			DEBUG("%s: factory cable connected\n", __func__);
			return;
		}

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
