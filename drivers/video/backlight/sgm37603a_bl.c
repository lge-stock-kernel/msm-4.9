/* drivers/video/backlight/sgm37603a_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>

#include <soc/qcom/lge/board_lge.h>

#define I2C_BL_NAME                              "sgm37603a"
#define MAX_BRIGHTNESS_SGM37603A                    4095
#define MIN_BRIGHTNESS_SGM37603A                    0x03
#define DEFAULT_BRIGHTNESS                       0xFF
#define DEFAULT_FTM_BRIGHTNESS                   0x03
#define BRIGHT_LSB_MASK                          0x0F
#define BRIGHT_MSB_MASK                          0xFF
#define BRIGHT_MSB_SHIFT                         0x04
#define ON        1
#define OFF       0


/* LGE_CHANGE  - To turn backlight on by setting default brightness while kernel booting */
#define BOOT_BRIGHTNESS 1

static struct i2c_client *sgm37603a_i2c_client;

static int store_level_used = 0;

struct backlight_platform_data {
	void (*platform_init)(void);
	int gpio;
	unsigned int mode;
	int max_current;
	int init_on_boot;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	int blmap_size;
	char *blmap;
};

struct sgm37603a_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	int gpio;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	struct mutex bl_mutex;
	int blmap_size;
	char *blmap;
};

static const struct i2c_device_id sgm37603a_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};
#if defined(CONFIG_BACKLIGHT_CABC_DEBUG_ENABLE)
static int sgm37603a_read_reg(struct i2c_client *client, u8 reg, u8 *buf);
#endif

static int sgm37603a_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val);

static int cur_main_lcd_level = DEFAULT_BRIGHTNESS;
static int saved_main_lcd_level = DEFAULT_BRIGHTNESS;
static int backlight_status = OFF;
static int cabc_status = OFF;
static struct sgm37603a_device *main_sgm37603a_dev;

#ifdef CONFIG_LGE_PM_THERMALE_BACKLIGHT_CHG_CONTROL
int get_backlight_state(void)
{
	return backlight_status;
}
EXPORT_SYMBOL(get_backlight_state);
#endif

static void sgm37603a_hw_reset(void)
{
	int gpio = main_sgm37603a_dev->gpio;
	/* LGE_CHANGE - Fix GPIO Setting Warning*/
	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		mdelay(10);
		pr_info("%s: gpio is OK !!\n", __func__);
	}
	else
		pr_err("%s: gpio is not valid !!\n", __func__);
}
#if defined(CONFIG_BACKLIGHT_CABC_DEBUG_ENABLE)
static int sgm37603a_read_reg(struct i2c_client *client, u8 reg, u8 *buf)
{
	s32 ret;

	pr_info("[Display][DEBUG] reg: %x\n", reg);

	ret = i2c_smbus_read_byte_data(client, reg);

	if(ret < 0)
		pr_err("[Display][DEBUG] error\n");

	*buf = ret;

	return 0;

}
#endif
static int sgm37603a_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int err;
	u8 buf[2];
	struct i2c_msg msg = {
		client->addr, 0, 2, buf
	};

	buf[0] = reg;
	buf[1] = val;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		pr_info("i2c write error reg: %d, val: %d\n", buf[0], buf[1]  );

	return 0;
}

static int exp_min_value = 150;
static int cal_value;

static void sgm37603a_set_main_current_level(struct i2c_client *client, int level)
{
	struct sgm37603a_device *dev = i2c_get_clientdata(client);
	int min_brightness = dev->min_brightness;
	int max_brightness = dev->max_brightness;
	int reg_val = 0;

	if (level == -BOOT_BRIGHTNESS)
		level = dev->default_brightness;

	cur_main_lcd_level = level;
	dev->bl_dev->props.brightness = cur_main_lcd_level;

	store_level_used = 0;

	mutex_lock(&dev->bl_mutex);
	if (level != 0) {
		if (level > 0 && level <= min_brightness)
			level = min_brightness;
		else if (level > max_brightness)
			level = max_brightness;

		if (dev->blmap) {
			if (level < dev->blmap_size) {
				cal_value = dev->blmap[level];
			} else
				dev_warn(&client->dev, "invalid index %d:%d\n", dev->blmap_size, level);
		} else {
			cal_value = level;
		}

		pr_info("%s : level=%d, cal_value=%d \n", __func__, level, cal_value);
		reg_val = cal_value & BRIGHT_LSB_MASK;
		sgm37603a_write_reg(client, 0x1A, reg_val);

		reg_val = cal_value >> BRIGHT_MSB_SHIFT;
		reg_val = reg_val & BRIGHT_MSB_MASK;
		sgm37603a_write_reg(client, 0x19, reg_val);
	} else {
		pr_info("%s backlight off level : %d\n", __func__, level);
		sgm37603a_write_reg(client, 0x10, 0x00);
	}
	mutex_unlock(&dev->bl_mutex);
}

static void sgm37603a_set_main_current_level_no_mapping(
		struct i2c_client *client, int level)
{
	struct sgm37603a_device *dev;
	dev = (struct sgm37603a_device *)i2c_get_clientdata(client);

	if (level > 255)
		level = 255;
	else if (level < 0)
		level = 0;

	cur_main_lcd_level = level;
	dev->bl_dev->props.brightness = cur_main_lcd_level;

	store_level_used = 1;

	mutex_lock(&main_sgm37603a_dev->bl_mutex);
	if (level != 0) {
		sgm37603a_write_reg(client, 0x19, level);
	} else {
		sgm37603a_write_reg(client, 0x10, 0x00);
	}
	mutex_unlock(&main_sgm37603a_dev->bl_mutex);
	pr_info("[Display][SGM37603A] %s : backlight level=%d\n", __func__, level);
}

void sgm37603a_backlight_cabc(int enable)
{
	if(enable)
//		sgm37603a_write_reg(main_sgm37603a_dev->client, 0x11, 0x40);
		sgm37603a_write_reg(main_sgm37603a_dev->client, 0x11, 0x60);
	else
		sgm37603a_write_reg(main_sgm37603a_dev->client, 0x11, 0x00);

	pr_info("[Display][SGM37603A] %s : CABC state = %d \n", __func__, enable);
}

void sgm37603a_backlight_on(int level, int cabc)
{
	if (backlight_status == OFF) {

		pr_info("%s with level %d\n", __func__, level);
		sgm37603a_hw_reset();

		pr_info("[backlight] %s Enter sgm37603a initial\n", __func__);

		sgm37603a_write_reg(main_sgm37603a_dev->client, 0x1B, main_sgm37603a_dev->max_current);  //Bank A Full-scale current
		sgm37603a_backlight_cabc(cabc);

		sgm37603a_write_reg(main_sgm37603a_dev->client, 0x10, 0xFF);  //Enable Bank A
	}
	mdelay(1);
	sgm37603a_set_main_current_level(main_sgm37603a_dev->client, level);
	backlight_status = ON;

	return;
}

void sgm37603a_backlight_off(void)
{
	int gpio = main_sgm37603a_dev->gpio;

	pr_info("+\n%s\n", __func__);
	if (backlight_status == OFF)
		return;

	saved_main_lcd_level = cur_main_lcd_level;
	sgm37603a_set_main_current_level(main_sgm37603a_dev->client, 0);
	backlight_status = OFF;

	gpio_direction_output(gpio, 0);
	msleep(6);

	pr_info("%s\n-\n", __func__);
	return;
}

void sgm37603a_lcd_backlight_set_level(int level, int cabc)
{
	if (level > MAX_BRIGHTNESS_SGM37603A)
		level = MAX_BRIGHTNESS_SGM37603A;

	cabc_status = cabc;

	if (sgm37603a_i2c_client != NULL) {
		if (level == 0) {
			sgm37603a_backlight_off();
		} else {
			sgm37603a_backlight_on(level, cabc);
		}
	} else {
		pr_err("%s(): No client\n", __func__);
	}
}
EXPORT_SYMBOL(sgm37603a_lcd_backlight_set_level);

static int bl_set_intensity(struct backlight_device *bd)
{
	struct i2c_client *client = to_i2c_client(bd->dev.parent);

	/* LGE_CHANGE - if it's trying to set same backlight value, skip it.*/
	if(bd->props.brightness == cur_main_lcd_level){
		pr_info("%s level is already set. skip it\n", __func__);
		return 0;
	}

	pr_info("%s bd->props.brightness : %d     cur_main_lcd_level : %d \n", __func__, bd->props.brightness, cur_main_lcd_level);

	sgm37603a_set_main_current_level(client, bd->props.brightness);
	cur_main_lcd_level = bd->props.brightness;

	return 0;
}

static int bl_get_intensity(struct backlight_device *bd)
{
	unsigned char val = 0;
	val &= 0x1f;

	return (int)val;
}

static ssize_t lcd_backlight_show_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;

	if(store_level_used == 0)
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
				cal_value);
	else if(store_level_used == 1)
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
				cur_main_lcd_level);

	return r;
}

static ssize_t lcd_backlight_store_level(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int level;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);

	sgm37603a_set_main_current_level_no_mapping(client, level);
	pr_info("[Display][DEBUG] write %d direct to "
			"backlight register\n", level);

	return count;
}

static int sgm37603a_bl_resume(struct i2c_client *client)
{
	sgm37603a_lcd_backlight_set_level(saved_main_lcd_level, cabc_status);
	return 0;
}

static int sgm37603a_bl_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("[Display][DEBUG] %s: new state: %d\n",
			__func__, state.event);

	sgm37603a_lcd_backlight_set_level(saved_main_lcd_level, cabc_status);
	return 0;
}

static ssize_t lcd_backlight_show_on_off(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;

	pr_info("%s received (prev backlight_status: %s)\n",
			__func__, backlight_status ? "ON" : "OFF");

	return r;
}

static ssize_t lcd_backlight_store_on_off(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int on_off;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	pr_info("%s received (prev backlight_status: %s)\n",
			__func__, backlight_status ? "ON" : "OFF");

	on_off = simple_strtoul(buf, NULL, 10);

	pr_info("[Display][DEBUG] %d", on_off);

	if (on_off == 1)
		sgm37603a_bl_resume(client);
	else if (on_off == 0)
		sgm37603a_bl_suspend(client, PMSG_SUSPEND);

	return count;

}
static ssize_t lcd_backlight_show_exp_min_value(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;

	r = snprintf(buf, PAGE_SIZE, "LCD Backlight  : %d\n", exp_min_value);

	return r;
}

static ssize_t lcd_backlight_store_exp_min_value(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);
	exp_min_value = value;

	return count;
}

static ssize_t lcd_backlight_show_fs_curr(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;

	r = snprintf(buf, PAGE_SIZE, "LCD fs-curr : %d\n", main_sgm37603a_dev->max_current);

	return r;
}

static ssize_t lcd_backlight_store_fs_curr(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);
	main_sgm37603a_dev->max_current = value;

	return count;
}

static ssize_t lcd_backlight_show_cabc(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;

	r = snprintf(buf, PAGE_SIZE, "CABC state : %d \n", cabc_status);

	return r;
}
static ssize_t lcd_backlight_store_cabc(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);
	sgm37603a_backlight_cabc(value);

	return count;
}

DEVICE_ATTR(sgm37603a_level, 0644, lcd_backlight_show_level,
		lcd_backlight_store_level);
DEVICE_ATTR(sgm37603a_backlight_on_off, 0644, lcd_backlight_show_on_off,
		lcd_backlight_store_on_off);
DEVICE_ATTR(sgm37603a_exp_min_value, 0644, lcd_backlight_show_exp_min_value,
		lcd_backlight_store_exp_min_value);
DEVICE_ATTR(sgm37603a_fs_curr, 0644, lcd_backlight_show_fs_curr,
		lcd_backlight_store_fs_curr);
DEVICE_ATTR(sgm37603a_cabc, 0644, lcd_backlight_show_cabc, lcd_backlight_store_cabc);

static int sgm37603a_parse_dt(struct device *dev,
		struct backlight_platform_data *pdata)
{
	int rc = 0, i;
	u32 *array;
	struct device_node *np = dev->of_node;

	pdata->gpio = of_get_named_gpio_flags(np,
			"sgm37603a,lcd_bl_en", 0, NULL);
	rc = of_property_read_u32(np, "sgm37603a,max_current",
			&pdata->max_current);
	rc = of_property_read_u32(np, "sgm37603a,min_brightness",
			&pdata->min_brightness);
	rc = of_property_read_u32(np, "sgm37603a,default_brightness",
			&pdata->default_brightness);
	rc = of_property_read_u32(np, "sgm37603a,max_brightness",
			&pdata->max_brightness);
//	rc = of_property_read_u32(np, "sgm37603a,enable_cabc",
//			&sgm37603a_cabc_enable);
	rc = of_property_read_u32(np, "sgm37603a,blmap_size",
			&pdata->blmap_size);

	if (pdata->blmap_size) {
		array = kzalloc(sizeof(u32) * pdata->blmap_size, GFP_KERNEL);
		if (!array)
			return -ENOMEM;

		rc = of_property_read_u32_array(np, "sgm37603a,blmap", array, pdata->blmap_size);
		if (rc) {
			pr_err("%s:%d, uable to read backlight map\n",__func__, __LINE__);
			kfree(array);
			return -EINVAL;
		}
		pdata->blmap = kzalloc(sizeof(char) * pdata->blmap_size, GFP_KERNEL);

		if (!pdata->blmap) {
			kfree(array);
			return -ENOMEM;
		}

		for (i = 0; i < pdata->blmap_size; i++ )
			pdata->blmap[i] = (char)array[i];

		if (array)
			kfree(array);


	} else {
		pdata->blmap_size = 0;
		pdata->blmap = NULL;
		rc = 0;
	}

	pr_info("[sgm37603a] %s gpio: %d, max_current: %d, min: %d, "
			"default: %d, max: %d, blmap_size : %d\n",
			__func__, pdata->gpio,
			pdata->max_current,
			pdata->min_brightness,
			pdata->default_brightness,
			pdata->max_brightness,
			pdata->blmap_size);

	return rc;
}

static struct backlight_ops sgm37603a_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};

static int sgm37603a_probe(struct i2c_client *i2c_dev,
		const struct i2c_device_id *id)
{
	struct backlight_platform_data *pdata;
	struct sgm37603a_device *dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err;

	pr_info("[Display][sgm37603a] %s: i2c probe start\n", __func__);

	if (&i2c_dev->dev.of_node) {
		pdata = devm_kzalloc(&i2c_dev->dev,
				sizeof(struct backlight_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		err = sgm37603a_parse_dt(&i2c_dev->dev, pdata);
		if (err != 0)
			return err;
	} else {
		pdata = i2c_dev->dev.platform_data;
	}

	pr_info("[Display][sgm37603a] %s: gpio = %d\n", __func__,pdata->gpio);
	if (pdata->gpio && gpio_request(pdata->gpio, "sgm37603a reset") != 0) {
		return -ENODEV;
	}

	sgm37603a_i2c_client = i2c_dev;

	dev = kzalloc(sizeof(struct sgm37603a_device), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(&i2c_dev->dev, "fail alloc for sgm37603a_device\n");
		return 0;
	}
	main_sgm37603a_dev = dev;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;

	props.max_brightness = MAX_BRIGHTNESS_SGM37603A;
	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev,
			NULL, &sgm37603a_bl_ops, &props);
	bl_dev->props.max_brightness = MAX_BRIGHTNESS_SGM37603A;
	bl_dev->props.brightness = DEFAULT_BRIGHTNESS;
	bl_dev->props.power = FB_BLANK_UNBLANK;

	dev->bl_dev = bl_dev;
	dev->client = i2c_dev;

	dev->gpio = pdata->gpio;
	dev->max_current = pdata->max_current;
	dev->min_brightness = pdata->min_brightness;
	dev->default_brightness = pdata->default_brightness;
	dev->max_brightness = pdata->max_brightness;
	dev->blmap_size = pdata->blmap_size;

	if (dev->blmap_size) {
		dev->blmap = kzalloc(sizeof(char) * dev->blmap_size, GFP_KERNEL);
		if (!dev->blmap) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		memcpy(dev->blmap, pdata->blmap, dev->blmap_size);
	} else {
		dev->blmap = NULL;
	}

	if (gpio_get_value(dev->gpio)){
		backlight_status = ON;
	}
	else
		backlight_status = OFF;

	i2c_set_clientdata(i2c_dev, dev);

	mutex_init(&dev->bl_mutex);

	err = device_create_file(&i2c_dev->dev,
			&dev_attr_sgm37603a_level);
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_sgm37603a_backlight_on_off);
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_sgm37603a_exp_min_value);
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_sgm37603a_fs_curr);
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_sgm37603a_cabc);

	return 0;
}

static int sgm37603a_remove(struct i2c_client *i2c_dev)
{
	struct sgm37603a_device *dev;
	int gpio = main_sgm37603a_dev->gpio;

	pr_info("[Display][sgm37603a] %s: ++\n", __func__);
	device_remove_file(&i2c_dev->dev, &dev_attr_sgm37603a_level);
	device_remove_file(&i2c_dev->dev, &dev_attr_sgm37603a_backlight_on_off);
	dev = (struct sgm37603a_device *)i2c_get_clientdata(i2c_dev);
	backlight_device_unregister(dev->bl_dev);
	i2c_set_clientdata(i2c_dev, NULL);

	if (gpio_is_valid(gpio))
	{
		pr_info("[Display][sgm37603a] %s: gpio %d free \n", __func__, gpio);
		gpio_free(gpio);
	}

	pr_info("[Display][sgm37603a] %s: -- \n", __func__);
	return 0;
}

static struct of_device_id sgm37603a_match_table[] = {
	{ .compatible = "sgm37603a",},
	{ },
};

static struct i2c_driver main_sgm37603a_driver = {
	.probe = sgm37603a_probe,
	.remove = sgm37603a_remove,
	.id_table = sgm37603a_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sgm37603a_match_table,
	},
};

static int __init lcd_backlight_init(void)
{
	static int err;

	err = i2c_add_driver(&main_sgm37603a_driver);

	return err;
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("sgm37603a Backlight Control");
MODULE_LICENSE("GPL");
