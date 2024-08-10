/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2000-2017 MELFAS Inc.
 *
 *
 * mip4_ts_mod.c : Model dependent functions
 *
 * Version : 2017.05.01
 */

#include "mip4_ts.h"

/*
* Pre-run config
*/
int mip4_ts_startup_config(struct mip4_ts_info *info)
{
	TOUCH_D(BASE_INFO, "[START]\n");

	if (info->disable_esd) {
		TOUCH_D(BASE_INFO, "disable_esd\n");
		mip4_ts_disable_esd_alert(info);
	}

	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	// ...

	//
	//////////////////////////

	TOUCH_D(BASE_INFO, "[DONE]\n");
	return 0;
}

/*
* Config regulator
*/
int mip4_ts_config_regulator(struct mip4_ts_info *info)
{
	int ret = 0;

#ifdef CONFIG_REGULATOR
	TOUCH_D(BASE_INFO, "[START]\n");

	info->regulator_vd33 = regulator_get(&info->client->dev, "vd33");

	if (IS_ERR_OR_NULL(info->regulator_vd33)) {
		TOUCH_E("[ERROR] regulator_get : vd33\n");
		ret = PTR_ERR(info->regulator_vd33);
	} else {
		TOUCH_E("- regulator_get : vd33\n");

		ret = regulator_set_voltage(info->regulator_vd33, 3300000, 3300000);
		if (ret) {
			TOUCH_E("[ERROR] regulator_set_voltage : vd33\n");
		} else {
			TOUCH_D(BASE_INFO, "regulator_set_voltage : 3300000\n");
		}
	}

	TOUCH_D(BASE_INFO, "[DONE]\n");
#endif

	return ret;
}

/*
* Control regulator
*/
int mip4_ts_control_regulator(struct mip4_ts_info *info, int enable)
{
#ifdef CONFIG_REGULATOR
	int ret = 0;

	TOUCH_D(BASE_INFO, "[START]\n");
	TOUCH_D(BASE_INFO, "switch : %d\n", enable);

	if (info->power == enable) {
		TOUCH_D(BASE_INFO, "skip\n");
		goto exit;
	}

	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	if (IS_ERR_OR_NULL(info->regulator_vd33)) {
		TOUCH_E("[ERROR] regulator_vd33 not found\n");
		goto exit;
	}

	if (enable) {
		ret = regulator_enable(info->regulator_vd33);
		if (ret) {
			TOUCH_E("[ERROR] regulator_enable : vd33\n");
			goto error;
		} else {
			TOUCH_D(BASE_INFO, "regulator_enable\n");
		}

#ifdef CONFIG_OF
		if (!IS_ERR_OR_NULL(info->pinctrl)) {
			ret = pinctrl_select_state(info->pinctrl, info->pins_enable);
			if (ret < 0) {
				TOUCH_E("[ERROR] pinctrl_select_state : pins_enable\n");
			}
		} else {
			TOUCH_E("[ERROR] pinctrl not found\n");
		}
#endif /* CONFIG_OF */
	} else {
		if (regulator_is_enabled(info->regulator_vd33)) {
			regulator_disable(info->regulator_vd33);
			TOUCH_D(BASE_INFO, "regulator_disable\n");
		}

#ifdef CONFIG_OF
		if (!IS_ERR_OR_NULL(info->pinctrl)) {
			ret = pinctrl_select_state(info->pinctrl, info->pins_disable);
			if (ret < 0) {
				TOUCH_E("[ERROR] pinctrl_select_state : pins_disable\n");
			}
		} else {
			TOUCH_E("[ERROR] pinctrl not found\n");
		}
#endif /* CONFIG_OF */
	}

	//
	//////////////////////////

	info->power = enable;

exit:
	TOUCH_D(BASE_INFO, "[DONE]\n");
	return 0;

error:
	TOUCH_E("[ERROR]\n");
	return ret;
#else
	return 0;
#endif /* CONFIG_REGULATOR */
}

/*
* Turn off power supply
*/
int mip4_ts_power_off(struct mip4_ts_info *info)
{
	int __maybe_unused ret = 0;

	TOUCH_D(BASE_INFO, "[START]\n");

	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	/* Use CE pin */
#if 0
	if (info->gpio_ce) {
		gpio_direction_output(info->gpio_ce, 0);
		TOUCH_D(BASE_INFO, "gpio_ce : 0\n");
	}
#endif

	/* Use VD33 regulator */
#if 1
	mip4_ts_control_regulator(info, 0);
#endif

	/* Use VD33_EN pin */
#if 0
	if (info->gpio_vd33_en) {
		gpio_direction_output(info->gpio_vd33_en, 0);
		TOUCH_D(BASE_INFO, "gpio_vd33_en : 0\n");
	}
#endif

#ifdef CONFIG_OF
	/* Use pinctrl */
#if 0
	if (!IS_ERR_OR_NULL(info->pinctrl)) {
		ret = pinctrl_select_state(info->pinctrl, info->pins_disable);
		if (ret < 0) {
			TOUCH_E("[ERROR] pinctrl_select_state : pins_disable\n");
		} else {
			TOUCH_D(BASE_INFO, "pinctrl_select_state : disable\n");
		}
	}
#endif
#endif /* CONFIG_OF */

	//
	//////////////////////////

	usleep_range(1 * 1000, 2 * 1000);

	TOUCH_D(BASE_INFO, "[DONE]\n");
	return 0;
}

/*
* Turn on power supply
*/
int mip4_ts_power_on(struct mip4_ts_info *info)
{
	int __maybe_unused ret = 0;

	TOUCH_D(BASE_INFO, "[START]\n");

	//////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	/* Use VD33 regulator */
#if 1
	mip4_ts_control_regulator(info, 1);
#endif

	/* Use VD33_EN pin */
#if 0
	if (info->gpio_vd33_en) {
		gpio_direction_output(info->gpio_vd33_en, 1);
		TOUCH_D(BASE_INFO, "gpio_vd33_en : 1\n");
	}
#endif

	/* Use CE pin */
#if 0
	if (info->gpio_ce) {
		gpio_direction_output(info->gpio_ce, 1);
		TOUCH_D(BASE_INFO, "gpio_ce : 1\n");
	}
#endif

#ifdef CONFIG_OF
	/* Use pinctrl */
#if 0
	if (!IS_ERR_OR_NULL(info->pinctrl)) {
		ret = pinctrl_select_state(info->pinctrl, info->pins_enable);
		if (ret < 0) {
			TOUCH_E("[ERROR] pinctrl_select_state : pins_enable\n");
		} else {
			TOUCH_D(BASE_INFO, "pinctrl_select_state : enable\n");
		}
	}
#endif
#endif /* CONFIG_OF */

	//
	//////////////////////////

#if !USE_STARTUP_WAITING
	msleep(200);
#endif

	TOUCH_D(BASE_INFO, "[DONE]\n");
	return 0;
}

/*
* Clear touch input event status
*/
void mip4_ts_clear_input(struct mip4_ts_info *info)
{
	int i;

	TOUCH_D(BASE_INFO, "[START]\n");

	/* Screen */
	for (i = 0; i < MAX_FINGER_NUM; i++) {
		/////////////////////////////////
		// PLEASE MODIFY HERE !!!
		//

		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

		input_report_key(info->input_dev, BTN_TOUCH, 0);
		input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

		//input_mt_sync(info->input_dev);

		info->touch_state[i] = 0;

		//
		/////////////////////////////////
	}
	input_sync(info->input_dev);

	/* Key */
	if (info->key_enable == true) {
		for (i = 0; i < info->key_num; i++) {
			input_report_key(info->input_dev, info->key_code[i], 0);
		}
	}
	input_sync(info->input_dev);

	TOUCH_D(BASE_INFO, "[DONE]\n");
}

/*
* Input event handler - Report input event
*/
void mip4_ts_input_event_handler(struct mip4_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	int type;
	int id;
	int hover = 0;
	int palm = 0;
	int state = 0;
	int x, y, z;
	int size = 0;
	int pressure_stage = 0;
	int pressure = 0;
	int touch_major = 0;
	int touch_minor = 0;
	int finger_id = 0;
	int finger_cnt = 0;

	TOUCH_D(TRACE, "[START]\n");
	//print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " Event Packet : ", DUMP_PREFIX_OFFSET, 16, 1, buf, sz, false);

	for (i = 0; i < sz; i += info->event_size) {
		u8 *packet = &buf[i];

		/* Event format & type */
		if ((info->event_format == 0) || (info->event_format == 1)) {
			type = (packet[0] & 0x40) >> 6;
		} else if (info->event_format == 3) {
			type = (packet[0] & 0xF0) >> 4;
		} else {
			TOUCH_E("[ERROR] Unknown event format [%d]\n", info->event_format);
			goto exit;
		}
		TOUCH_D(TRACE, "Type[%d]\n", type);

		/* Report input event */
		if (type == MIP4_EVENT_INPUT_TYPE_SCREEN) {
			/* Screen event */
			if (info->event_format == 0) {
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[5];
				touch_minor = packet[5];
			} else if (info->event_format == 1) {
				state = (packet[0] & 0x80) >> 7;
				hover = (packet[0] & 0x20) >> 5;
				palm = (packet[0] & 0x10) >> 4;
				id = (packet[0] & 0x0F) - 1;
				x = ((packet[1] & 0x0F) << 8) | packet[2];
				y = (((packet[1] >> 4) & 0x0F) << 8) | packet[3];
				pressure = packet[4];
				size = packet[5];
				touch_major = packet[6];
				touch_minor = packet[7];
			} else if (info->event_format == 3) {
				id = (packet[0] & 0x0F) - 1;
				hover = (packet[1] & 0x04) >> 2;
				palm = (packet[1] & 0x02) >> 1;
				state = (packet[1] & 0x01);
				x = ((packet[2] & 0x0F) << 8) | packet[3];
				y = (((packet[2] >> 4) & 0x0F) << 8) | packet[4];
				z = packet[5];
				size = packet[6];
				pressure_stage = (packet[7] & 0xF0) >> 4;
				pressure = ((packet[7] & 0x0F) << 8) | packet[8];
				touch_major = packet[9];
				touch_minor = packet[10];
			} else {
				TOUCH_E("[ERROR] Unknown event format [%d]\n", info->event_format);
				goto exit;
			}

			if (!((id >= 0) && (id < MAX_FINGER_NUM))) {
				TOUCH_E("[ERROR] Unknown finger id [%d]\n", id);
				continue;
			}

			/////////////////////////////////
			// PLEASE MODIFY HERE !!!
			//

			/* Report screen event */
			if (state == 1) {
				/* Press or move event*/
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, true);

				input_report_key(info->input_dev, BTN_TOUCH, 1);
				input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);

#if defined(DD14_RESOLUTION) || defined(WIDEPD_RESOLUTION) || defined(SMALLPD_RESOLUTION)
				input_report_abs(info->input_dev, ABS_MT_POSITION_X, (x * X_AXIS) / AXIS_RATIO);
				input_report_abs(info->input_dev, ABS_MT_POSITION_Y, (y * Y_AXIS) / AXIS_RATIO);
#else
				input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
				input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
#endif
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
				input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR, touch_minor);

				input_report_abs(info->input_dev, ABS_MT_TRACKING_ID, id);
				//input_mt_sync(info->input_dev);

				if(info->touch_state[id] == 0 || touch_debug_mask >= TRACE)
#if defined(DD14_RESOLUTION) || defined(WIDEPD_RESOLUTION) || defined(SMALLPD_RESOLUTION)
					TOUCH_D(BASE_INFO, "Screen : ID[%d] X[%d] Y[%d] Z[%d] Major[%d] Minor[%d] Size[%d] Pressure[%d] Palm[%d] Hover[%d]\n", id, (x * X_AXIS) / AXIS_RATIO, (y * Y_AXIS) / AXIS_RATIO, pressure, touch_major, touch_minor, size, pressure, palm, hover);
#else
					TOUCH_D(BASE_INFO, "Screen : ID[%d] X[%d] Y[%d] Z[%d] Major[%d] Minor[%d] Size[%d] Pressure[%d] Palm[%d] Hover[%d]\n", id, x, y, pressure, touch_major, touch_minor, size, pressure, palm, hover);
#endif
				
				info->touch_state[id] = 1;				
			} else if (state == 0) {
				/* Release event */
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

				input_report_key(info->input_dev, BTN_TOUCH, 0);
				input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

				//input_mt_sync(info->input_dev);

				info->touch_state[id] = 0;

				TOUCH_D(BASE_INFO, "Screen : ID[%d] Release\n", id);

				/* Final release event */
				finger_cnt = 0;
				for (finger_id = 0; finger_id < MAX_FINGER_NUM; finger_id++) {
					if (info->touch_state[finger_id] != 0) {
						finger_cnt++;
						break;
					}
				}
				if (finger_cnt <= 0) {
					input_report_key(info->input_dev, BTN_TOUCH, 0);
					input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

					TOUCH_D(TRACE, "Screen : Release\n");
				}
			} else {
				TOUCH_E("[ERROR] Unknown event state [%d]\n", state);
				goto exit;
			}

			//
			/////////////////////////////////
		} else if (type == MIP4_EVENT_INPUT_TYPE_KEY) {
			/* Key event */
			if ((info->event_format == 0) || (info->event_format == 1)) {
				id = (packet[0] & 0x0F) - 1;
				state = (packet[0] & 0x80) >> 7;
			} else if (info->event_format == 3) {
				id = (packet[0] & 0x0F) - 1;
				state = (packet[1] & 0x01);
			} else {
				TOUCH_E("[ERROR] Unknown event format [%d]\n", info->event_format);
				goto exit;
			}

			/* Report key event */
			if ((id >= 0) && (id < info->key_num)) {
				/////////////////////////////////
				// PLEASE MODIFY HERE !!!
				//

				int keycode = info->key_code[id];

				input_report_key(info->input_dev, keycode, state);

				TOUCH_D(BASE_INFO, "Key : ID[%d] Code[%d] Event[%d]\n", id, keycode, state);

				//
				/////////////////////////////////
			} else {
				TOUCH_E("[ERROR] Unknown key id [%d]\n", id);
				continue;
			}
		} else if (type == MIP4_EVENT_INPUT_TYPE_PROXIMITY) {
			/* Proximity event */

			/////////////////////////////////
			// PLEASE MODIFY HERE !!!
			//

			state = (packet[1] & 0x01);
			z = packet[5];

			TOUCH_D(BASE_INFO, "Proximity : State[%d] Value[%d]\n", state, z);

			//
			/////////////////////////////////
		} else {
			TOUCH_E("[ERROR] Unknown event type [%d]\n", type);
			goto exit;
		}
	}

	input_sync(info->input_dev);

exit:
	TOUCH_D(TRACE, "[DONE]\n");
}

/*
* Wake-up gesture event handler
*/
int mip4_ts_gesture_wakeup_event_handler(struct mip4_ts_info *info, int gesture_code)
{
	u8 wbuf[4];

	TOUCH_D(BASE_INFO, "[START]\n");

	/////////////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	/* Report wake-up event */
	TOUCH_D(BASE_INFO, "gesture[%d]\n", gesture_code);

	info->wakeup_gesture_code = gesture_code;

	switch (gesture_code) {
	case MIP4_EVENT_GESTURE_C:
	case MIP4_EVENT_GESTURE_W:
	case MIP4_EVENT_GESTURE_V:
	case MIP4_EVENT_GESTURE_M:
	case MIP4_EVENT_GESTURE_S:
	case MIP4_EVENT_GESTURE_Z:
	case MIP4_EVENT_GESTURE_O:
	case MIP4_EVENT_GESTURE_E:
	case MIP4_EVENT_GESTURE_V_90:
	case MIP4_EVENT_GESTURE_V_180:
	case MIP4_EVENT_GESTURE_FLICK_RIGHT:
	case MIP4_EVENT_GESTURE_FLICK_DOWN:
	case MIP4_EVENT_GESTURE_FLICK_LEFT:
	case MIP4_EVENT_GESTURE_FLICK_UP:
	case MIP4_EVENT_GESTURE_DOUBLE_TAP:
		/* Example : emulate power key */
		input_report_key(info->input_dev, KEY_POWER, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_POWER, 0);
		input_sync(info->input_dev);
		break;
	default:
		/* Re-enter gesture wake-up mode */
		wbuf[0] = MIP4_R0_CTRL;
		wbuf[1] = MIP4_R1_CTRL_POWER_STATE;
		wbuf[2] = MIP4_CTRL_POWER_LOW;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			TOUCH_E("[ERROR] mip4_ts_i2c_write\n");
			goto error;
		}
		break;
	}

	//
	/////////////////////////////////

	TOUCH_D(BASE_INFO, "[DONE]\n");
	return 0;

error:
	return 1;
}

/*
* Config GPIO
*/
int mip4_ts_config_gpio(struct mip4_ts_info *info)
{
	int ret = 0;

	TOUCH_D(BASE_INFO, "[START]\n");

	/* Interrupt */
	TOUCH_D(BASE_INFO, "gpio_intr[%d]\n", info->gpio_intr);
	if (info->gpio_intr) {
		ret = gpio_request(info->gpio_intr, "irq-gpio");
		if (ret < 0) {
			TOUCH_E("[ERROR] gpio_request : irq-gpio\n");
			goto error;
		} else {
			gpio_direction_input(info->gpio_intr);

			/* Set IRQ */
			info->client->irq = gpio_to_irq(info->gpio_intr);
			info->irq = info->client->irq;
			TOUCH_D(BASE_INFO, "gpio_to_irq : irq[%d]\n", info->irq);
		}
	}

#if 0
	/* CE (Optional) */
	TOUCH_D(BASE_INFO, "gpio_ce[%d]\n", info->gpio_ce);
	if (info->gpio_ce) {
		ret = gpio_request(info->gpio_ce, "ce-gpio");
		if (ret < 0) {
			TOUCH_E("[ERROR] gpio_request : ce-gpio\n");
		} else {
			gpio_direction_output(info->gpio_ce, 0);
		}
	}

	/* VD33_EN (Optional) */
	TOUCH_D(BASE_INFO, "gpio_vd33_en[%d]\n", info->gpio_vd33_en);
	if (info->gpio_vd33_en) {
		ret = gpio_request(info->gpio_vd33_en, "vd33_en-gpio");
		if (ret < 0) {
			TOUCH_E("[ERROR] gpio_request : vd33_en-gpio\n");
		} else {
			gpio_direction_output(info->gpio_vd33_en, 0);
		}
	}
#endif

#ifdef CONFIG_OF
	/* Pinctrl (Optional) */
#if 0
	if (!IS_ERR_OR_NULL(info->pinctrl)) {
		ret = pinctrl_select_state(info->pinctrl, info->pins_enable);
		if (ret < 0) {
			TOUCH_E("[ERROR] pinctrl_select_state : pins_enable\n");
		} else {
			TOUCH_D(BASE_INFO, "pinctrl_select_state : enable\n");
		}
	}
#endif
#endif

	TOUCH_D(BASE_INFO, "[DONE]\n");
	return 0;

error:
	TOUCH_E("[ERROR]\n");
	return 0;
}

#ifdef CONFIG_OF
/*
* Parse device tree
*/
int mip4_ts_parse_devicetree(struct device *dev, struct mip4_ts_info *info)
{
	struct device_node *np = dev->of_node;
	int ret;

	TOUCH_D(BASE_INFO, "[START]\n");

	/////////////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	/* Get Interrupt GPIO */
	ret = of_get_named_gpio(np, "irq-gpio", 0);
	if (!gpio_is_valid(ret)) {
		TOUCH_E("[ERROR] of_get_named_gpio : irq-gpio\n");
		info->gpio_intr = 0;
	} else {
		info->gpio_intr = ret;
	}
	TOUCH_D(BASE_INFO, "gpio_intr[%d]\n", info->gpio_intr);

#if 0
	/* Get CE GPIO (Optional) */
	ret = of_get_named_gpio(np, "ce-gpio", 0);
	if (!gpio_is_valid(ret)) {
		TOUCH_E("[ERROR] of_get_named_gpio : ce-gpio\n");
		info->gpio_ce = 0;
	} else {
		info->gpio_ce = ret;
	}
	TOUCH_D(BASE_INFO, "gpio_ce[%d]\n", info->gpio_ce);

	/* Get VD33_EN GPIO (Optional) */
	ret = of_get_named_gpio(np, "vd33_en-gpio", 0);
	if (!gpio_is_valid(ret)) {
		TOUCH_E("[ERROR] of_get_named_gpio : vd33_en-gpio\n");
		info->gpio_vd33_en = 0;
	} else {
		info->gpio_vd33_en = ret;
	}
	TOUCH_D(BASE_INFO, "gpio_vd33_en[%d]\n", info->gpio_vd33_en);
#endif

	/* Get Pinctrl (Optional) */
	info->pinctrl = devm_pinctrl_get(&info->client->dev);
	if (IS_ERR(info->pinctrl)) {
		TOUCH_E("[ERROR] devm_pinctrl_get\n");
	} else {
		info->pins_enable = pinctrl_lookup_state(info->pinctrl, "enable");
		if (IS_ERR(info->pins_enable)) {
			TOUCH_E("[ERROR] pinctrl_lookup_state : enable\n");
		}

		info->pins_disable = pinctrl_lookup_state(info->pinctrl, "disable");
		if (IS_ERR(info->pins_disable)) {
			TOUCH_E("[ERROR] pinctrl_lookup_state : disable\n");
		}
	}

	//
	/////////////////////////////////

	/* Config GPIO */
	ret = mip4_ts_config_gpio(info);
	if (ret) {
		TOUCH_E("[ERROR] mip4_ts_config_gpio\n");
		goto error;
	}

	TOUCH_D(BASE_INFO, "[DONE]\n");
	return 0;

error:
	TOUCH_E("[ERROR]\n");
	return 1;
}
#endif

/*
* Config input interface
*/
void mip4_ts_config_input(struct mip4_ts_info *info)
{
	struct input_dev *input_dev = info->input_dev;

	TOUCH_D(BASE_INFO, "[START]\n");

	/////////////////////////////
	// PLEASE MODIFY HERE !!!
	//

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);

	/* Screen */
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);

	//input_mt_init_slots(input_dev, MAX_FINGER_NUM);
	input_mt_init_slots(input_dev, MAX_FINGER_NUM, INPUT_MT_DIRECT);

	//input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, MAX_FINGER_NUM, 0, 0);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, INPUT_PRESSURE_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, INPUT_TOUCH_MAJOR_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR, 0, INPUT_TOUCH_MINOR_MAX, 0, 0);

	/* Key */
	//set_bit(KEY_BACK, input_dev->keybit);
	//set_bit(KEY_MENU, input_dev->keybit);

	//info->key_code[0] = KEY_BACK;
	//info->key_code[1] = KEY_MENU;

#if USE_WAKEUP_GESTURE
	set_bit(KEY_POWER, input_dev->keybit);
#endif

	//
	/////////////////////////////

	TOUCH_D(BASE_INFO, "[DONE]\n");
}

