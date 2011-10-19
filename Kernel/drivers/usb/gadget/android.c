/*
 * Gadget Driver for Android
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/platform_device.h>

#include <linux/usb/android_composite.h>
#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/switch.h>
#include <mach/param.h>

#include "gadget_chips.h"

/*
 * Kbuild is not very cooperative with respect to linking separately
 * compiled library objects into one module.  So for now we won't use
 * separate compilation ... ensuring init/exit sections work to shrink
 * the runtime footprint, and giving us at least some parts of what
 * a "gcc --combine ... part1.c part2.c part3.c ... " build would.
 */
#include "usbstring.c"
#include "config.c"
#include "epautoconf.c"

#define _SUPPORT_SAMSUNG_AUTOINSTALLER_
#define CSY_USE_SAFE_USB_SWITCH
/* soonyong.cho : If usb switch can call usb cable handler safely,
 *		  you don't need to turn on usb udc.
 */

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : refer product id and config string of usb from 'arch/arm/plat-samsung/include/plat/devs.h' */
#  include <plat/devs.h>
#endif
#include "composite.c"

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : Added functions and modifed composite framework for samsung composite.
 *                Developers can make custom composite easily using this custom samsung framework.
 */
MODULE_AUTHOR("SoonYong Cho");
#else
MODULE_AUTHOR("Mike Lockwood");
#endif
MODULE_DESCRIPTION("Android Composite USB Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

int currentusbstatus;
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
int askon_status;
#endif

static const char longname[] = "Gadget Android";

 #if defined(CONFIG_MACH_VICTORY)	
extern void s5p_set_otg_dvfs(int);
static int g_tethering;
#endif
/* Default vendor and product IDs, overridden by platform data */
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
#  define VENDOR_ID		0x04e8	/* SAMSUNG */
/* soonyong.cho : default product id refered as <plat/devs.h> */
#  define PRODUCT_ID		SAMSUNG_DEBUG_PRODUCT_ID
#else /* Original VID & PID */
#  define VENDOR_ID		0x18D1
#  define PRODUCT_ID		0x0001
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */

struct android_dev {
	struct usb_composite_dev *cdev;
	struct usb_configuration *config;
	int num_products;
	struct android_usb_product *products;
	int num_functions;
	char **functions;

	int product_id;
	int version;
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
	int current_usb_mode;   /* soonyong.cho : save usb mode except tethering and askon mode. */
	int requested_usb_mode; /*                requested usb mode from app included tethering and askon */
	int debugging_usb_mode; /*		  debugging usb mode */
#endif

};

static struct android_dev *_android_dev;

/* string IDs are assigned dynamically */

#define STRING_MANUFACTURER_IDX		0
#define STRING_PRODUCT_IDX		1
#define STRING_SERIAL_IDX		2

/* String Table */
static struct usb_string strings_dev[] = {
	/* These dummy values should be overridden by platform data */
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : Samsung string default product id refered as <plat/devs.h> */
	[STRING_MANUFACTURER_IDX].s = "SAMSUNG",
	[STRING_PRODUCT_IDX].s = "SAMSUNG_Android",
	[STRING_SERIAL_IDX].s = "C110_Android",
#else /* Original */
	[STRING_MANUFACTURER_IDX].s = "Android",
	[STRING_PRODUCT_IDX].s = "Android",
	[STRING_SERIAL_IDX].s = "0123456789ABCDEF",
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

static struct usb_device_descriptor device_desc = {
	.bLength              = sizeof(device_desc),
	.bDescriptorType      = USB_DT_DEVICE,
	.bcdUSB               = __constant_cpu_to_le16(0x0200),
	.bDeviceClass         = USB_CLASS_PER_INTERFACE,
	.idVendor             = __constant_cpu_to_le16(VENDOR_ID),
	.idProduct            = __constant_cpu_to_le16(PRODUCT_ID),
	.bcdDevice            = __constant_cpu_to_le16(0xffff),
	.bNumConfigurations   = 1,
};

static struct list_head _functions = LIST_HEAD_INIT(_functions);
static int _registered_function_count = 0;

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
void samsung_enable_function(int mode);
#endif

extern void UsbIndicator(u8 state);

#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
void askon_gadget_disconnect()
{
        struct android_dev *dev = _android_dev;
        printk("Inside askon_gadget_disconnect ASKON VALUE =%d\n",askon_status);
         if(askon_status)
              usb_gadget_disconnect(dev->cdev->gadget);


}
EXPORT_SYMBOL(askon_gadget_disconnect);
#endif

static struct android_usb_function *get_function(const char *name)
{
	struct android_usb_function	*f;
	list_for_each_entry(f, &_functions, list) {
		if (!strcmp(name, f->name))
			return f;
	}
	return 0;
}

static void bind_functions(struct android_dev *dev)
{
	struct android_usb_function	*f;
	char **functions = dev->functions;
	int i;

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE /* soonyong.cho : Just review bind functions */
	list_for_each_entry(f, &_functions, list) {
		CSY_DBG("functions->name=%s\n", f->name);
	}

#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */
	for (i = 0; i < dev->num_functions; i++) {
		char *name = *functions++;
		f = get_function(name);
		if (f)
			f->bind_config(dev->config);
		else
			printk(KERN_ERR "function %s not found in bind_functions\n", name);
	}
}

static int android_bind_config(struct usb_configuration *c)
{
	struct android_dev *dev = _android_dev;

	printk(KERN_DEBUG "android_bind_config\n");
	dev->config = c;

	/* bind our functions if they have all registered */
	if (_registered_function_count == dev->num_functions)
		bind_functions(dev);

	return 0;
}

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : It is default config string. It'll be changed to real config string when last function driver is registered. */
#  define       ANDROID_DEFAULT_CONFIG_STRING "Samsung Android Shared Config"	/* android default config string */
#else /* original */
#  define	ANDROID_DEBUG_CONFIG_STRING "UMS + ADB (Debugging mode)"
#  define	ANDROID_NO_DEBUG_CONFIG_STRING "UMS Only (Not debugging mode)"
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */

static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl);

static struct usb_configuration android_config_driver = {
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : usb default config string */
	.label		= ANDROID_DEFAULT_CONFIG_STRING,
#else /* original */
	.label		= ANDROID_NO_DEBUG_CONFIG_STRING,
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */
	.bind		= android_bind_config,
	.setup		= android_setup_config,
	.bConfigurationValue = 1,
	.bmAttributes	= USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : This value of max power is referred from S1 */
	.bMaxPower	= 0x30, /* 96ma */
#else /* original */
	.bMaxPower	= 0xFA, /* 500ma */
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */

};

static int android_setup_config(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl)
{
	int i;
	int ret = -EOPNOTSUPP;
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : Do not call same function config when function has many interface.
 *                If another function driver has different config function, It needs calling.
 */
	char temp_name[128]={0,};
#endif
	CSY_DBG("\n");
	for (i = 0; i < android_config_driver.next_interface_id; i++) {
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE /* soonyong.cho : find same interface for to skip calling*/
		if (!android_config_driver.interface[i]->disabled && android_config_driver.interface[i]->setup) {
			if (!strcmp(temp_name, android_config_driver.interface[i]->name)) {
				CSY_DBG("[%d]skip name=%s\n",i, temp_name);
				continue;
			}
			else
				strcpy(temp_name,android_config_driver.interface[i]->name);
			CSY_DBG("[%d]name=%s enabled. and it has setup function. \n", i, android_config_driver.interface[i]->name);
#else
		if (android_config_driver.interface[i]->setup) {
#endif
			ret = android_config_driver.interface[i]->setup(
				android_config_driver.interface[i], ctrl);
			if (ret >= 0)
				return ret;
		}
	}
	return ret;
}

static int product_has_function(struct android_usb_product *p,
		struct usb_function *f)
{
	char **functions = p->functions;
	int count = p->num_functions;
	const char *name = f->name;
	int i;

	for (i = 0; i < count; i++) {
		if (!strcmp(name, *functions++))
			return 1;
	}
	return 0;
}

static int product_matches_functions(struct android_usb_product *p)
{
	struct usb_function		*f;
	list_for_each_entry(f, &android_config_driver.functions, list) {
		if (product_has_function(p, f) == !!f->disabled)
			return 0;
	}
	return 1;
}

static int get_product_id(struct android_dev *dev)
{
	struct android_usb_product *p = dev->products;
	int count = dev->num_products;
	int i;

	if (p) {
		for (i = 0; i < count; i++, p++) {
			if (product_matches_functions(p))
				return p->product_id;
		}
	}
	/* use default product ID */
	return dev->product_id;
}

static int android_bind(struct usb_composite_dev *cdev)
{
	struct android_dev *dev = _android_dev;
	struct usb_gadget	*gadget = cdev->gadget;
	int			gcnum, id, product_id, ret;

	printk(KERN_INFO "android_bind\n");

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_MANUFACTURER_IDX].id = id;
	device_desc.iManufacturer = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_PRODUCT_IDX].id = id;
	device_desc.iProduct = id;

	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_dev[STRING_SERIAL_IDX].id = id;
	device_desc.iSerialNumber = id;

	/* register our configuration */
	ret = usb_add_config(cdev, &android_config_driver);
	if (ret) {
		printk(KERN_ERR "usb_add_config failed\n");
		return ret;
	}

	gcnum = usb_gadget_controller_number(gadget);
	if (gcnum >= 0)
		//device_desc.bcdDevice = cpu_to_le16(0x0200 + gcnum);
		device_desc.bcdDevice = cpu_to_le16(0x0400);
	else {
		/* gadget zero is so simple (for now, no altsettings) that
		 * it SHOULD NOT have problems with bulk-capable hardware.
		 * so just warn about unrcognized controllers -- don't panic.
		 *
		 * things like configuration and altsetting numbering
		 * can need hardware-specific attention though.
		 */
		pr_warning("%s: controller '%s' not recognized\n",
			longname, gadget->name);
		device_desc.bcdDevice = __constant_cpu_to_le16(0x9999);
	}

	usb_gadget_set_selfpowered(gadget);
	dev->cdev = cdev;
	product_id = get_product_id(dev);
	device_desc.idProduct = __constant_cpu_to_le16(product_id);
	cdev->desc.idProduct = device_desc.idProduct;

	return 0;
}

static struct usb_composite_driver android_usb_driver = {
	.name		= "android_usb",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.bind		= android_bind,
	.enable_function = android_enable_function,
};

void android_register_function(struct android_usb_function *f)
{
	struct android_dev *dev = _android_dev;

	printk(KERN_INFO "android_register_function %s\n", f->name);
	list_add_tail(&f->list, &_functions);
	_registered_function_count++;

	/* bind our functions if they have all registered
	 * and the main driver has bound.
	 */
	CSY_DBG("name=%s, registered_function_count=%d, dev->num_functions=%d\n",f->name, _registered_function_count, dev->num_functions);
	if (dev && dev->config && _registered_function_count == dev->num_functions) {
		bind_functions(dev);
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : Change usb mode and enable usb ip when device register last function driver */
		samsung_enable_function( USBSTATUS_UMS);
#  ifdef CSY_USE_SAFE_USB_SWITCH
/* soonyong.cho : If usb switch can call usb cable handler safely, you don't need below code.
 *		  Below codes are used for to turn on always.
 *		  Do not enable udc. USB switch must call usb cable handler when cable status is changed.
 */
		CSY_DBG_ESS("Don't enable udc.\n");
#  else
		if(dev->cdev) {
			CSY_DBG("dev->cdev=0x%p\n", dev->cdev);
			if(dev->cdev->gadget) {
				CSY_DBG("dev->cdev->gadget=0x%p\n", dev->cdev->gadget);
				if(dev->cdev->gadget->ops) {
					CSY_DBG("dev->cdev->gadget->ops=0x%p\n", dev->cdev->gadget->ops);
					dev->cdev->gadget->ops->vbus_session(dev->cdev->gadget, 1);
					/* Enable USB when device binds every function driver */
				}
			}
		}
#  endif
#endif
	}
}

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/*
 * Description  : Set enable functions
 * Parameters   : char** functions (product function list), int num_f (number of product functions)
 * Return value : Count of enable functions
 *
 * Written by SoonYong,Cho  (Fri 5, Nov 2010)
 */
static int set_enable_functions(char **functions, int num_f)
{
	int i;
	struct usb_function		*func;
	int find = false;
	int count = 0;
	char **head_functions = functions;

	list_for_each_entry(func, &android_config_driver.functions, list) {

		CSY_DBG2("func->name=%s\n", func->name);
		functions = head_functions;
		for(i = 0; i < num_f; i++) {
			/* enable */
			if (!strcmp(func->name, *functions++)) {
				usb_function_set_enabled(func, 1);
				find = true;
				++count;
				CSY_DBG_ESS("enable %s\n", func->name);
				break;
			}
		}
		/* disable */
		if(find == false) {
			usb_function_set_enabled(func, 0);
			CSY_DBG_ESS("disable %s\n", func->name);
		}
		else /* fineded */
			find = false;
	}
	return count;
}

/*
 * Description  : Set product using function as set_enable_function
 * Parameters   : struct android_dev *dev (Refer dev->products), __u16 mode (usb mode)
 * Return Value : -1 (fail to find product), positive value (number of functions)
 *
 * Written by SoonYong,Cho  (Fri 5, Nov 2010)
 */
static int set_product(struct android_dev *dev, __u16 mode)
{
	struct android_usb_product *p = dev->products;
	int count = dev->num_products;
	int i, ret;

	dev->requested_usb_mode = mode; /* Save usb mode always even though it will be failed */

	if (p) {
		for (i = 0; i < count; i++, p++) {
			if(p->mode == mode) {
				/* It is for setting dynamic interface in composite.c */
				dev->cdev->product_num		= p->num_functions;
				dev->cdev->products		= p;

				dev->cdev->desc.bDeviceClass	 = p->bDeviceClass;
				dev->cdev->desc.bDeviceSubClass	 = p->bDeviceSubClass;
				dev->cdev->desc.bDeviceProtocol	 = p->bDeviceProtocol;
				android_config_driver.label	 = p->s;

				ret = set_enable_functions(p->functions, p->num_functions);
				CSY_DBG_ESS("Change Device Descriptor : DeviceClass(0x%x),SubClass(0x%x),Protocol(0x%x)\n",
					p->bDeviceClass, p->bDeviceSubClass, p->bDeviceProtocol);
				CSY_DBG_ESS("Change Label : [%d]%s\n", i, p->s);
				if(ret == 0)
					CSY_DBG("Can't find functions(mode=0x%x)\n", mode);
				else
					CSY_DBG("set function num=%d\n", ret);
				return ret;
			}
		}
	}
	else
		CSY_DBG_ESS("dev->products is not available\n");

	CSY_DBG_ESS("mode=0x%x is not available\n",mode);
	return -1;
}

#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
bool IsKiesCurrentUsbStatus(void)
{
        if( (currentusbstatus ==  USBSTATUS_SAMSUNG_KIES)
        || (currentusbstatus == USBSTATUS_SAMSUNG_KIES_REAL)
        ) {
                return true;
        }

        return false;
}
EXPORT_SYMBOL(IsKiesCurrentUsbStatus);
EXPORT_SYMBOL(currentusbstatus);
#endif

/*
 * Description  : Enable functions for samsung composite driver
 * Parameters   : struct usb_function *f (It depends on function's sysfs), int enable (1:enable, 0:disable)
 * Return value : void
 *
 * Written by SoonYong,Cho  (Fri 5, Nov 2010)
 */
void android_enable_function(struct usb_function *f, int enable)
{
	struct android_dev *dev = _android_dev;
	int product_id = 0;
	int ret = -1;
	CSY_DBG_ESS("++ f->name=%s enable=%d\n", f->name, enable);

	printk(KERN_INFO "(android: %s) f->name=%s enable=%d \n", __func__,f->name, enable);
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_ 
	askon_status = 0;
#endif
	if(enable) {
		if (!strcmp(f->name, "acm")) {
			ret = set_product(dev, USBSTATUS_SAMSUNG_KIES);
			if (ret != -1)
				dev->current_usb_mode = USBSTATUS_SAMSUNG_KIES;
			currentusbstatus =  dev->current_usb_mode; 
		}
		if (!strcmp(f->name, "adb")) {
			ret = set_product(dev, USBSTATUS_ADB);
			if (ret != -1)
				dev->debugging_usb_mode = 1; /* save debugging status */
			currentusbstatus =  USBSTATUS_ADB ;//dev->current_usb_mode; 
		}
		if (!strcmp(f->name, "mtp")) {
			ret = set_product(dev, USBSTATUS_MTPONLY);
			if (ret != -1)
				dev->current_usb_mode = USBSTATUS_MTPONLY;
			currentusbstatus =  dev->current_usb_mode; 
		}
		if (!strcmp(f->name, "rndis")) {
			ret = set_product(dev, USBSTATUS_VTP);
		}
		if (!strcmp(f->name, "usb_mass_storage")) {
			ret = set_product(dev, USBSTATUS_UMS);
			if (ret != -1)
				dev->current_usb_mode = USBSTATUS_UMS;
			currentusbstatus =  dev->current_usb_mode; 
		}

	}
	else { 
                  //dev->current_usb_mode = USBSTATUS_UMS;
                /* for disable : Return old mode. If Non-GED model changes policy, below code has to be modified. */
		if (!strcmp(f->name, "rndis") && dev->debugging_usb_mode)
			ret = set_product(dev, USBSTATUS_ADB);
		else{
			ret = set_product(dev, dev->current_usb_mode);
			if (!strcmp(f->name, "adb"))
				currentusbstatus =  0;//dev->current_usb_mode; 
		}

		if(!strcmp(f->name, "adb")) {
			dev->debugging_usb_mode = 0;
		}

	} /* if(enable) */

	if(ret == -1) {
		CSY_DBG_ESS("Can't find product. It is not changed !\n");
		return ;
	}

	product_id = get_product_id(dev);
	device_desc.idProduct = __constant_cpu_to_le16(product_id);

	if (dev->cdev)
		dev->cdev->desc.idProduct = device_desc.idProduct;

	/* force reenumeration */
	CSY_DBG_ESS("dev->cdev=0x%p, dev->cdev->gadget=0x%p, dev->cdev->gadget->speed=0x%x, mode=%d\n",
		dev->cdev, dev->cdev->gadget, dev->cdev->gadget->speed, dev->current_usb_mode);
	usb_composite_force_reset(dev->cdev);

	CSY_DBG_ESS("finished setting pid=0x%x currentusbstatus: %d \n",product_id,currentusbstatus);
}

#else /* original code */
void android_enable_function(struct usb_function *f, int enable)
{
	struct android_dev *dev = _android_dev;
	int disable = !enable;
	int product_id;

	if (!!f->disabled != disable) {
		usb_function_set_enabled(f, !disable);

	if (!strcmp(f->name, "adb"))
	{
		if (enable)
			android_config_driver.label = ANDROID_DEBUG_CONFIG_STRING;
		else
			android_config_driver.label = ANDROID_NO_DEBUG_CONFIG_STRING;
	}

#ifdef CONFIG_USB_ANDROID_RNDIS
		if (!strcmp(f->name, "rndis")) {
			struct usb_function		*func;

			/* We need to specify the COMM class in the device descriptor
			 * if we are using RNDIS.
			 */
			if (enable)
#ifdef CONFIG_USB_ANDROID_RNDIS_WCEIS
				dev->cdev->desc.bDeviceClass = USB_CLASS_WIRELESS_CONTROLLER;
#else
				dev->cdev->desc.bDeviceClass = USB_CLASS_COMM;
#endif
			else
				dev->cdev->desc.bDeviceClass = USB_CLASS_PER_INTERFACE;

			/* Windows does not support other interfaces when RNDIS is enabled,
			 * so we disable UMS and MTP when RNDIS is on.
			 */
			list_for_each_entry(func, &android_config_driver.functions, list) {
				if (!strcmp(func->name, "usb_mass_storage")
					|| !strcmp(func->name, "mtp")) {
					usb_function_set_enabled(func, !enable);
				}
			}
		}
#endif

		product_id = get_product_id(dev);
		device_desc.idProduct = __constant_cpu_to_le16(product_id);
		if (dev->cdev)
			dev->cdev->desc.idProduct = device_desc.idProduct;
		usb_composite_force_reset(dev->cdev);
	}
}
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */



/*
 * Description  : Enable functions for samsung composite driver using mode
 * Parameters   : int mode (Static mode number such as KIES, UMS, MTP, etc...)
 * Return value : void
 *
 * Written by SoonYong,Cho  (Fri 5, Nov 2010)
 */
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
void samsung_enable_function(int mode)
{
	struct android_dev *dev = _android_dev;
	int product_id = 0;
	int ret = -1;
	CSY_DBG_ESS("enable mode=0x%x (function: %s)\n",mode,__func__);

	switch(mode) {
		case USBSTATUS_UMS:
			CSY_DBG_ESS("mode = USBSTATUS_UMS (0x%x)\n", mode);
			ret = set_product(dev, USBSTATUS_UMS);
			break;
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_ 
		case USBSTATUS_SAMSUNG_KIES:
			CSY_DBG_ESS("mode = USBSTATUS_SAMSUNG_KIES (0x%x)\n", mode);
			ret = set_product(dev, USBSTATUS_SAMSUNG_KIES);
			break;
		case USBSTATUS_SAMSUNG_KIES_REAL:
			CSY_DBG_ESS("mode = USBSTATUS_SAMSUNG_KIES (0x%x)\n", mode);
			ret = set_product(dev, USBSTATUS_SAMSUNG_KIES);
			break;
#else
		case USBSTATUS_SAMSUNG_KIES:
			CSY_DBG_ESS("mode = USBSTATUS_SAMSUNG_KIES (0x%x)\n", mode);
			ret = set_product(dev, USBSTATUS_SAMSUNG_KIES);
			break;
#endif //  _SUPPORT_SAMSUNG_AUTOINSTALLER_
		case USBSTATUS_MTPONLY:
			CSY_DBG_ESS("mode = USBSTATUS_MTPONLY (0x%x)\n", mode);
			ret = set_product(dev, USBSTATUS_MTPONLY);
			break;
		case USBSTATUS_ADB:
			CSY_DBG_ESS("mode = USBSTATUS_ADB (0x%x)\n", mode);
			ret = set_product(dev, USBSTATUS_ADB);
			break;
		case USBSTATUS_VTP: /* do not save usb mode */
			CSY_DBG_ESS("mode = USBSTATUS_VTP (0x%x)\n", mode);
			ret = set_product(dev, USBSTATUS_VTP);
			break;
		case USBSTATUS_ASKON: /* do not save usb mode */
			CSY_DBG_ESS("mode = USBSTATUS_ASKON (0x%x) Don't change usb mode\n", mode);
			return;
	}

	if(ret == -1) {
		CSY_DBG_ESS("Can't find product. It is not changed !\n");
		return ;
	}
	else if((mode != USBSTATUS_VTP) && (mode != USBSTATUS_ASKON)) {
		CSY_DBG_ESS("Save usb mode except tethering and askon (mode=%d)\n", mode);
		dev->current_usb_mode = mode;
	}

	currentusbstatus =  mode;

	product_id = get_product_id(dev);
	device_desc.idProduct = __constant_cpu_to_le16(product_id);

	if (dev->cdev)
		dev->cdev->desc.idProduct = device_desc.idProduct;

	/* force reenumeration */
	printk("dev->cdev=0x%p, dev->cdev->gadget=0x%p, dev->cdev->gadget->speed=0x%x, mode=%d\n",
		dev->cdev, dev->cdev->gadget, dev->cdev->gadget->speed, dev->current_usb_mode);
	usb_composite_force_reset(dev->cdev);

	CSY_DBG_ESS("finished setting pid=0x%x currentusbstate: %d (func: %s)\n",product_id, currentusbstatus,__func__);
}
EXPORT_SYMBOL(samsung_enable_function);
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : sysfs for to show status of tethering switch
 *                Path (/sys/devices/platform/android_usb/tethering)
 */
static ssize_t tethering_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct android_dev *a_dev = _android_dev;
	int value = -1;
	if(a_dev->cdev) {
	 #if defined(CONFIG_MACH_VICTORY)	
		if(g_tethering ==1)
			value = 1;
		else
			value = 0;
	#else
		if (a_dev->cdev->product_num == SAMSUNG_RNDIS_PRODUCT_ID )
			value = 1;
		else
			value = 0;
	#endif
	}
	else {
		CSY_DBG("Fail to show tethering switch. dev->cdev is not valid\n");
	}
	return sprintf(buf, "%d\n", value);
}

/* soonyong.cho : sysfs for to change status of tethering switch
 *                Path (/sys/devices/platform/android_usb/tethering)
 */
static ssize_t tethering_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	struct android_dev *a_dev = _android_dev;
	sscanf(buf, "%d", &value);

	if (value) {
		printk("Enable tethering\n");
		 #if defined(CONFIG_MACH_VICTORY)
			g_tethering = 1;
                        s5p_set_otg_dvfs(1);
		#endif
		samsung_enable_function(USBSTATUS_VTP);
	}
	else {
		printk("Disable tethering\n");
		 #if defined(CONFIG_MACH_VICTORY)
			g_tethering = 0;
                        s5p_set_otg_dvfs(0);
		#endif
		if(a_dev->debugging_usb_mode)
			samsung_enable_function(USBSTATUS_ADB);
		else
			samsung_enable_function(a_dev->current_usb_mode);
	}

	return size;
}

/* soonyong.cho : attribute of sysfs for tethering switch */
static DEVICE_ATTR(tethering, 0664, tethering_switch_show, tethering_switch_store);

#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
static int kies_status = 0;
static ssize_t KiesStatus_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if(kies_status == 1)
		return sprintf(buf, "%s\n", "START");
	else if( kies_status == 2)
		return sprintf(buf, "%s\n", "STOP");
	else
		return sprintf(buf, "%s\n", "INIT");
}
static ssize_t KiesStatus_switch_store(struct device *dev, struct device_attribute *attr,       const char *buf, size_t size)
{
	printk(KERN_INFO "KiesStatus switch store buf=%s\n", buf);

	if (strncmp(buf, "START", 5) == 0 ){
		kies_status = 1;
	}else if (strncmp(buf, "STOP", 4) == 0){
		kies_status = 2;
		UsbIndicator(2);
	}else if (strncmp(buf, "INIT", 4) == 0){
		kies_status = 0;
	}

	return size;
}
static DEVICE_ATTR(KiesStatus, 0664, KiesStatus_switch_show, KiesStatus_switch_store);
#endif /* _SUPPORT_SAMSUNG_AUTOINSTALLER_ */

/* soonyong.cho : sysfs for to show status of usb config
 *                Path (/sys/devices/platform/android_usb/UsbMenuSel)
 */
static ssize_t UsbMenuSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct android_dev *a_dev = _android_dev;
	int value = -1;

	if(a_dev->cdev) {
		CSY_DBG_ESS("product num = %d\n", a_dev->cdev->product_num);
		switch(a_dev->requested_usb_mode) {
			case USBSTATUS_UMS:
				return sprintf(buf, "[UsbMenuSel] UMS\n");
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
			case USBSTATUS_SAMSUNG_KIES:
				return sprintf(buf, "[UsbMenuSel] UMS_CDFS\n");
			case USBSTATUS_SAMSUNG_KIES_REAL:
				return sprintf(buf, "[UsbMenuSel] ACM_MTP\n");
#else
			case USBSTATUS_SAMSUNG_KIES:
				return sprintf(buf, "[UsbMenuSel] ACM_MTP\n");
#endif
			case USBSTATUS_MTPONLY:
				return sprintf(buf, "[UsbMenuSel] MTP\n");
			case USBSTATUS_ASKON:
				return sprintf(buf, "[UsbMenuSel] ASK\n");
			case USBSTATUS_VTP:
				return sprintf(buf, "[UsbMenuSel] TETHERING\n");
			case USBSTATUS_ADB:
				return sprintf(buf, "[UsbMenuSel] ACM_ADB_UMS\n");
		}
	}
	else {
		CSY_DBG("Fail to show usb menu switch. dev->cdev is not valid\n");
	}

	return sprintf(buf, "%d\n", value);
}

#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
static int UsbMenuSelStore(int sel)
{
        int switch_sel, ret;

        if (sec_get_param_value)
                sec_get_param_value(__SWITCH_SEL, &switch_sel);

        if(sel == 0){
                switch_sel &= ~(int)USB_UMS_MASK;
                switch_sel &= ~(int)USB_MTP_MASK;
                switch_sel &= ~(int)USB_VTP_MASK;
                switch_sel &= ~(int)USB_ASKON_MASK;
                switch_sel &= ~(int)USB_SAMSUNG_KIES_REAL_MASK;
                switch_sel |= (int)USB_SAMSUNG_KIES_MASK;
                }
        else if(sel == 1){
                switch_sel &= ~(int)USB_UMS_MASK;
               switch_sel &= ~(int)USB_SAMSUNG_KIES_REAL_MASK;
                switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
                switch_sel &= ~(int)USB_VTP_MASK;
                switch_sel &= ~(int)USB_ASKON_MASK;
                switch_sel |= (int)USB_MTP_MASK;
                }
        else if(sel == 2){
                switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
                switch_sel &= ~(int)USB_MTP_MASK;
                switch_sel &= ~(int)USB_VTP_MASK;
                switch_sel &= ~(int)USB_ASKON_MASK;
              switch_sel &= ~(int)USB_SAMSUNG_KIES_REAL_MASK;
                switch_sel |= (int)USB_UMS_MASK;
                }
        else if(sel == 3){
                switch_sel &= ~(int)USB_UMS_MASK;
                switch_sel &= ~(int)USB_MTP_MASK;
                switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
               switch_sel &= ~(int)USB_ASKON_MASK;
                switch_sel &= ~(int)USB_SAMSUNG_KIES_REAL_MASK;
                switch_sel |= (int)USB_VTP_MASK;
                }
        else if(sel == 4){
                switch_sel &= ~(int)USB_UMS_MASK;
                switch_sel &= ~(int)USB_MTP_MASK;
                switch_sel &= ~(int)USB_VTP_MASK;
                switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
                switch_sel &= ~(int)USB_SAMSUNG_KIES_REAL_MASK;
                switch_sel |= (int)USB_ASKON_MASK;
                }
                // KIES_REAL
        else if(sel == 5){
                        switch_sel &= ~(int)USB_UMS_MASK;
                        switch_sel &= ~(int)USB_MTP_MASK;
                        switch_sel &= ~(int)USB_VTP_MASK;
                        switch_sel &= ~(int)USB_ASKON_MASK;
                        switch_sel &= ~(int)USB_SAMSUNG_KIES_MASK;
                        switch_sel |= (int)USB_SAMSUNG_KIES_REAL_MASK;
                }
        if (sec_set_param_value)
                sec_set_param_value(__SWITCH_SEL, &switch_sel);

        printk("\n[WJ] %s, %s, switch_sel=%d\n", __FILE__, __FUNCTION__, switch_sel);

        // returns current USB Mode setting...
        ret = switch_sel;
        ret &= ~(UART_SEL_MASK|USB_SEL_MASK);

        printk("ret = 0x%x\n", ret);

        return ret;
}
#endif


/* soonyong.cho : sysfs for to change status of usb config
 *                Path (/sys/devices/platform/android_usb/UsbMenuSel)
 */
static ssize_t UsbMenuSel_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	sscanf(buf, "%d", &value);
             CSY_DBG_ESS("UsbMenuSel_switch_store =(%s)\n", buf);

	if (strncmp(buf, "KIES", 4) == 0){
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
                        printk("Enable KIES(%d)\n", value);
                       askon_status = 0;
                       UsbMenuSelStore(0);
#endif
                        CSY_DBG_ESS("Enable KIES(%d)\n", value);
                        samsung_enable_function(USBSTATUS_SAMSUNG_KIES);
                }
                if (strncmp(buf, "MTP", 3) == 0){
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
                        printk("Enable MTP(%d)\n", value);
                       askon_status = 0;
                       UsbMenuSelStore(1);
#endif
                        CSY_DBG_ESS("Enable MTP(%d)\n", value);
                        samsung_enable_function(USBSTATUS_MTPONLY);
                }
                if (strncmp(buf, "UMS", 3) == 0){
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
                        printk("Enable UMS(%d)\n", value);
                       askon_status = 0;
                       UsbMenuSelStore(2);
#endif
                        CSY_DBG_ESS("Enable UMS(%d)\n", value);
                        samsung_enable_function(USBSTATUS_UMS);
                }
                if (strncmp(buf, "ASKON", 5) == 0){
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
                        printk("Enable ASKON(%d)\n", value);
                       askon_status = 1;
                       UsbMenuSelStore(4);
#endif
                        CSY_DBG_ESS("Enable ASKON(%d)\n", value);
                        samsung_enable_function(USBSTATUS_ASKON);
                }

	return size;
}

/* soonyong.cho : attribute of sysfs for usb menu switch */
static DEVICE_ATTR(UsbMenuSel, 0664, UsbMenuSel_switch_show, UsbMenuSel_switch_store);
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */

static ssize_t AskOnStatus_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
//	if(inaskonstatus || (usb_path == SWITCH_MODEM))
//		return sprintf(buf, "%s\n", "Blocking");
//	else
		return sprintf(buf, "%s\n", "NonBlocking");
}
static ssize_t AskOnStatus_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}
static DEVICE_ATTR(AskOnStatus, 0664, AskOnStatus_switch_show, AskOnStatus_switch_store);

static ssize_t AskOnMenuSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s[AskOnMenuSel] Port test ready!! \n", buf);
}

static ssize_t AskOnMenuSel_switch_store(struct device *dev, struct device_attribute *attr,     const char *buf, size_t size)
{
		printk(KERN_INFO "----> AskOnMenuSel :%s \n",buf);
	if (strncmp(buf, "KIES", 4) == 0){
		CSY_DBG_ESS("Enable KIES \n");
		samsung_enable_function(USBSTATUS_SAMSUNG_KIES);
	}
	if (strncmp(buf, "MTP", 3) == 0){
		CSY_DBG_ESS("Enable MTP \n");
		samsung_enable_function(USBSTATUS_MTPONLY);
	}
	if (strncmp(buf, "UMS", 3) == 0){
		CSY_DBG_ESS("Enable UMS \n");
		UsbIndicator(1);
		samsung_enable_function(USBSTATUS_UMS);
	}
	if (strncmp(buf, "VTP", 3) == 0){
		CSY_DBG_ESS("Enable VTP \n");
		samsung_enable_function(USBSTATUS_VTP);
	}
	return size;
}
static DEVICE_ATTR(AskOnMenuSel, 0664, AskOnMenuSel_switch_show, AskOnMenuSel_switch_store);

static int android_probe(struct platform_device *pdev)
{
	struct android_usb_platform_data *pdata = pdev->dev.platform_data;
	struct android_dev *dev = _android_dev;

	printk(KERN_INFO "android_probe pdata: %p\n", pdata);

	if (pdata) {
		dev->products = pdata->products;
		dev->num_products = pdata->num_products;
		dev->functions = pdata->functions;
		dev->num_functions = pdata->num_functions;
		if (pdata->vendor_id)
			device_desc.idVendor =
				__constant_cpu_to_le16(pdata->vendor_id);
		if (pdata->product_id) {
			dev->product_id = pdata->product_id;
			device_desc.idProduct =
				__constant_cpu_to_le16(pdata->product_id);
		}
		if (pdata->version)
			dev->version = pdata->version;

		if (pdata->product_name)
			strings_dev[STRING_PRODUCT_IDX].s = pdata->product_name;
		if (pdata->manufacturer_name)
			strings_dev[STRING_MANUFACTURER_IDX].s =
					pdata->manufacturer_name;
		if (pdata->serial_number)
			strings_dev[STRING_SERIAL_IDX].s = pdata->serial_number;
		CSY_DBG_ESS("vid=0x%x,pid=0x%x,ver=0x%x,product_name=%s,manufacturer_name=%s,serial=%s\n",
			pdata->vendor_id, pdata->product_id, pdata->version, pdata->product_name, pdata->manufacturer_name,
			pdata->serial_number);
	}
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : Create attribute of sysfs as '/sys/devices/platform/android_usb/UsbMenuSel'
 *                It is for USB menu selection.
 * 		  Application for USB Setting made by SAMSUNG uses property that uses below sysfs.
 */
	if (device_create_file(&pdev->dev, &dev_attr_UsbMenuSel) < 0)
		CSY_DBG("Failed to create device file(%s)!\n", dev_attr_UsbMenuSel.attr.name);

	if (device_create_file(&pdev->dev, &dev_attr_AskOnMenuSel) < 0)
                CSY_DBG("Failed to create device file(%s)!\n", dev_attr_AskOnMenuSel.attr.name);

	if (device_create_file(&pdev->dev, &dev_attr_AskOnStatus) < 0)
                CSY_DBG("Failed to create device file(%s)!\n", dev_attr_AskOnStatus.attr.name);

#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
	if (device_create_file(&pdev->dev, &dev_attr_KiesStatus) < 0)
                CSY_DBG("Failed to create device file(%s)!\n", dev_attr_KiesStatus.attr.name);
#endif

/* soonyong.cho : Create attribute of sysfs as '/sys/devices/platform/android_usb/tethering'
 *                It is for tethering menu. Netd controls usb setting when user click tethering menu.
 *                Actually netd is android open source project.
 *                And it did use sysfs as '/sys/class/usb_composite/rndis/enable'
 *                But SAMSUNG modified this path to '/sys/class/sec/switch/tethering' in S1 model.
 *
 *		  This driver I made supports both original sysfs and modified sysfs as
 *                '/sys/devices/platform/android_usb/tethering' for compatibility.
 *
 *                But old modified path as '/sys/class/sec/switch/tethering' is not available.
 *                You can refer netd source code in '/Android/system/netd/UsbController.cpp'
 */
	if (device_create_file(&pdev->dev, &dev_attr_tethering) < 0)
		CSY_DBG("Failed to create device file(%s)!\n", dev_attr_tethering.attr.name);

/* soonyong.cho : If you use usb switch and enable usb switch before to initilize final function driver,
 *		  it can be called as vbus_session function without to initialize product number
 *		  and present product. 
 *		  But, Best guide is that usb switch doesn't initialize before usb driver.
 *		  If you want initialize, please implement it.
 */
#endif

	return usb_composite_register(&android_usb_driver);
}

static struct platform_driver android_platform_driver = {
	.driver = { .name = "android_usb", },
	.probe = android_probe,
};

static int __init init(void)
{
	struct android_dev *dev;

	printk(KERN_INFO "android init\n");

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	/* set default values, which should be overridden by platform data */
	dev->product_id = PRODUCT_ID;
	_android_dev = dev;

	return platform_driver_register(&android_platform_driver);
}
module_init(init);

static void __exit cleanup(void)
{
	usb_composite_unregister(&android_usb_driver);
	platform_driver_unregister(&android_platform_driver);
	kfree(_android_dev);
	_android_dev = NULL;
}
module_exit(cleanup);
