/* drivers/video/backlight/external_bl.c
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

#define I2C_BL_NAME                              "ext_bl"
#define MAX_BRIGHTNESS_EXT_BL                    2047
#define MIN_BRIGHTNESS_EXT_BL                    0x03
#define DEFAULT_BRIGHTNESS                       0xFF
#define DEFAULT_FTM_BRIGHTNESS                   0x03
#define BRIGHT_LSB_MASK                          0x07
#define BRIGHT_MSB_MASK                          0xFF
#define BRIGHT_MSB_SHIFT                         0x03
#define BL_ON        1
#define BL_OFF       0


/* LGE_CHANGE  - To turn backlight on by setting default brightness while kernel booting */
#define BOOT_BRIGHTNESS 1

static struct i2c_client *ext_bl_i2c_client;
static int store_level_used = 0;

struct backlight_platform_data {
	void (*platform_init)(void);
	int gpio;
	unsigned int mode;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int control_bank;
	int bl_config;
	int cur_sink_feedback;
	int boost_control;
	int cabc_enable;
	int blmap_size;
	char *blmap;
};

struct ext_bl_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	int gpio;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int control_bank;
	int bl_config;
	int cur_sink_feedback;
	int boost_control;
	int cabc_enable;
	struct mutex bl_mutex;
	int i2c_addr;
	int i2c_data;
	int blmap_size;
	char *blmap;
};

static const struct i2c_device_id ext_bl_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};
static int ext_bl_read_reg(struct i2c_client *client, u8 reg, u8 *buf);
static int ext_bl_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val);

static int cur_main_lcd_level = DEFAULT_BRIGHTNESS;
static int saved_main_lcd_level = DEFAULT_BRIGHTNESS;
static int backlight_status = BL_OFF;
static struct ext_bl_device *main_ext_bl_dev;

static void ext_bl_hw_reset(void)
{
	int gpio = main_ext_bl_dev->gpio;

	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		mdelay(5);
		pr_info("%s: gpio is OK !!\n", __func__);
	}
	else
		pr_err("%s: gpio is not valid !!\n", __func__);
}

static int ext_bl_read_reg(struct i2c_client *client, u8 reg, u8 *buf)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if(ret < 0)
		pr_err("[Display][DEBUG] error\n");

	*buf = ret;

	return 0;
}

static int ext_bl_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int err;
	u8 buf[2];
	struct i2c_msg msg = { client->addr, 0, 2, buf };

	buf[0] = reg;
	buf[1] = val;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		pr_info("i2c write error reg: %d, val: %d\n", buf[0], buf[1]  );

	return 0;
}

static int cal_value;
static void ext_bl_set_main_current_level(struct i2c_client *client, int level)
{
	struct ext_bl_device *dev = i2c_get_clientdata(client);
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
		ext_bl_write_reg(client, 0x20, reg_val);

		reg_val = cal_value >> BRIGHT_MSB_SHIFT;
		reg_val = reg_val & BRIGHT_MSB_MASK;
		ext_bl_write_reg(client, 0x21, reg_val);
	} else {
		pr_info("%s backlight off level : %d\n", __func__, level);
		ext_bl_write_reg(client, 0x24, 0x00);
	}
	mutex_unlock(&dev->bl_mutex);
}

void ext_bl_backlight_cabc(int enable)
{
	if(enable)
		ext_bl_write_reg(main_ext_bl_dev->client, 0x1C, 0x0D);
	else
		ext_bl_write_reg(main_ext_bl_dev->client, 0x1C, 0x0C);

	pr_info("[Display][ext_bl] %s : CABC state = %d \n", __func__, enable);
}

void ext_bl_backlight_on(int level)
{
	if (backlight_status == BL_OFF) {

		pr_info("%s with level %d\n", __func__, level);
		ext_bl_hw_reset();

		pr_info("[backlight] %s Enter ext_bl initial\n", __func__);

		ext_bl_write_reg(main_ext_bl_dev->client, 0x10, main_ext_bl_dev->control_bank);	//HVLED1&2 assigned to control bank A
		ext_bl_write_reg(main_ext_bl_dev->client, 0x16, main_ext_bl_dev->bl_config);  //Linear Mapping Mode
		ext_bl_write_reg(main_ext_bl_dev->client, 0x17, main_ext_bl_dev->max_current);  //Bank A Full-scale current
		ext_bl_write_reg(main_ext_bl_dev->client, 0x19, main_ext_bl_dev->cur_sink_feedback);	// Eable feedback on HVLED 1&2
		ext_bl_write_reg(main_ext_bl_dev->client, 0x1A, main_ext_bl_dev->boost_control);  //OVP 32V, Freq 500kh
		ext_bl_backlight_cabc(main_ext_bl_dev->cabc_enable);

		ext_bl_write_reg(main_ext_bl_dev->client, 0x24, 0x01);  //Enable Bank A
	}
	mdelay(1);

	ext_bl_set_main_current_level(main_ext_bl_dev->client, level);
	backlight_status = BL_ON;

	return;
}

void ext_bl_backlight_off(void)
{
	int gpio = main_ext_bl_dev->gpio;

	pr_info("+\n%s\n", __func__);
	if (backlight_status == BL_OFF)
		return;

	saved_main_lcd_level = cur_main_lcd_level;
	ext_bl_set_main_current_level(main_ext_bl_dev->client, 0);
	backlight_status = BL_OFF;

	gpio_direction_output(gpio, 0);
	msleep(5);

	pr_info("%s\n-\n", __func__);
	return;
}

void ext_bl_lcd_backlight_set_level(int level)
{
	if (level > MAX_BRIGHTNESS_EXT_BL)
		level = MAX_BRIGHTNESS_EXT_BL;

	if (ext_bl_i2c_client != NULL) {
		if (level == 0) {
			ext_bl_backlight_off();
		} else {
			ext_bl_backlight_on(level);
		}
	} else {
		pr_err("%s(): No client\n", __func__);
	}
}
EXPORT_SYMBOL(ext_bl_lcd_backlight_set_level);

static int bl_set_intensity(struct backlight_device *bd)
{
	struct i2c_client *client = to_i2c_client(bd->dev.parent);

	/* LGE_CHANGE - if it's trying to set same backlight value, skip it.*/
	if(bd->props.brightness == cur_main_lcd_level){
		pr_info("%s level is already set. skip it\n", __func__);
		return 0;
	}

	pr_info("%s bd->props.brightness : %d, cur_main_lcd_level : %d \n", __func__, bd->props.brightness, cur_main_lcd_level);

	ext_bl_set_main_current_level(client, bd->props.brightness);
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
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n", cal_value);
	else if(store_level_used == 1)
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n", cur_main_lcd_level);

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

	ext_bl_set_main_current_level(client, level);
	pr_info("[Display][DEBUG] write %d direct to backlight register\n", level);

	return count;
}

static int ext_bl_bl_resume(struct i2c_client *client)
{
	ext_bl_lcd_backlight_set_level(saved_main_lcd_level);
	return 0;
}

static int ext_bl_bl_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("[Display][DEBUG] %s: new state: %d\n", __func__, state.event);

	ext_bl_lcd_backlight_set_level(saved_main_lcd_level);
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
		ext_bl_bl_resume(client);
	else if (on_off == 0)
		ext_bl_bl_suspend(client, PMSG_SUSPEND);

	return count;
}

static ssize_t lcd_backlight_show_fs_curr(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;
	r = snprintf(buf, PAGE_SIZE, "LCD fs-curr : %d\n", main_ext_bl_dev->max_current);

	return r;
}

static ssize_t lcd_backlight_store_fs_curr(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);
	main_ext_bl_dev->max_current = value;

	return count;
}

static ssize_t lcd_backlight_show_cabc(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;
	r = snprintf(buf, PAGE_SIZE, "CABC state : %d \n", main_ext_bl_dev->cabc_enable);

	return r;
}
static ssize_t lcd_backlight_store_cabc(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;

	value = simple_strtoul(buf, NULL, 10);
	main_ext_bl_dev->cabc_enable = value;
	ext_bl_backlight_cabc(main_ext_bl_dev->cabc_enable);

	return count;
}

static ssize_t lcd_backlight_show_i2c_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;
	u8 data;

	mutex_lock(&main_ext_bl_dev->bl_mutex);
	ext_bl_read_reg(main_ext_bl_dev->client, main_ext_bl_dev->i2c_addr, &data);
	main_ext_bl_dev->i2c_data = data;
	mutex_unlock(&main_ext_bl_dev->bl_mutex);

	r = snprintf(buf, PAGE_SIZE, "Backlight I2C read addr:0x%x, value:0x%x\n",
			main_ext_bl_dev->i2c_addr, main_ext_bl_dev->i2c_data);

	return r;
}

static ssize_t lcd_backlight_store_i2c_data(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 data;

	data = (char)simple_strtoul(buf, NULL, 16);

	mutex_lock(&main_ext_bl_dev->bl_mutex);
	ext_bl_write_reg(main_ext_bl_dev->client, main_ext_bl_dev->i2c_addr, data);
	mutex_unlock(&main_ext_bl_dev->bl_mutex);

	return count;
}

static ssize_t lcd_backlight_store_i2c_addr(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int addr;

	if (!count)
		return -EINVAL;

	addr = (char)simple_strtoul(buf, NULL, 16);
	main_ext_bl_dev->i2c_addr = addr;

	return count;
}

static ssize_t lcd_backlight_show_i2c_addr(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;

	r = snprintf(buf, PAGE_SIZE, "I2C addr: 0x%x \n", main_ext_bl_dev->i2c_addr);

	return r;
}

DEVICE_ATTR(ext_bl_level, 0644, lcd_backlight_show_level, lcd_backlight_store_level);
DEVICE_ATTR(ext_bl_backlight_on_off, 0644, lcd_backlight_show_on_off, lcd_backlight_store_on_off);
DEVICE_ATTR(ext_bl_fs_curr, 0644, lcd_backlight_show_fs_curr, lcd_backlight_store_fs_curr);
DEVICE_ATTR(ext_bl_cabc, 0644, lcd_backlight_show_cabc, lcd_backlight_store_cabc);
DEVICE_ATTR(ext_bl_i2c_addr, 0644, lcd_backlight_show_i2c_addr, lcd_backlight_store_i2c_addr);
DEVICE_ATTR(ext_bl_i2c_data, 0644, lcd_backlight_show_i2c_data, lcd_backlight_store_i2c_data);

static int ext_bl_parse_dt(struct device *dev,
		struct backlight_platform_data *pdata)
{
	int rc = 0, i;
	u32 *array;
	struct device_node *np = dev->of_node;

	pdata->gpio = of_get_named_gpio_flags(np,
			"ext_bl,lcd_bl_en", 0, NULL);
	rc = of_property_read_u32(np, "ext_bl,max_current",
			&pdata->max_current);
	rc = of_property_read_u32(np, "ext_bl,min_brightness",
			&pdata->min_brightness);
	rc = of_property_read_u32(np, "ext_bl,default_brightness",
			&pdata->default_brightness);
	rc = of_property_read_u32(np, "ext_bl,max_brightness",
			&pdata->max_brightness);
	rc = of_property_read_u32(np, "ext_bl,control_bank",
			&pdata->control_bank);
	rc = of_property_read_u32(np, "ext_bl,bl_config",
			&pdata->bl_config);
	rc = of_property_read_u32(np, "ext_bl,cur_sink_feedback",
			&pdata->cur_sink_feedback);
	rc = of_property_read_u32(np, "ext_bl,boost_control",
			&pdata->boost_control);
	rc = of_property_read_u32(np, "ext_bl,enable_cabc",
			&pdata->cabc_enable);
	rc = of_property_read_u32(np, "ext_bl,blmap_size",
			&pdata->blmap_size);

	if (pdata->blmap_size) {
		array = kzalloc(sizeof(u32) * pdata->blmap_size, GFP_KERNEL);
		if (!array)
			return -ENOMEM;

		rc = of_property_read_u32_array(np, "ext_bl,blmap", array, pdata->blmap_size);
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

	pr_info("[ext_bl] %s gpio: %d, max_current: %d, min: %d, "
			"default: %d, max: %d, cabc : %d , blmap_size : %d\n",
			__func__, pdata->gpio,
			pdata->max_current,
			pdata->min_brightness,
			pdata->default_brightness,
			pdata->max_brightness,
			pdata->cabc_enable,
			pdata->blmap_size);

	return rc;
}

static struct backlight_ops ext_bl_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};

static int ext_bl_probe(struct i2c_client *i2c_dev,
		const struct i2c_device_id *id)
{
	struct backlight_platform_data *pdata;
	struct ext_bl_device *dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err;

	pr_info("[Display][ext_bl] %s: i2c probe start\n", __func__);

	if (&i2c_dev->dev.of_node) {
		pdata = devm_kzalloc(&i2c_dev->dev,
				sizeof(struct backlight_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		err = ext_bl_parse_dt(&i2c_dev->dev, pdata);
		if (err != 0)
			return err;
	} else {
		pdata = i2c_dev->dev.platform_data;
	}

	pr_info("[Display][ext_bl] %s: gpio = %d\n", __func__,pdata->gpio);
	if (pdata->gpio && gpio_request(pdata->gpio, "ext_bl reset") != 0) {
		return -ENODEV;
	}

	ext_bl_i2c_client = i2c_dev;

	dev = kzalloc(sizeof(struct ext_bl_device), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(&i2c_dev->dev, "fail alloc for ext_bl_device\n");
		return 0;
	}
	main_ext_bl_dev = dev;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;

	props.max_brightness = MAX_BRIGHTNESS_EXT_BL;
	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev,
			NULL, &ext_bl_bl_ops, &props);
	bl_dev->props.max_brightness = MAX_BRIGHTNESS_EXT_BL;
	bl_dev->props.brightness = DEFAULT_BRIGHTNESS;
	bl_dev->props.power = FB_BLANK_UNBLANK;

	dev->bl_dev = bl_dev;
	dev->client = i2c_dev;

	dev->gpio = pdata->gpio;
	dev->max_current = pdata->max_current;
	dev->min_brightness = pdata->min_brightness;
	dev->default_brightness = pdata->default_brightness;
	dev->max_brightness = pdata->max_brightness;
	dev->control_bank = pdata->control_bank;
	dev->bl_config = pdata->bl_config;
	dev->cur_sink_feedback = pdata->cur_sink_feedback;
	dev->boost_control = pdata->boost_control;
	dev->cabc_enable = pdata->cabc_enable;
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

	if (gpio_get_value(dev->gpio))
		backlight_status = BL_ON;
	else
		backlight_status = BL_OFF;

	i2c_set_clientdata(i2c_dev, dev);

	mutex_init(&dev->bl_mutex);

	err = device_create_file(&i2c_dev->dev, &dev_attr_ext_bl_level);
	err = device_create_file(&i2c_dev->dev,	&dev_attr_ext_bl_backlight_on_off);
	err = device_create_file(&i2c_dev->dev,	&dev_attr_ext_bl_fs_curr);
	err = device_create_file(&i2c_dev->dev,	&dev_attr_ext_bl_cabc);
	err = device_create_file(&i2c_dev->dev, &dev_attr_ext_bl_i2c_addr);
	err = device_create_file(&i2c_dev->dev, &dev_attr_ext_bl_i2c_data);

	return 0;
}

static int ext_bl_remove(struct i2c_client *i2c_dev)
{
	struct ext_bl_device *dev;
	int gpio = main_ext_bl_dev->gpio;

	pr_info("[Display][ext_bl] %s: ++\n", __func__);
	device_remove_file(&i2c_dev->dev, &dev_attr_ext_bl_level);
	device_remove_file(&i2c_dev->dev, &dev_attr_ext_bl_backlight_on_off);
	dev = (struct ext_bl_device *)i2c_get_clientdata(i2c_dev);
	backlight_device_unregister(dev->bl_dev);
	i2c_set_clientdata(i2c_dev, NULL);

	if (gpio_is_valid(gpio))
	{
		pr_info("[Display][ext_bl] %s: gpio %d free \n", __func__, gpio);
		gpio_free(gpio);
	}

	pr_info("[Display][ext_bl] %s: -- \n", __func__);
	return 0;
}

static struct of_device_id ext_bl_match_table[] = {
	{ .compatible = "ext_bl",},
	{ },
};

static struct i2c_driver main_ext_bl_driver = {
	.probe = ext_bl_probe,
	.remove = ext_bl_remove,
	.id_table = ext_bl_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ext_bl_match_table,
	},
};

static int __init lcd_backlight_init(void)
{
	static int err;
	err = i2c_add_driver(&main_ext_bl_driver);

	return err;
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("ext_bl Backlight Control");
MODULE_LICENSE("GPL");
