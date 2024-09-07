/* linux/drivers/usb/gadget/u_lgeusb.c
 *
 * Copyright (C) 2011,2012 LG Electronics Inc.
 * Author : Hyeon H. Park <hyunhui.park@lge.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "u_lgeusb.h"

#ifdef CONFIG_MACH_LGE
#include <soc/qcom/lge/board_lge.h>
#endif

#if defined(CONFIG_LGE_USB_DIAG_LOCK_SPR) || defined(CONFIG_LGE_USB_DIAG_LOCK_TRF) || defined(CONFIG_LGE_ONE_BINARY_SKU)
#include <soc/qcom/smem.h>
#include <linux/soc/qcom/smem.h>
#endif
#ifdef CONFIG_LGE_USB_GADGET_AUTORUN
static char model_string[32];
static char swver_string[32];
static char subver_string[32];
static char phoneid_string[32];
#endif

#define LGEUSB_STRING_ATTR(field, buffer)				\
static ssize_t								\
field ## _show(struct device *dev, struct device_attribute *attr,	\
	       char *buf)						\
{									\
	return snprintf(buf, PAGE_SIZE, "%s", buffer);			\
}									\
static ssize_t								\
field ## _store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	if (size >= sizeof(buffer))					\
	return -EINVAL;							\
	if (sscanf(buf, "%31s", buffer) == 1) {				\
		return size;						\
	}								\
	return -ENODEV;							\
}									\
static DEVICE_ATTR(field, S_IRUGO | S_IWUSR, field ## _show, field ## _store);

#ifdef CONFIG_LGE_USB_GADGET_AUTORUN
LGEUSB_STRING_ATTR(model_name, model_string)
LGEUSB_STRING_ATTR(sw_version, swver_string)
LGEUSB_STRING_ATTR(sub_version, subver_string)
LGEUSB_STRING_ATTR(phone_id, phoneid_string)
#endif

static struct device_attribute *lgeusb_attributes[] = {
#ifdef CONFIG_LGE_USB_GADGET_AUTORUN
	&dev_attr_model_name,
	&dev_attr_sw_version,
	&dev_attr_sub_version,
	&dev_attr_phone_id,
#endif
	NULL
};

#ifdef CONFIG_LGE_USB_GADGET_AUTORUN
int lgeusb_get_model_name(char *model, size_t size)
{
	if (!model)
		return -EINVAL;

	strlcpy(model, model_string, size);
	pr_info("lgeusb: model name %s\n", model);
	return 0;
}

int lgeusb_get_phone_id(char *phoneid, size_t size)
{
	if (!phoneid)
		return -EINVAL;

	strlcpy(phoneid, phoneid_string, size);
	pr_info("lgeusb: phoneid %s\n", phoneid);
	return 0;
}

int lgeusb_get_sw_ver(char *sw_ver, size_t size)
{
	if (!sw_ver)
		return -EINVAL;

	strlcpy(sw_ver, swver_string, size);
	pr_info("lgeusb: sw version %s\n", sw_ver);
	return 0;
}

int lgeusb_get_sub_ver(char *sub_ver, size_t size)
{
	if (!sub_ver)
		return -EINVAL;

	strlcpy(sub_ver, subver_string, size);
	pr_info("lgeusb: sw sub version %s\n", sub_ver);
	return 0;
}
#endif

static int lgeusb_probe(struct platform_device *pdev)
{
	struct device_attribute **attrs = lgeusb_attributes;
	struct device_attribute *attr;
	int ret;

	while ((attr = *attrs++)) {
		ret = device_create_file(&pdev->dev, attr);
		if (ret) {
			pr_err("lgeusb: error on creating device file %s\n",
			       attr->attr.name);
			return ret;
		}
	}
	return 0;
}

#ifdef CONFIG_LGE_USB_DIAG_LOCK
#define DIAG_DISABLE 0
#define DIAG_ENABLE 1

int user_diag_enable = DIAG_DISABLE;

#if defined(CONFIG_LGE_USB_DIAG_LOCK_SPR) || defined(CONFIG_LGE_USB_DIAG_LOCK_TRF) || defined(CONFIG_LGE_ONE_BINARY_SKU)
typedef struct {
	int hw_rev;
	char model_name[10];
	// LGE_ONE_BINARY ???
	char diag_enable;
} lge_hw_smem_id0_type;

static int lge_diag_get_smem_value(void)
{
	struct _smem_id_vendor0 {
			unsigned int hw_rev;
			char model_name[10]; /* MODEL NAME */
#if defined(CONFIG_LGE_USB_DIAG_LOCK_TRF)
			uint32_t golden_copy_available;
#endif
			char ntcode[2048];
			char lg_modem_name[20];
			unsigned int sim_num;
		    char sku_carrier[8];	
		    char default_operator[10];	
		    unsigned int gpio_value;
			char svn_val[4];
			char diag_enable;

	} *smem_id_vendor0 = NULL;

	int smem_size = 0;

	smem_id_vendor0 = smem_get_entry(SMEM_ID_VENDOR0, &smem_size, 0, 0);

	if (IS_ERR_OR_NULL(smem_id_vendor0)) {
		pr_warn("Can't find SMEM_ID_VENDOR0; falling back on dummy values.\n");
		return 0;
	}

//	WARN(sizeof(struct _smem_id_vendor0) != smem_size,
//		 "size of smem_id_vendor0 is not correct. (%lu/%d)\n",
//		 sizeof(struct _smem_id_vendor0), smem_size);

	pr_info("%s: diag_enable(%d) svn_val(%s) gpio_value(%d) default_operator(%s) sku_carrier(%s) sim_num(%d) lg_modem_name(%s) ntcode(%s) model_name(%s) hw_rev(%d)\n", __func__,
		smem_id_vendor0->diag_enable, smem_id_vendor0->svn_val, smem_id_vendor0->gpio_value, smem_id_vendor0->default_operator, smem_id_vendor0->sku_carrier, smem_id_vendor0->sim_num, smem_id_vendor0->lg_modem_name, smem_id_vendor0->ntcode, smem_id_vendor0->model_name, smem_id_vendor0->hw_rev);

	return smem_id_vendor0->diag_enable;

}
#endif

int get_diag_enable(void)
{
#if defined(CONFIG_LGE_USB_FACTORY) && !defined(CONFIG_LGE_USB_DIAG_LOCK_SPR) && \
    !defined(CONFIG_LGE_USB_DIAG_LOCK_TRF)
#if defined(CONFIG_LGE_ONE_BINARY_SKU)
	if (lge_get_laop_operator() != OP_SPR_US && lge_get_laop_operator() != OP_DISH)
#endif
	{
	    if (lge_get_factory_boot())
		    user_diag_enable = DIAG_ENABLE;
	}

#endif
	//user_diag_enable = DIAG_ENABLE;

	return user_diag_enable;
}
EXPORT_SYMBOL(get_diag_enable);

#if !defined(CONFIG_LGE_USB_DIAG_LOCK_TRF)
static ssize_t read_diag_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%d", user_diag_enable);

	return ret;
}
static ssize_t write_diag_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	unsigned char string[2];

	if (sscanf(buf, "%1s", string) != 1)
		return -EINVAL;

	if (!strncmp(string, "0", 1))
	{
		user_diag_enable = 0;
	}
	else
	{
		user_diag_enable = 1;
	}

	pr_err("[%s] diag_enable: %d\n", __func__, user_diag_enable);

	return size;
}
static DEVICE_ATTR(diag_enable, S_IRUGO | S_IWUSR,
					read_diag_enable, write_diag_enable);
int lg_diag_create_file(struct platform_device *pdev)
{
	int ret;

	ret = device_create_file(&pdev->dev, &dev_attr_diag_enable);
	if (ret) {
		device_remove_file(&pdev->dev, &dev_attr_diag_enable);
		return ret;
	}
	return ret;
}


int lg_diag_remove_file(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_diag_enable);
	return 0;
}

static int lg_diag_cmd_probe(struct platform_device *pdev)
{
	int ret;
	ret = lg_diag_create_file(pdev);

	return ret;
}

static int lg_diag_cmd_remove(struct platform_device *pdev)
{
	lg_diag_remove_file(pdev);

	return 0;
}

static struct platform_driver lg_diag_cmd_driver = {
	.probe		= lg_diag_cmd_probe,
	.remove		= lg_diag_cmd_remove,
	.driver		= {
		.name = "lg_diag_cmd",
		.owner	= THIS_MODULE,
	},
};
#endif // !defined(CONFIG_LGE_USB_DIAG_LOCK_TRF)
#endif

static struct platform_driver lgeusb_driver = {
	.probe          = lgeusb_probe,
	.driver         = {
		.name = "lge_android_usb",
		.owner  = THIS_MODULE,
	},
};

static struct platform_device lgeusb_device = {
	.name = "lge_android_usb",
	.id = -1,
};

static int __init lgeusb_init(void)
{
	int rc;

	rc = platform_device_register(&lgeusb_device);
	if (rc) {
		pr_err("%s: platform_device_register fail\n", __func__);
		return rc;
	}
#if defined(CONFIG_LGE_USB_DIAG_LOCK) && !defined(CONFIG_LGE_USB_DIAG_LOCK_TRF)
	platform_driver_register(&lg_diag_cmd_driver);
#endif
#if defined(CONFIG_LGE_USB_DIAG_LOCK_TRF)
    user_diag_enable = lge_diag_get_smem_value();
#elif defined(CONFIG_LGE_USB_DIAG_LOCK_SPR) || defined(CONFIG_LGE_ONE_BINARY_SKU)
if (lge_get_laop_operator() == OP_SPR_US || lge_get_laop_operator() == OP_DISH)
	user_diag_enable = lge_diag_get_smem_value();
#endif
	rc = platform_driver_register(&lgeusb_driver);
	if (rc) {
		pr_err("%s: platform_driver_register fail\n", __func__);
		platform_device_unregister(&lgeusb_device);
		return rc;
	}

	return 0;
}
module_init(lgeusb_init);
