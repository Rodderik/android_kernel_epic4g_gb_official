/*
 * UART/USB path switching driver for Samsung Electronics devices.
 *
 * Copyright (C) 2010 Samsung Electronics.
 *
 * Authors: Ikkeun Kim <iks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/fsa9480.h>
#include <linux/mfd/max8998.h>
#include <linux/regulator/consumer.h>
#include <linux/moduleparam.h>
#include <asm/mach/arch.h>
#include <mach/param.h>
#include <mach/gpio.h>
#if defined(CONFIG_MACH_ATLAS)
#include <mach/gpio-atlas.h>
#elif defined(CONFIG_MACH_FORTE)
#include <mach/gpio-forte.h>
#else 
#include <mach/gpio-victory.h>
#endif
#include <mach/sec_switch.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <asm/mach-types.h>
struct sec_switch_struct {
	struct sec_switch_platform_data *pdata;
	int switch_sel;
};

struct sec_switch_wq {
	struct delayed_work work_q;
	struct sec_switch_struct *sdata;
	struct list_head entry;
};

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
extern void samsung_enable_function(int mode);
#endif

#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_ 
extern int askon_status;
#endif


/* for sysfs control (/sys/class/sec/switch/) */
extern struct device *switch_dev;

#ifdef CONFIG_S5PV210_GARNETT_DELTA
extern unsigned int system_rev;
static ssize_t HWrevision_show(struct device *dev, struct device_attribute *attr, char *buf){
        return sprintf(buf, "%x\n", system_rev );
}

static ssize_t HWrevision_store(
                struct device *dev, struct device_attribute *attr,
                const char *buf, size_t size){
    printk(KERN_INFO "[SEC SWITCH]\n ");

        return 0;
}
/*sysfs for HW Board revision */
static DEVICE_ATTR(HWrevision, 0664, HWrevision_show, HWrevision_store);
#endif //CONFIG_S5PV210_GARNETT_DELTA


static void usb_switch_mode(struct sec_switch_struct *secsw, int mode)
{
	if (mode == SWITCH_PDA)	{
		if (secsw->pdata && secsw->pdata->set_vbus_status)
			secsw->pdata->set_vbus_status((u8)USB_VBUS_AP_ON);
		mdelay(10);
		fsa9480_manual_switching(SWITCH_PORT_AUTO);
	} else {
		if (secsw->pdata && secsw->pdata->set_vbus_status)
			secsw->pdata->set_vbus_status((u8)USB_VBUS_CP_ON);
		mdelay(10);
		if(machine_is_victory())
                fsa9480_manual_switching(SWITCH_PORT_AUDIO);
                else if (machine_is_atlas())
                fsa9480_manual_switching(SWITCH_PORT_VAUDIO);
                else if (machine_is_forte())
                fsa9480_manual_switching(SWITCH_PORT_AUDIO);
	}
}

static ssize_t uart_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int uart_sel = secsw->switch_sel & UART_SEL_MASK;
	return sprintf(buf, "%s UART\n", uart_sel ? "PDA" : "MODEM");
}

static ssize_t uart_sel_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	printk("uart_sel_store called\n");
	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &secsw->switch_sel);

	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0) {
		gpio_set_value(GPIO_UART_SEL, 1);
		secsw->switch_sel |= UART_SEL_MASK;
		pr_debug("[UART Switch] Path : PDA\n");
	}

	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {
		gpio_set_value(GPIO_UART_SEL, 0);
		#if defined(CONFIG_MACH_FORTE)
		printk("uart_sel_store: setting GPIO_UART_SEL1\n");
                gpio_set_value(GPIO_UART_SEL1, 0);
		#endif
		secsw->switch_sel &= ~UART_SEL_MASK;
		pr_debug("[UART Switch] Path : MODEM\n");
	}

	if (sec_set_param_value)
		sec_set_param_value(__SWITCH_SEL, &secsw->switch_sel);

	return size;
}

static ssize_t usb_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int usb_path = secsw->switch_sel & USB_SEL_MASK;
	return sprintf(buf, "%s USB\n", usb_path ? "PDA" : "MODEM");
}

static ssize_t usb_sel_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);

	if (sec_get_param_value)
		sec_get_param_value(__SWITCH_SEL, &secsw->switch_sel);

	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0) {
		usb_switch_mode(secsw, SWITCH_PDA);
		secsw->switch_sel |= USB_SEL_MASK;
	}

	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {
		usb_switch_mode(secsw, SWITCH_MODEM);
		secsw->switch_sel &= ~USB_SEL_MASK;
	}

	if (sec_set_param_value)
		sec_set_param_value(__SWITCH_SEL, &secsw->switch_sel);

	return size;
}

static ssize_t usb_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int cable_state = CABLE_TYPE_NONE;

	if (secsw->pdata && secsw->pdata->get_cable_status)
		cable_state = secsw->pdata->get_cable_status();

	return sprintf(buf, "%s\n", (cable_state == CABLE_TYPE_USB) ?
			"USB_STATE_CONFIGURED" : "USB_STATE_NOTCONFIGURED");
}

static ssize_t usb_state_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	return 0;
}

static ssize_t disable_vbus_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t disable_vbus_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(secsw->pdata) ||
	    IS_ERR_OR_NULL(secsw->pdata->set_vbus_status) ||
	    IS_ERR_OR_NULL(secsw->pdata->set_usb_gadget_vbus))
		return size;

	secsw->pdata->set_usb_gadget_vbus(false);
	secsw->pdata->set_vbus_status((u8)USB_VBUS_ALL_OFF);
	msleep(10);
	secsw->pdata->set_usb_gadget_vbus(true);

	return size;
}

static DEVICE_ATTR(uart_sel, 0664, uart_sel_show, uart_sel_store);
static DEVICE_ATTR(usb_sel, 0664, usb_sel_show, usb_sel_store);
static DEVICE_ATTR(usb_state, 0664, usb_state_show, usb_state_store);
static DEVICE_ATTR(disable_vbus, 0664, disable_vbus_show, disable_vbus_store);
extern int adb_enabled;
static void sec_switch_init_work(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct sec_switch_wq *wq = container_of(dw, struct sec_switch_wq, work_q);
	struct sec_switch_struct *secsw = wq->sdata;
	int usb_sel = 0;
	int uart_sel = 0;
	int samsung_kies_sel,ums_sel,mtp_sel,vtp_sel,askon_sel;

	if (sec_get_param_value &&
	    secsw->pdata &&
	    secsw->pdata->set_vbus_status &&
	    secsw->pdata->get_phy_init_status &&
	    secsw->pdata->get_phy_init_status()) {
		sec_get_param_value(__SWITCH_SEL, &secsw->switch_sel);
		cancel_delayed_work(&wq->work_q);
	} else {
		schedule_delayed_work(&wq->work_q, msecs_to_jiffies(1000));
		return;
	}

	pr_debug("%s : initial sec switch value = 0x%X\n", __func__, secsw->switch_sel);

	usb_sel = secsw->switch_sel & USB_SEL_MASK;
	uart_sel = secsw->switch_sel & UART_SEL_MASK;
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
        samsung_kies_sel = (secsw->switch_sel & (int)(USB_SAMSUNG_KIES_MASK)) >> 2;
        ums_sel = (secsw->switch_sel & (int)(USB_UMS_MASK)) >> 3;
        mtp_sel = (secsw->switch_sel & (int)(USB_MTP_MASK)) >> 4;
        vtp_sel = (secsw->switch_sel & (int)(USB_VTP_MASK)) >> 5;
        askon_sel = (secsw->switch_sel & (int)(USB_ASKON_MASK)) >> 6;
#endif


	/* init UART/USB path */
	if (usb_sel)
		usb_switch_mode(secsw, SWITCH_PDA);
	else
		usb_switch_mode(secsw, SWITCH_MODEM);

	if (uart_sel)
		gpio_set_value(GPIO_UART_SEL, 1);
	else
		{
			#if defined(CONFIG_MACH_FORTE)
			printk("sec_switch_init_work: setting GPIO_UART_SEL1\n");
                	gpio_set_value(GPIO_UART_SEL1, 0);
			#endif

			gpio_set_value(GPIO_UART_SEL, 0);
		}
#ifdef _SUPPORT_SAMSUNG_AUTOINSTALLER_
	if(!adb_enabled)
{

       if(samsung_kies_sel){
               printk("$$$$$$$$$$$$$$%s: samsung_kies_sel ON\n", __func__);
               samsung_enable_function(USBSTATUS_SAMSUNG_KIES);
       }
        else if(mtp_sel){
               printk("$$$$$$$$$$$$$$$$%s: mtp_sel ON\n", __func__);
               samsung_enable_function(USBSTATUS_MTPONLY);
        }
        else if(ums_sel){
               printk("$$$$$$$$$$$$$$$$%s: ums_sel ON\n", __func__);
               samsung_enable_function(USBSTATUS_UMS);
        }
        else if(vtp_sel){
               samsung_enable_function(USBSTATUS_VTP);
        }
        else if(askon_sel){
               printk("$$$$$$$$$$$$$$$$%s: askon_sel  ON\n", __func__);
               samsung_enable_function(USBSTATUS_ASKON);
               askon_status = 1;
        }

}
#endif
}

static int sec_switch_probe(struct platform_device *pdev)
{
	struct sec_switch_struct *secsw;
	struct sec_switch_platform_data *pdata = pdev->dev.platform_data;
	struct sec_switch_wq *wq;


	if (!pdata) {
		pr_err("%s : pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	secsw = kzalloc(sizeof(struct sec_switch_struct), GFP_KERNEL);
	if (!secsw) {
		pr_err("%s : failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	secsw->pdata = pdata;
	secsw->switch_sel = 1;

	dev_set_drvdata(switch_dev, secsw);

	/* create sysfs files */
	if (device_create_file(switch_dev, &dev_attr_uart_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_uart_sel.attr.name);

	if (device_create_file(switch_dev, &dev_attr_usb_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_sel.attr.name);

	if (device_create_file(switch_dev, &dev_attr_usb_state) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);

	if (device_create_file(switch_dev, &dev_attr_disable_vbus) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);

#ifdef	CONFIG_S5PV210_GARNETT_DELTA
	if (device_create_file(switch_dev, &dev_attr_HWrevision) < 0)
                pr_err("Failed to create device file(%s)!\n", dev_attr_HWrevision.attr.name);
#endif

	/* run work queue */
	wq = kmalloc(sizeof(struct sec_switch_wq), GFP_ATOMIC);
	if (wq) {
		wq->sdata = secsw;
		INIT_DELAYED_WORK(&wq->work_q, sec_switch_init_work);
		schedule_delayed_work(&wq->work_q, msecs_to_jiffies(100));
	} else
		return -ENOMEM;

	return 0;
}

static int sec_switch_remove(struct platform_device *pdev)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(&pdev->dev);
	kfree(secsw);
	return 0;
}

static struct platform_driver sec_switch_driver = {
	.probe = sec_switch_probe,
	.remove = sec_switch_remove,
	.driver = {
			.name = "sec_switch",
			.owner = THIS_MODULE,
	},
};

static int __init sec_switch_init(void)
{
	return platform_driver_register(&sec_switch_driver);
}

static void __exit sec_switch_exit(void)
{
	platform_driver_unregister(&sec_switch_driver);
}

module_init(sec_switch_init);
module_exit(sec_switch_exit);

MODULE_AUTHOR("Ikkeun Kim <iks.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung Electronics Corp Switch driver");
MODULE_LICENSE("GPL");
