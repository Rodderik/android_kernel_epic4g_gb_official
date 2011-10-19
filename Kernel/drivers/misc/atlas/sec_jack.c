/*
 *	JACK device detection driver.
 *
 *	Copyright (C) 2009 Samsung Electronics, Inc.
 *
 *	Authors:
 *		Uk Kim <w0806.kim@samsung.com>
 * 		Rakesh Gohel <rakesh.gohel@samsung.com> Major revision for new algorithm
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; version 2 of the License.
 */

#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <mach/gpio-atlas.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/irqs.h>
#include <asm/mach-types.h>
#include <mach/atlas/sec_jack.h>

#define CONFIG_DEBUG_SEC_JACK
#define SUBJECT "JACK_DRIVER"

#ifdef CONFIG_DEBUG_SEC_JACK
#define SEC_JACKDEV_DBG(format,...)\
	printk ("[ "SUBJECT " (%s,%d) ] " format "\n", __func__, __LINE__, ## __VA_ARGS__);

#else
#define DEBUG_LOG(format,...)
#endif

#define KEYCODE_SENDEND 248
#define WAKELOCK_DET_TIMEOUT	HZ * 5 //5 sec

static struct platform_driver sec_jack_driver;
extern unsigned int system_rev;

struct class *jack_class;
EXPORT_SYMBOL(jack_class);
static struct device *jack_selector_fs;				// Sysfs device, this is used for communication with Cal App.
EXPORT_SYMBOL(jack_selector_fs);

static unsigned int sendend_type; //0=short, 1=open

extern void wm8994_set_vps_related_output_path(int enable);
extern int s3c_adc_get_adc_data(int channel);

/* for short key time limit */
static u64 pressed_jiffies;
static u64 irq_jiffies;
#define SHORTKEY_MS			120
#define SHORTKEY_JIFFIES	((HZ / 10) * (SHORTKEY_MS / 100)) + (HZ / 100) * ((SHORTKEY_MS % 100) / 10)

static void send_end_press_work_handler(struct work_struct *ignored);
static void send_end_release_work_handler(struct work_struct *ignored);

static DECLARE_DELAYED_WORK(sendend_press_work, send_end_press_work_handler);
static DECLARE_DELAYED_WORK(sendend_release_work, send_end_release_work_handler);

struct sec_jack_info {
	struct sec_jack_port port;
	struct input_dev *input;
};

static struct sec_jack_info *hi;

struct switch_dev switch_jack_detection = {
		.name = "h2w",
};

struct switch_dev switch_sendend = {
		.name = "send_end",
};

static unsigned int send_end_irq_token = 0;
static unsigned short int current_jack_type_status = 0;
static struct timer_list send_end_enable_timer;
static struct wake_lock jack_sendend_wake_lock;
static int recording_status=0;


unsigned int get_headset_status(void)
{
	SEC_JACKDEV_DBG(" headset_status %d", current_jack_type_status);
	return current_jack_type_status;
}

EXPORT_SYMBOL(get_headset_status);

void set_recording_status(int value)
{
	recording_status = value;
}
static int get_recording_status(void)
{
	return recording_status;
}

static void jack_input_selector(int jack_type_status)
{
	SEC_JACKDEV_DBG("jack_type_status = 0X%x", jack_type_status);
}

void vps_status_change(int status)
{
	printk("[ JACK_DRIVER ] %s \n",__func__);

	if(status)
    {
		current_jack_type_status = SEC_EXTRA_DOCK_SPEAKER;

        //wm8994_set_vps_related_output_path(1);
    }
	else
    {
        int adc = s3c_adc_get_adc_data(SEC_HEADSET_ADC_CHANNEL);
	    struct sec_gpio_info *det_jack= &hi->port.det_jack;
        int jack_detect = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;

        if(jack_detect)
        {
            if(adc > 800)
            {
                current_jack_type_status = SEC_HEADSET_4_POLE_DEVICE;
            }
            else
            {
                current_jack_type_status = SEC_HEADSET_3_POLE_DEVICE;
            }
        }
        else
        {
		    current_jack_type_status = SEC_JACK_NO_DEVICE;
        }
        //wm8994_set_vps_related_output_path(0);
    }

	switch_set_state(&switch_jack_detection, current_jack_type_status);
}

void car_vps_status_change(int status)
{
	printk("[ CAR_JACK_DRIVER ] %s \n",__func__);

	if(status)
    {
		current_jack_type_status = SEC_EXTRA_CAR_DOCK_SPEAKER;

        //wm8994_set_vps_related_output_path(1);
    }
	else
    {
        int adc = s3c_adc_get_adc_data(SEC_HEADSET_ADC_CHANNEL);
	    struct sec_gpio_info *det_jack = &hi->port.det_jack;
        int jack_detect = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;

        if(jack_detect)
        {
            if(adc > 800)
            {
                current_jack_type_status = SEC_HEADSET_4_POLE_DEVICE;
            }
            else
            {
                current_jack_type_status = SEC_HEADSET_3_POLE_DEVICE;
            }
        }
        else
        {
		    current_jack_type_status = SEC_JACK_NO_DEVICE;
        }
        //wm8994_set_vps_related_output_path(0);
    }

	switch_set_state(&switch_jack_detection, current_jack_type_status);
}

#if 1//kimhyuns_test
static irqreturn_t send_end_irq_handler(int irq, void *dev_id);
#endif

//WORK QUEING FUNCTION
static void jack_type_detect_change(struct work_struct *ignored)
{
	int adc = 0;
	struct sec_gpio_info   *det_jack = &hi->port.det_jack;
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	struct sec_gpio_info   *send_end_open = &hi->port.send_end_open;
	int sendend_state,sendend_open_state;
	int state = 0;
	int count_abnormal=0;
	int count_pole=0;
	int ret =0;
	bool bQuit = false;

	while(!bQuit)
	{
		state = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;

	if (state)
	{
    	adc = s3c_adc_get_adc_data(SEC_HEADSET_ADC_CHANNEL);
			if(adc > 3250)
			{
				if(count_abnormal++>100)
				{
					count_abnormal=0;
					bQuit = true;
					/* detect 3pole or tv-out cable */
					printk(KERN_INFO "[ JACK_DRIVER (%s,%d) ] 3 pole headset or TV-out attatched : adc = %d\n", __func__,__LINE__,adc);
					count_pole = 0;
					if(send_end_irq_token==1)
					{
						disable_irq(send_end->eint);
						send_end_irq_token=0;
					}
					current_jack_type_status = SEC_HEADSET_3_POLE_DEVICE;
				}

				/* Todo : to prevent unexpected reset bug.
				 * 		  is it msleep bug? need wakelock.
				 */
				wake_lock_timeout(&jack_sendend_wake_lock, WAKELOCK_DET_TIMEOUT);
				msleep(10);

			}

				/* 4 pole zone */
			else if(adc > 800)
			{
				current_jack_type_status = SEC_HEADSET_4_POLE_DEVICE;
				printk(KERN_INFO "[ JACK_DRIVER (%s,%d) ] 4 pole  headset attached : adc = %d\n",__func__,__LINE__, adc);
				bQuit = true;
				if(send_end_irq_token==0)
				{
					printk(KERN_INFO "[ JACK_DRIVER (%s,%d) ] send_end_irq_token: %d\n",__func__,__LINE__, send_end_irq_token);
					enable_irq(send_end->eint);
					send_end_irq_token=1;
				}
			}
			/* unstable zone */
			else if(adc > 400)
			{
				SEC_JACKDEV_DBG("invalid adc > 400\n");
				return -1;
			}
			/* 3 pole zone or unstable zone */
			else
			{
				if(!adc || count_pole == 10)
				{

					/* detect 3pole or tv-out cable */
					printk(KERN_INFO "[ JACK_DRIVER (%s,%d) ] 3 pole headset or TV-out attatched : adc = %d\n", __func__,__LINE__,adc);
					count_pole = 0;

					printk(KERN_INFO "[ JACK_DRIVER (%s,%d) ] send_end_irq_token : %d\n", __func__,__LINE__,send_end_irq_token);
					if(send_end_irq_token==1)
					{
						disable_irq(send_end->eint);
						send_end_irq_token=0;
					}
					current_jack_type_status = SEC_HEADSET_3_POLE_DEVICE;
					bQuit=true;

				}
				/* If 4 pole is inserted slowly, ADC value should be lower than 250.
			 	* So, check again.
				 */
				else
				{
					++count_pole;
					/* Todo : to prevent unexpected reset bug.
					 * 		  is it msleep bug? need wakelock.
					 */
					wake_lock_timeout(&jack_sendend_wake_lock, WAKELOCK_DET_TIMEOUT);
					msleep(20);

				}

			}

		} /* if(state) */
		else
		{
			bQuit = true;

#if defined(CONFIG_JUPITER_VER_B4)
			gpio_set_value(GPIO_EAR_SEL, 1);
#else
			gpio_set_value(GPIO_EARPATH_SEL, 1);
#endif
			current_jack_type_status = SEC_JACK_NO_DEVICE;

			SEC_JACKDEV_DBG("JACK dev detached  \n");

		}
		switch_set_state(&switch_jack_detection, current_jack_type_status);
		jack_input_selector(current_jack_type_status);
		wake_unlock(&jack_sendend_wake_lock); 	

	}


	return 0;

}

static DECLARE_DELAYED_WORK(detect_jack_type_work, jack_type_detect_change);

static void send_end_enable_timer_handler(unsigned long arg)
{
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	struct sec_gpio_info   *det_jack = &hi->port.det_jack;
	int state = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;

	if(state)
	{
		SEC_JACKDEV_DBG("Detect  \n" );
		
		gpio_set_value(GPIO_EAR_BIAS_EN, 1); //Injection Ear Mic Bias	
		schedule_delayed_work(&detect_jack_type_work,50);
	}
}

static void send_end_press_work_handler(struct work_struct *ignored)
{
	SEC_JACKDEV_DBG("SEND/END Button is pressed");
	printk(KERN_ERR "SISO:sendend isr work queue\n");

	switch_set_state(&switch_sendend, SEC_HEADSET_4_POLE_DEVICE);
	input_report_key(hi->input, KEYCODE_SENDEND, 1);
	input_sync(hi->input);
}

static void send_end_release_work_handler(struct work_struct *ignored)
{
	switch_set_state(&switch_sendend, SEC_JACK_NO_DEVICE);
	input_report_key(hi->input, KEYCODE_SENDEND, 0);
	input_sync(hi->input);

	SEC_JACKDEV_DBG("SEND/END Button is %s.\n", "released");
}

static void jack_detect_change(struct work_struct *ignored)
{
	struct sec_gpio_info   *det_jack = &hi->port.det_jack;
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	int state;

	del_timer(&send_end_enable_timer);

	state = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;

	if (state && !send_end_irq_token)
	{
		SEC_JACKDEV_DBG("Headset attached, send end enable 2sec after");
		//send_end_enable_timer.expires = get_jiffies_64() + (5*HZ/10);//2sec HZ is 200
		send_end_enable_timer.expires = get_jiffies_64() + (HZ/10*2);//0.2sec HZ is 200
		add_timer(&send_end_enable_timer);
		wake_lock(&jack_sendend_wake_lock);
	
		printk(KERN_DEBUG "[wake_lock]222222222222222222\n");		
	}
	else if(!state)
	{
		SEC_JACKDEV_DBG("Headset detached %d ", send_end_irq_token);

		gpio_set_value(GPIO_EAR_BIAS_EN, 0);

		// if headset is ejected while sendend is pressed
		//   generate release key
		// else if sendend is detected while waiting for the delayed work
		//   cancel the work.
		if(switch_get_state(&switch_sendend) == 1) {
			printk(KERN_INFO " #@#@#@# Generate sendend release key!\n");
			send_end_release_work_handler(NULL);
		}
		else {
			cancel_delayed_work_sync(&sendend_press_work);
		}

		if(current_jack_type_status ==SEC_HEADSET_4_POLE_DEVICE) {
			disable_irq (send_end->eint);
		}

		wake_unlock(&jack_sendend_wake_lock);
		printk(KERN_DEBUG "[wake_unlock]Detach\n");
        switch_set_state(&switch_jack_detection, state);
		current_jack_type_status = state;

		//free_irq (send_end->eint, 0);

		if(send_end_irq_token > 0)
		{
			send_end_irq_token--;
		}
	}
	else
	{
		SEC_JACKDEV_DBG("Headset state does not valid. or send_end event");
		wake_unlock(&jack_sendend_wake_lock);
		printk(KERN_DEBUG "[wake_unlock]666666666666666666\n");
    }
}

static void sendend_switch_change(struct work_struct *ignored)
{

	struct sec_gpio_info   *det_jack = &hi->port.det_jack;
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	int state, jack_state;

	jack_state = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;
	state = gpio_get_value(send_end->gpio) ^ send_end->low_active;

	if(jack_state && send_end_irq_token)//headset connect && send irq enable
	{
		SEC_JACKDEV_DBG(" sendend_switch_change sendend state %d\n",state);
		if(!state)
		{
			printk(KERN_ERR "SISO:sendend isr work queue\n");
			wake_unlock(&jack_sendend_wake_lock);
			printk(KERN_DEBUG "[wake_unlock]8888888888888888\n");
			/* if keep pressed event for short key, reconize release event */
			schedule_delayed_work(&sendend_release_work, SHORTKEY_MS); 	
		}else{
			/* if keep pressed event for short key, reconize press event */
			schedule_delayed_work(&sendend_press_work, SHORTKEY_MS); 	
			wake_lock(&jack_sendend_wake_lock);
			printk(KERN_DEBUG "[wake_lock]4444444444444444444\n");
			SEC_JACKDEV_DBG("SEND/END Button is %s. timer start\n", "pressed");
		}
	}else{
		SEC_JACKDEV_DBG("SEND/END Button is %s but headset disconnect or irq disable.\n", state?"pressed":"released");
		wake_unlock(&jack_sendend_wake_lock);
		printk(KERN_DEBUG "[wake_unlock]99999999999999\n");
	}
}

static DECLARE_WORK(jack_detect_work, jack_detect_change);
static DECLARE_WORK(sendend_switch_work, sendend_switch_change);


//IRQ Handler
static irqreturn_t detect_irq_handler(int irq, void *dev_id)
{
	printk("##########[detect_irq_handler]########## \n");
	schedule_work(&jack_detect_work);
	return IRQ_HANDLED;
}

static irqreturn_t send_end_irq_handler(int irq, void *dev_id)
{
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	struct sec_gpio_info   *det_jack = &hi->port.det_jack;
	int headset_state;

	irq_jiffies = jiffies_64;
	headset_state = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;
	printk("##########[send_end_irq_handler irq : headset_state : %d ]########## \n" ,headset_state);

	if(headset_state)
	{
		/* Pressed */
		if (gpio_get_value(send_end->gpio) ^ send_end->low_active)
		{
		    printk("send/end Pressed \n");
			pressed_jiffies = irq_jiffies;
			schedule_work(&sendend_switch_work);
		}
		/* Released */
		else
		{
			/* ignore shortkey */
			if (irq_jiffies - pressed_jiffies < SHORTKEY_JIFFIES) {
				cancel_delayed_work_sync(&sendend_press_work);
				wake_unlock(&jack_sendend_wake_lock);
				printk(KERN_DEBUG "[wake_unlock]bbbbbbbbbbbbbbbb\n");
				SEC_JACKDEV_DBG("Cancel press work queue");
			}	
			else
				schedule_work(&sendend_switch_work);
		    printk("send/end Released \n");
		}
	}
	return IRQ_HANDLED;
}

//USER can select jack type if driver can't check the jack type
static int strtoi(char *buf)
{
	int ret;
	ret = buf[0]-48;
	return ret;
}
static ssize_t select_jack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "[JACK] %s : operate nothing\n", __FUNCTION__);

	return 0;
}
static ssize_t select_jack_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value = 0;
	struct sec_gpio_info   *det_jack = &hi->port.det_jack;
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	int state = gpio_get_value(det_jack->gpio) ^ det_jack->low_active;

	SEC_JACKDEV_DBG("buf = %s", buf);
	SEC_JACKDEV_DBG("buf size = %d", sizeof(buf));
	SEC_JACKDEV_DBG("buf size = %d", strlen(buf));

	if(state)
	{
		if(current_jack_type_status != SEC_UNKNOWN_DEVICE)
		{
			printk(KERN_ERR "user can't select jack device if current_jack_status isn't unknown status");
			return -1;
		}

		if(sizeof(buf)!=1)
		{
			printk("input error\n");
			printk("Must be stored ( 1,2,4)\n");
			return -1;
		}

		value = strtoi(buf);
		SEC_JACKDEV_DBG("User  selection : 0X%x", value);

		switch(value)
		{
			case SEC_HEADSET_3_POLE_DEVICE:
			{
				current_jack_type_status = SEC_HEADSET_3_POLE_DEVICE;
				switch_set_state(&switch_jack_detection, current_jack_type_status);
				jack_input_selector(current_jack_type_status);
				break;
			}
			case SEC_HEADSET_4_POLE_DEVICE:
			{
				enable_irq (send_end->eint);
				send_end_irq_token++;
				current_jack_type_status = SEC_HEADSET_4_POLE_DEVICE;
				switch_set_state(&switch_jack_detection, current_jack_type_status);
				jack_input_selector(current_jack_type_status);
				break;
			}
			case SEC_TVOUT_DEVICE:
			{
				current_jack_type_status = SEC_TVOUT_DEVICE;
//				gpio_set_value(GPIO_MICBIAS_EN, 0);
				switch_set_state(&switch_jack_detection, current_jack_type_status);
				jack_input_selector(current_jack_type_status);
				break;
			}
		}
	}
	else
	{
		printk(KERN_ALERT "Error : mic bias enable complete but headset detached!!\n");
		current_jack_type_status = SEC_JACK_NO_DEVICE;
//		gpio_set_value(GPIO_MICBIAS_EN, 0);
	}

	return size;
}
static DEVICE_ATTR(select_jack, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, select_jack_show, select_jack_store);
static int sec_jack_probe(struct platform_device *pdev)
{
	int ret;
	struct sec_jack_platform_data *pdata = pdev->dev.platform_data;
	struct sec_gpio_info   *det_jack;
	struct sec_gpio_info   *send_end;
	struct input_dev	   *input;
	current_jack_type_status = SEC_JACK_NO_DEVICE;
	sendend_type =0x00;//short type always

	printk(KERN_INFO "SEC HEADSET: Registering headset driver\n");
	hi = kzalloc(sizeof(struct sec_jack_info), GFP_KERNEL);
	if (!hi)
		return -ENOMEM;

	memcpy (&hi->port, pdata->port, sizeof(struct sec_jack_port));

	input = hi->input = input_allocate_device();
	if (!input)
	{
		ret = -ENOMEM;
		printk(KERN_ERR "SEC HEADSET: Failed to allocate input device.\n");
		goto err_request_input_dev;
	}

	input->name = "sec_jack";
	set_bit(EV_SYN, input->evbit);
	set_bit(EV_KEY, input->evbit);
	set_bit(KEYCODE_SENDEND, input->keybit);

	ret = input_register_device(input);
	if (ret < 0)
{
		printk(KERN_ERR "SEC HEADSET: Failed to register driver\n");
		goto err_register_input_dev;
	}
	
	init_timer(&send_end_enable_timer);
	send_end_enable_timer.function = send_end_enable_timer_handler;

	SEC_JACKDEV_DBG("registering switch_sendend switch_dev sysfs sec_jack");

	ret = switch_dev_register(&switch_jack_detection);
	if (ret < 0)
        {
		printk(KERN_ERR "SEC HEADSET: Failed to register switch device\n");
		goto err_switch_dev_register;
	}
		
         printk(KERN_ERR "SISO:registering switch_sendend switch_dev\n");
	
	ret = switch_dev_register(&switch_sendend);
	if (ret < 0) 
        {
		printk(KERN_ERR "SEC HEADSET: Failed to register switch sendend device\n");
		goto err_switch_dev_register;
	}

	//Create JACK Device file in Sysfs
	jack_class = class_create(THIS_MODULE, "jack");
	if(IS_ERR(jack_class))
	{
		printk(KERN_ERR "Failed to create class(sec_jack)\n");
	}

	jack_selector_fs = device_create(jack_class, NULL, 0, NULL, "jack_selector");
	if (IS_ERR(jack_selector_fs))
		printk(KERN_ERR "Failed to create device(sec_jack)!= %ld\n", IS_ERR(jack_selector_fs));

	if (device_create_file(jack_selector_fs, &dev_attr_select_jack) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_select_jack.attr.name);

	//GPIO configuration for short SENDEND
	send_end = &hi->port.send_end;
	s3c_gpio_cfgpin(send_end->gpio, S3C_GPIO_SFN(send_end->gpio_af));
	s3c_gpio_setpull(send_end->gpio, S3C_GPIO_PULL_NONE);
	set_irq_type(send_end->eint, IRQ_TYPE_EDGE_BOTH);


	ret = request_irq(send_end->eint, send_end_irq_handler, IRQF_DISABLED, "sec_headset_send_end", NULL);

	SEC_JACKDEV_DBG("sended isr send=0X%x, ret =%d", send_end->eint, ret);
	if (ret < 0)
	{
		printk(KERN_ERR "SEC HEADSET: Failed to register send/end interrupt.\n");
		goto err_request_send_end_irq;
	}

	disable_irq(send_end->eint);
	//GPIO configuration for Detect
	det_jack = &hi->port.det_jack;
    s3c_gpio_cfgpin(det_jack->gpio, S3C_GPIO_SFN(det_jack->gpio_af));
    s3c_gpio_setpull(det_jack->gpio, S3C_GPIO_PULL_NONE);
    set_irq_type(det_jack->eint, IRQ_TYPE_EDGE_BOTH);
	
	ret = request_irq(det_jack->eint, detect_irq_handler,IRQF_DISABLED, "sec_headset_detect", NULL);

	SEC_JACKDEV_DBG("detect isr send=0X%x, ret =%d", det_jack->eint, ret);
	if (ret < 0) {
		printk(KERN_ERR "SEC HEADSET: Failed to register detect interrupt.\n");
		goto err_request_detect_irq;
	}

	if(system_rev >= 0x4)
	{
		det_jack->low_active = 1;
		//EAR_BIAS_EN
    	s3c_gpio_cfgpin(GPIO_EAR_BIAS_EN, S3C_GPIO_SFN(GPIO_EAR_BIAS_EN_AF));
    	s3c_gpio_setpull(GPIO_EAR_BIAS_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_EAR_BIAS_EN, 0);

		if(gpio_get_value(GPIO_EAR_BIAS_EN) != 0)
			SEC_JACKDEV_DBG("GPIO EAR_EIAS set None");
	}
	
	gpio_direction_output(GPIO_MICBIAS_EN,0);
	s3c_gpio_slp_cfgpin(GPIO_MICBIAS_EN, S3C_GPIO_SLP_PREV); //seonha 6.14

	SEC_JACKDEV_DBG("sec_jack_probe system_rev =%d, 0x01=%d jack->low_active =%d", system_rev, 0x01, det_jack->low_active);

	wake_lock_init(&jack_sendend_wake_lock, WAKE_LOCK_SUSPEND, "sec_jack"); //戚嬢肉 繞嘉壱 獣拙 馬檎 宋製. 
	
	schedule_work(&jack_detect_work);

	return 0;

err_request_send_end_irq:
	free_irq(det_jack->eint, 0);
err_request_detect_irq:
	switch_dev_unregister(&switch_jack_detection);
err_switch_dev_register:
	input_unregister_device(input);
err_register_input_dev:
	input_free_device(input);
err_request_input_dev:
	kfree (hi);

	return ret;
}

//kvpz: this is to add support for earjack, and send end key simulation from sysfs state store for the two
void sec_jack_sendend_report_key(void)
{
		input_report_key(hi->input, KEYCODE_SENDEND, 1);				
		input_sync(hi->input);
}

EXPORT_SYMBOL(sec_jack_sendend_report_key);

static int sec_jack_remove(struct platform_device *pdev)
{
	SEC_JACKDEV_DBG("[sec_headset_remove]");
	input_unregister_device(hi->input);
	free_irq(hi->port.det_jack.eint, 0);
	free_irq(hi->port.send_end.eint, 0);

	switch_dev_unregister(&switch_jack_detection);
	return 0;
}

#ifdef CONFIG_PM
static int sec_jack_wake = 0;
static struct regulator *jack_regulator;
static int sec_jack_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	struct sec_gpio_info   *det_jack = &hi->port.det_jack;
        int err = 0;
	u32 pend_reg = 0;
	// to check jack is connected
	enable_irq_wake(det_jack->eint);
	if(current_jack_type_status == SEC_HEADSET_4_POLE_DEVICE)
	{
		printk("Dont suspend sec jack , head set connected\n");
		enable_irq_wake(send_end->eint);
		sec_jack_wake = 1;
		return 0;
	}
	
	printk("[sec_jack_suspend]");

        jack_regulator = regulator_get(NULL, "VADC");
        if(IS_ERR_OR_NULL(jack_regulator)) {
                pr_err("failed to get regulater VADC_3.3V \n");
                return 0;
        }

        err = regulator_enable(jack_regulator);
        if (err) {
                pr_err(" sec_jack_suspend -> Failed to enable regulator VADC_3.3V - sec_jack\n");
		return 0;	
        }


        err = regulator_disable(jack_regulator);
        if (err) {
                pr_err("Failed to disable regulator VADC_3.3V - sec_jack\n");
        }
	

    //msleep(10);
	//free_irq(hi->port.send_end.eint, 0);
	//flush_scheduled_work();
	return 0;
}
static int sec_jack_resume(struct platform_device *pdev)
{
	//struct regulator *jack_regulator;
        int err=0;
	struct sec_gpio_info   *send_end = &hi->port.send_end;
	struct sec_gpio_info   *det_jack = &hi->port.det_jack;

	disable_irq_wake(det_jack->eint);
	if(sec_jack_wake  == 1) {
		disable_irq_wake(send_end->eint);
		sec_jack_wake = 0;
	}
#if 1
	SEC_JACKDEV_DBG("[sec_jack_resume]");

        jack_regulator = regulator_get(NULL, "VADC");
        if(IS_ERR_OR_NULL(jack_regulator)) {
                pr_err("failed to get regulater VADC_3.3V \n");
                return 0;
        }

        err = regulator_enable(jack_regulator);
        if (err) {
                pr_err("Failed to enable regulator VADC_3.3V - sec_jack\n");
        }

	//schedule_work(&jack_detect_work);
	//if(extra_eint0pend&0x00400)
		//msleep(1000);
	//schedule_work(&sendend_switch_work);
	//request_irq( hi->port.send_end.eint, send_end_irq_handler,IRQF_DISABLED, "sec_jack_send_end", NULL);
#endif
	return 0;
}
#else
#define s3c_jack_resume 	NULL
#define s3c_jack_suspend	NULL
#endif

static struct platform_driver sec_jack_driver = {
	.probe		= sec_jack_probe,
	.remove		= sec_jack_remove,
	.suspend	= sec_jack_suspend,
	.resume		= sec_jack_resume,
	.driver		= {
		.name		= "sec_jack",
		.owner		= THIS_MODULE,
	},
};

static int __init sec_jack_init(void)
{
	SEC_JACKDEV_DBG("");
	return platform_driver_register(&sec_jack_driver);
}

static void __exit sec_jack_exit(void)
{
	platform_driver_unregister(&sec_jack_driver);
}

module_init(sec_jack_init);
module_exit(sec_jack_exit);

MODULE_AUTHOR("Eunki Kim <eunki_kim@samsung.com>");
MODULE_DESCRIPTION("SEC HEADSET detection driver");
MODULE_LICENSE("GPL");
