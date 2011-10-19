/*  drivers/misc/sec_jack.c
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/sec_jack.h>

#define MAX_ZONE_LIMIT		10
/* keep this value if you support double-pressed concept */

#define SEND_KEY_CHECK_TIME_MS	30		/* 30ms */
#define DET_CHECK_TIME_MS	200		/* 200ms */
#define WAKE_LOCK_TIME		(HZ * 5)	/* 5 sec */


#define CONFIG_DEBUG_SEC_JACK 1
#ifdef CONFIG_DEBUG_SEC_JACK
#define SEC_JACKDEV_DBG(...)\
        printk (__VA_ARGS__);
#else
#define SEC_JACKDEV_DBG(...)
#endif

/* Event to sent to platform */
#define KEYCODE_SENDEND 248

#define DETECTION_CHECK_COUNT   2
#define DETECTION_CHECK_TIME    get_jiffies_64() + (HZ/10)// 1000ms / 10 = 100ms
#define SEND_END_ENABLE_TIME    get_jiffies_64() + (HZ*2)// 1000ms * 1 = 1sec


#define SEND_END_CHECK_COUNT    3
#define SEND_END_CHECK_TIME     get_jiffies_64() + (HZ/50) //2000ms

struct class *jack_class;
EXPORT_SYMBOL(jack_class);
static struct device *jack_dev;                         // Sysfs device, this is used for communication with Cal App.
//EXPORT_SYMBOL(jack_selector_fs);                      //This variable is not being used outside 

struct sec_jack_info {
	struct sec_jack_platform_data *pdata;
	struct wake_lock det_wake_lock;
	struct sec_jack_zone *zone;
	struct input_dev *input;
	int det_irq;
	int dev_id;
	struct platform_device *send_key_dev;
	unsigned int cur_jack_type;
}*jack_info;

struct switch_dev switch_jack_detection = {
                .name = "h2w",
};

/* To support AT+FCESTEST=1 */
struct switch_dev switch_sendend = {
                .name = "send_end",
};
/* with some modifications like moving all the gpio structs inside
 * the platform data and getting the name for the switch and
 * gpio_event from the platform data, the driver could support more than
 * one headset jack, but currently user space is looking only for
 * one key file and switch for a headset so it'd be overkill and
 * untestable so we limit to one instantiation for now.
 */
static atomic_t instantiated = ATOMIC_INIT(0);
static unsigned int sendend_type; //0=short, 1=open
static unsigned int send_end_key_timer_token;
static unsigned int current_jack_type_status;
static unsigned int jack_detect_timer_token;
static unsigned int send_end_irq_token;

static struct timer_list jack_detect_timer;
static struct timer_list send_end_key_event_timer;
static struct wake_lock jack_sendend_wake_lock;

static struct gpio_event_direct_entry sec_jack_key_map[] = {
	{
		.code	= KEY_MEDIA,
	},
};

static struct gpio_event_input_info sec_jack_key_info = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = true,
	.type = EV_KEY,
	.debounce_time.tv.nsec = SEND_KEY_CHECK_TIME_MS * NSEC_PER_MSEC,
	.keymap = sec_jack_key_map,
	.keymap_size = ARRAY_SIZE(sec_jack_key_map)
};

static struct gpio_event_info *sec_jack_input_info[] = {
	&sec_jack_key_info.info,
};

static struct gpio_event_platform_data sec_jack_input_data = {
	.name = "sec_jack",
	.info = sec_jack_input_info,
	.info_count = ARRAY_SIZE(sec_jack_input_info),
};

static void sec_jack_set_type(struct sec_jack_info *hi, int jack_type)
{
	struct sec_jack_platform_data *pdata = hi->pdata;

	/* this can happen during slow inserts where we think we identified
	 * the type but then we get another interrupt and do it again
	 */
	if (jack_type == hi->cur_jack_type) {
		if (jack_type != SEC_HEADSET_4_POLE_DEVICE)
			pdata->set_micbias_state(false);
		return;
	}
	if (jack_type == SEC_HEADSET_4_POLE_DEVICE) {
		/* for a 4 pole headset, enable detection of send/end key */
		if (hi->send_key_dev == NULL)
			/* enable to get events again */
			hi->send_key_dev = platform_device_register_data(NULL,
					GPIO_EVENT_DEV_NAME,
					hi->dev_id,
					&sec_jack_input_data,
					sizeof(sec_jack_input_data));
	} else {
		/* for all other jacks, disable send/end key detection */
		if (hi->send_key_dev != NULL) {
			/* disable to prevent false events on next insert */
			platform_device_unregister(hi->send_key_dev);
			hi->send_key_dev = NULL;
		}
		/* micbias is left enabled for 4pole and disabled otherwise */
		pdata->set_micbias_state(false);
	}

	hi->cur_jack_type = jack_type;
	pr_info("%s : jack_type = %d\n", __func__, jack_type);

	/* prevent suspend to allow user space to respond to switch */
	wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME);

	switch_set_state(&switch_jack_detection, jack_type);
}

static void handle_jack_not_inserted(struct sec_jack_info *hi)
{
	sec_jack_set_type(hi, SEC_JACK_NO_DEVICE);
	hi->pdata->set_micbias_state(false);
}

static void determine_jack_type(struct sec_jack_info *hi)
{
	struct sec_jack_platform_data *pdata = hi->pdata;
	struct sec_jack_zone *zones = pdata->zones;
	int size = pdata->num_zones;

	int count[MAX_ZONE_LIMIT] = {0};
	int adc;
	int i;
	unsigned npolarity = !pdata->det_active_high;
	while (gpio_get_value(pdata->det_gpio) ^ npolarity) {
		adc = pdata->get_adc_value();
		pr_debug("%s: adc = %d\n", __func__, adc);

		/* determine the type of headset based on the
		 * adc value.  An adc value can fall in various
		 * ranges or zones.  Within some ranges, the type
		 * can be returned immediately.  Within others, the
		 * value is considered unstable and we need to sample
		 * a few more types (up to the limit determined by
		 * the range) before we return the type for that range.
		 */
		for (i = 0; i < size; i++) {
			if (adc <= zones[i].adc_high) {
				if (++count[i] > zones[i].check_count) {
					sec_jack_set_type(hi,
							  zones[i].jack_type);
					return;
				}
				msleep(zones[i].delay_ms);
				break;
			}
		}
	}
	/* jack removed before detection complete */
	pr_debug("%s : jack removed before detection complete\n", __func__);
	handle_jack_not_inserted(hi);
}

static void jack_input_selector(int jack_type_status)
{
        SEC_JACKDEV_DBG("jack_type_status = 0X%x", jack_type_status);
}

static void jack_type_detect_change(struct work_struct *ignored)
{
                int headset_state = gpio_get_value(jack_info->pdata->det_gpio) ^ jack_info->pdata->det_active_high;
                int sendend_short_state,sendend_open_state;
	
		printk(" ********************* LNT DEBUG : jack type detect change *************** \n");
                if(headset_state)
                {
                      //  sendend_state = gpio_get_value(send_end->gpio) ^ send_end->low_active;
                        sendend_short_state = gpio_get_value(jack_info->pdata->short_send_end_gpio) ^ jack_info->pdata->det_active_high;

        #if 1 //open_send_end do nothing
                        if (1) // suik_Fix (HWREV >= 0x01)
                        {
                               // sendend_open_state = gpio_get_value(send_end_open->gpio) ^ send_end_open->low_active;
				sendend_open_state = gpio_get_value(jack_info->pdata->open_send_end_gpio) ^ jack_info->pdata->det_active_low;
                                SEC_JACKDEV_DBG("SendEnd state short: %d Sendend open: %d sendend type : %d \n",sendend_short_state,sendend_open_state,sendend_type);
                                //if(sendend_state || sendend_open_state)   //suik_Fix
                                if(!sendend_open_state)
                                {
                                        printk("4 pole  headset attached\n");
                                        current_jack_type_status = SEC_HEADSET_4_POLE_DEVICE;
                                        #if 1 // REV07 Only suik_Check
                                       // if(gpio_get_value(send_end->gpio))
                                        if(gpio_get_value(jack_info->pdata->short_send_end_gpio))
                                        #endif
                                        {
                                         //  enable_irq (send_end->eint);  //suik_Fix
                                           enable_irq (jack_info->pdata->short_send_end_eintr);  //suik_Fix
					   printk("*************** LNT DEBUG *********** Enabled Short and Open Irqs \n");
                                        }
                                       // enable_irq (send_end_open->eint);
                                        enable_irq (jack_info->pdata->open_send_end_eintr);
                                }else
                                {
                                        printk("3 pole headset attatched\n");
                                        current_jack_type_status = SEC_HEADSET_3_POLE_DEVICE;
                                }

                        }else
        #endif
                        {
                                if(sendend_short_state)
                                {
                                        printk("4 pole  headset attached\n");
                                        current_jack_type_status = SEC_HEADSET_4_POLE_DEVICE;
                                }else
                                {
                                        printk("3 pole headset attatched\n");
                                        current_jack_type_status = SEC_HEADSET_3_POLE_DEVICE;
                                }
                               // enable_irq (send_end->eint);
                                enable_irq (jack_info->pdata->short_send_end_eintr);
                        }
                        send_end_irq_token++;
                        switch_set_state(&switch_jack_detection, current_jack_type_status);
                        jack_input_selector(current_jack_type_status);
                }
        wake_unlock(&jack_sendend_wake_lock);
}

static DECLARE_DELAYED_WORK(detect_jack_type_work, jack_type_detect_change);

static void jack_detect_change(struct work_struct *ignored)
{
	int headset_state;

	printk(" *************** LNT DEBUG *************** %s \n",__func__);
	// SEC_JACKDEV_DBG("");
	del_timer(&jack_detect_timer);
	cancel_delayed_work_sync(&detect_jack_type_work);

	headset_state = gpio_get_value(jack_info->pdata->det_gpio) ^ jack_info->pdata->det_active_high;
#if 1
	SEC_JACKDEV_DBG("jack_detect_change state %d send_end_irq_token %d", headset_state,send_end_irq_token);
	if (headset_state && !send_end_irq_token)
	{
		SEC_JACKDEV_DBG("************* Locked jack send end wake lock ");	
		wake_lock(&jack_sendend_wake_lock);
		if (jack_info->pdata->set_popup_sw_state)
			jack_info->pdata->set_popup_sw_state(1);
		SEC_JACKDEV_DBG("JACK dev attached timer start\n");
		jack_detect_timer_token = 0;
		jack_detect_timer.expires = DETECTION_CHECK_TIME;
		add_timer(&jack_detect_timer);
		sendend_type =0x00;//short type always
	}
	else if(!headset_state && send_end_irq_token)
	{
		/*  if(!get_recording_status())
		    {
		    gpio_set_value(GPIO_MICBIAS_EN, 0);
		    } */
		current_jack_type_status = SEC_JACK_NO_DEVICE;
		switch_set_state(&switch_jack_detection, current_jack_type_status);
		if (jack_info->pdata->set_popup_sw_state)
			jack_info->pdata->set_popup_sw_state(0);
		printk("JACK dev detached %d \n", send_end_irq_token);
		if(send_end_irq_token > 0)
		{
			//  if (1) //suik_Fix (HWREV >= 0x01)
			disable_irq (jack_info->pdata->open_send_end_eintr);
			disable_irq (jack_info->pdata->short_send_end_eintr);
			send_end_irq_token--;
			sendend_type = 0;
		}
		SEC_JACKDEV_DBG("************* Unlocked jack send end wake lock ");	
		wake_unlock(&jack_sendend_wake_lock);
	}
	else
		SEC_JACKDEV_DBG("Headset state does not valid. or send_end event");
#endif

}

static DECLARE_WORK(jack_detect_work, jack_detect_change);

static void jack_detect_timer_handler(unsigned long arg)
{
        struct sec_jack_platform_data *pdata = jack_info->pdata;
        int headset_state = 0;

        headset_state = gpio_get_value(pdata->det_gpio) ^ pdata->det_active_high;

	printk("****************** LNT DEBUG ********** In Jack detect timer handler: timer token : %d \n", jack_detect_timer_token);

        if(headset_state)
        {
               // SEC_JACKDEV_DBG("jack_detect_timer_token is %d\n", jack_detect_timer_token);
                if(jack_detect_timer_token < DETECTION_CHECK_COUNT)
                {
                        jack_detect_timer.expires = DETECTION_CHECK_TIME;
                        add_timer(&jack_detect_timer);
                        jack_detect_timer_token++;
                        //gpio_set_value(GPIO_MICBIAS_EN, 1); //suik_Fix for saving Sleep current
                }
                else if(jack_detect_timer_token == DETECTION_CHECK_COUNT)
                {
                        jack_detect_timer.expires = SEND_END_ENABLE_TIME;
                        jack_detect_timer_token = 0;
			printk(" ********** LNT DEBUG ********* JACK DETECT TIMER HANDLER : sendend_type : %d \n",sendend_type);
                        schedule_delayed_work(&detect_jack_type_work,50);
                }
                else if(jack_detect_timer_token == 4)
                {
                 //       SEC_JACKDEV_DBG("mic bias enable add work queue \n");
                        jack_detect_timer_token = 0;
                }
                else
                        printk(KERN_ALERT "wrong jack_detect_timer_token count %d", jack_detect_timer_token);
        }
        else
                printk(KERN_ALERT "headset detach!! %d", jack_detect_timer_token);
}

static void send_end_key_event_timer_handler(unsigned long arg)
{
	struct sec_jack_platform_data *pdata = jack_info->pdata;
	int sendend_state, headset_state = 0;

	headset_state = gpio_get_value(pdata->det_gpio) ^ pdata->det_active_high;

	printk(" ****************** LNT DEBUG ************** In send end key event timer handler \n ");

#if 1 //open_send_end do nothing..//suik_Fix
	if (sendend_type)
	{	
		sendend_state = gpio_get_value(pdata->open_send_end_gpio) ^ pdata->det_active_low;
	}else
#endif
	{
		sendend_state = gpio_get_value(pdata->short_send_end_gpio) ^ pdata->det_active_high;
	}

	printk(" ************** LNT DEBUG ********** In send event timer handler : headset state : %d , sendend_state : %d \n",headset_state,sendend_state);

	if(headset_state && sendend_state)
	{
		if(send_end_key_timer_token < SEND_END_CHECK_COUNT)
		{
			send_end_key_timer_token++;
			send_end_key_event_timer.expires = SEND_END_CHECK_TIME;
			add_timer(&send_end_key_event_timer);
			// SEC_JACKDEV_DBG("SendEnd Timer Restart %d", send_end_key_timer_token);
			printk("SendEnd Timer Restart %d", send_end_key_timer_token);
		}
		else if(send_end_key_timer_token == SEND_END_CHECK_COUNT)
		{
			printk("SEND/END is pressed\n");
			input_report_key(jack_info->input, KEYCODE_SENDEND, 1); //suik_Fix
			input_sync(jack_info->input);
			send_end_key_timer_token = 0;
		}
		else
			printk(KERN_ALERT "[JACK]wrong timer counter %d\n", send_end_key_timer_token);
	}else
		printk(KERN_ALERT "[JACK]GPIO Error\n");
}

/* thread run whenever the headset detect state changes (either insertion
 * or removal).
 */
static irqreturn_t sec_jack_detect_irq_handler(int irq, void *dev_id)
{
	printk(" *************** LNT DEBUG *********** SEC JACK DETECTED ****** \n");
	schedule_work(&jack_detect_work);
	return IRQ_HANDLED;
}

static void short_sendend_switch_change(struct work_struct *work)
{
	struct sec_jack_platform_data *pdata = jack_info->pdata;
	int headset_state, sendend_state;
	//  SEC_JACKDEV_DBG("");
	del_timer(&send_end_key_event_timer);
	send_end_key_timer_token = 0;

	headset_state = gpio_get_value(pdata->det_gpio) ^ pdata->det_active_high;
	sendend_state = gpio_get_value(pdata->open_send_end_gpio) ^ pdata->det_active_low;

	if(headset_state && send_end_irq_token)//headset connect && send irq enable
	{
		// SEC_JACKDEV_DBG(" short_sendend_switch_change sendend state %d\n",sendend_state);
		if(sendend_state)
		{
			wake_lock(&jack_sendend_wake_lock);
			send_end_key_event_timer.expires = SEND_END_CHECK_TIME;
			add_timer(&send_end_key_event_timer);
			switch_set_state(&switch_sendend, sendend_state);
			// SEC_JACKDEV_DBG("SEND/END %s.timer start \n", "pressed");
		}else
		{
			// SEC_JACKDEV_DBG(KERN_ERR "sendend isr work queue\n");
			switch_set_state(&switch_sendend, sendend_state);
			input_report_key(jack_info->input, KEYCODE_SENDEND, 0); //released  //suik_Fix
			input_sync(jack_info->input);
			printk("SEND/END %s.\n", "released");
			wake_unlock(&jack_sendend_wake_lock);
		}
	}else
	{
		printk("SEND/END Button is %s but headset disconnect or irq disable.\n", sendend_state?"pressed":"released");
		//  SEC_JACKDEV_DBG("SEND/END Button is %s but headset disconnect or irq disable.\n", state?"pressed":"released");
	}
}

DECLARE_WORK(short_sendend_switch_work,short_sendend_switch_change);

/* IRQ handler for SHORT SEND END */
static irqreturn_t short_send_end_irq_handler(int irq, void *dev_id)
{
	int headset_state;
	struct sec_jack_info *hi = dev_id;
	struct sec_jack_platform_data *pdata = hi->pdata;
	printk("************ LNT DEBUG *********** ENTERED SHORT SEND END IRQ HANDLER \n");
	// SEC_JACKDEV_DBG("[SHORT]send_end_open_irq_handler isr");
	del_timer(&send_end_key_event_timer);

	headset_state = gpio_get_value(pdata->det_gpio) ^ pdata->det_active_high;
	if (headset_state)
	{
		sendend_type = 0;
		schedule_work(&short_sendend_switch_work);               //suik_Fix
	}

	return IRQ_HANDLED;
}

static void open_sendend_switch_change(struct work_struct *work)
{

	int sendend_state, headset_state;
	struct sec_jack_platform_data *pdata = jack_info->pdata;
	//        SEC_JACKDEV_DBG("");
	del_timer(&send_end_key_event_timer);
	send_end_key_timer_token = 0;

	printk(" ****************** LNT DEBUG ************** In open send end switch change \n ");

	headset_state = gpio_get_value(pdata->det_gpio) ^ pdata->det_active_high;
	sendend_state = gpio_get_value(pdata->open_send_end_gpio) ^ pdata->det_active_low;

	printk("************* LNT DEBUG *********** : headset state : %d , sendend state : %d \n",headset_state, sendend_state);

	//if(headset_state && send_end_irq_token)//headset connect && send irq enable
	if(headset_state )//headset connect && send irq enable
	{
		printk(" open_sendend_switch_change sendend state %d\n",sendend_state);
		if(!sendend_state)  //suik_Fix sams as Sendend(Short)
		{
			// SEC_JACKDEV_DBG(KERN_ERR "sendend isr work queue\n");
			switch_set_state(&switch_sendend, sendend_state);
			input_report_key(jack_info->input, KEYCODE_SENDEND, 0); //released    //suik_Fix
			printk(" *********** Reported released event to platform *********** \n");
			input_sync(jack_info->input);
			printk("SEND/END %s.\n", "released");
			wake_unlock(&jack_sendend_wake_lock);
		}else
		{
			wake_lock(&jack_sendend_wake_lock);
			send_end_key_event_timer.expires = SEND_END_CHECK_TIME;
			add_timer(&send_end_key_event_timer);
			printk(" *********** Added timer to check for few milli seconds in open send end switch change  *********** \n");
			switch_set_state(&switch_sendend, sendend_state);
			printk("SEND/END %s.timer start \n", "pressed");
		}

	}else
	{
		printk("********** In else part of headset state in Bottom Half \n");
		//   SEC_JACKDEV_DBG("SEND/END Button is %s but headset disconnect or irq disable.\n", state?"pressed":"released");
	}
}


static DECLARE_WORK(open_sendend_switch_work, open_sendend_switch_change);

/* IRQ handler for Open SEND END */
static irqreturn_t send_end_open_irq_handler(int irq, void *dev_id)
{
	int headset_state;
	struct sec_jack_info *hi = dev_id;
	struct sec_jack_platform_data *pdata = hi->pdata;

	printk("************ LNT DEBUG *********** ENTERED OPEN SEND END IRQ HANDLER \n");

	/* SEC_JACKDEV_DBG("[OPEN]send_end_open_irq_handler isr"); */
	del_timer(&send_end_key_event_timer);
	headset_state = gpio_get_value(pdata->det_gpio) ^ (pdata->det_active_high);

	printk(" **************** LNT DEBUG *********: headset state : %d \n", headset_state);

	if (headset_state)
	{
		sendend_type = 0x01;
		schedule_work(&open_sendend_switch_work);               //suik_Fix
	}
	return IRQ_HANDLED;
}

/* sysfs entries */
static ssize_t select_jack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "[JACK] %s : operate nothing\n", __FUNCTION__);

	return 0;
}

static ssize_t select_jack_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t size)
{
	int value = 0;
	int headset_state =  gpio_get_value(jack_info->pdata->det_gpio) ^ jack_info->pdata->det_active_low;

	SEC_JACKDEV_DBG("buf = %s", buf);
	SEC_JACKDEV_DBG("buf size = %d", sizeof(buf));
	SEC_JACKDEV_DBG("buf size = %d", strlen(buf));

	if(headset_state)
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

		value = buf[0]-48; /* String to Integer */
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
					enable_irq (jack_info->pdata->short_send_end_eintr);
					send_end_irq_token++;
					current_jack_type_status = SEC_HEADSET_4_POLE_DEVICE;
					switch_set_state(&switch_jack_detection, current_jack_type_status);
					jack_input_selector(current_jack_type_status);
					break;
				}
			case SEC_TVOUT_DEVICE:
				{
					current_jack_type_status = SEC_TVOUT_DEVICE;
					//    gpio_set_value(GPIO_MICBIAS_EN, 0);
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
		//   gpio_set_value(GPIO_MICBIAS_EN, 0);
	}

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IXOTH,select_jack_show, select_jack_store);

static int sec_jack_probe(struct platform_device *pdev)
{
	struct sec_jack_info *hi;
	struct sec_jack_platform_data *pdata = pdev->dev.platform_data;
	struct input_dev *input;
	int ret;

	sendend_type = 0; /* By default sendend type 0 always , Short type */

	pr_info("%s : Registering jack driver\n", __func__);
	if (!pdata) {
		pr_err("%s : pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	if (!pdata->get_adc_value || !pdata->zones ||
			!pdata->set_micbias_state || pdata->num_zones > MAX_ZONE_LIMIT) {
		pr_err("%s : need to check pdata\n", __func__);
		return -ENODEV;
	}

	if (atomic_xchg(&instantiated, 1)) {
		pr_err("%s : already instantiated, can only have one\n",
				__func__);
		return -ENODEV;
	}

	sec_jack_key_map[0].gpio = pdata->short_send_end_gpio;

	jack_info = hi = kzalloc(sizeof(struct sec_jack_info), GFP_KERNEL);
	if (hi == NULL) {
		pr_err("%s : Failed to allocate memory.\n", __func__);
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	hi->pdata = pdata;

	/* make the id of our gpio_event device the same as our platform device,
	 * which makes it the responsiblity of the board file to make sure
	 * it is unique relative to other gpio_event devices
	 */
	hi->dev_id = pdev->id;

	/* Allocate the input device for reporting the sendend events to platform */
	input = hi->input = input_allocate_device();
	if (!input)
	{
		ret = -ENOMEM;
		printk(KERN_ERR "SEC JACK: Failed to allocate input device.\n");
		goto err_request_input_dev;
	}

	input->name = "sec_jack";
	set_bit(EV_SYN, input->evbit);
	set_bit(EV_KEY, input->evbit);
	set_bit(KEYCODE_SENDEND, input->keybit);

	ret = input_register_device(input);
	if (ret < 0)
	{
		printk(KERN_ERR "SEC JACK: Failed to register driver\n");
		goto err_register_input_dev;
	}

	wake_lock_init(&jack_sendend_wake_lock, WAKE_LOCK_SUSPEND, "sec_jack");

	init_timer(&jack_detect_timer);
	jack_detect_timer.function = jack_detect_timer_handler;

	init_timer(&send_end_key_event_timer);
	send_end_key_event_timer.function = send_end_key_event_timer_handler;

	/* Entries used by Call App */
	ret = switch_dev_register(&switch_jack_detection);
	if (ret < 0)
	{
		printk(KERN_ERR "SEC JACK: Failed to register switch device\n");
		goto err_switch_dev_register1;
	}

	ret = switch_dev_register(&switch_sendend);
	if (ret < 0)
	{
		printk(KERN_ERR "SEC JACK: Failed to register switch sendend device\n");
		goto err_switch_dev_register;
	}

	/* Create JACK Device file in Sysfs */
	jack_class = class_create(THIS_MODULE, "jack");
	if (IS_ERR(jack_class))
		pr_err("Failed to create class(sec_jack)\n");

	jack_dev = device_create(jack_class, NULL, 0, NULL, "jack_selector");
	if (IS_ERR(jack_dev))
			pr_err("Failed to create device(sec_jack)!= %ld\n",IS_ERR(jack_dev));

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file(%s)!\n",dev_attr_select_jack.attr.name);
	/* Sec Jack detection */
	ret = gpio_request(pdata->det_gpio, "ear_jack_detect");

	if (ret) {
		pr_err("%s : gpio_request failed for %d\n",
				__func__, pdata->det_gpio);
		goto err_gpio_request;
	}

	printk("******************* LNT DEBUG *********** gpio request for SEC JACK is success, gpio-pin : %d \n ", pdata->det_gpio); 


	hi->det_irq = gpio_to_irq(pdata->det_gpio);
	ret = request_threaded_irq(hi->det_irq, NULL,
			sec_jack_detect_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "sec_headset_detect", hi);
	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_detect_irq;
	}
	printk("******************* LNT DEBUG *********** IRQ SEC JACK registration is success, gpio-pin : %d \n ",hi->det_irq ); 

	/* to handle insert/removal when we're sleeping in a call */
	ret = enable_irq_wake(hi->det_irq);
	if (ret) {
		pr_err("%s : Failed to enable_irq_wake.\n", __func__);
		goto err_enable_irq_wake;
	}
#if 0
	/* GPIO configuration for short SENDEND */
	ret = request_threaded_irq(pdata->short_send_end_eintr,NULL,
			short_send_end_irq_handler, IRQF_DISABLED, 
			"sec_headset_send_end_short", hi);
	if (ret < 0)
	{
		printk(KERN_ERR "SEC HEADSET: Failed to register SHORT send/end interrupt.\n");
		goto err_request_send_end_irq;
	}

	disable_irq(pdata->short_send_end_eintr);
	printk("******************* LNT DEBUG *********** SEND END SHORT IRQ registration success, gpio-pin : %d \n ",pdata->short_send_end_gpio ); 
#endif
	/* GPIO configuration for open SENDEND */
	set_irq_type(pdata->open_send_end_eintr,IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT );
	ret = request_threaded_irq(pdata->open_send_end_eintr,NULL, 
			send_end_open_irq_handler, IRQF_DISABLED, 
			"sec_headset_send_end_open",hi);
	if (ret < 0) {
		printk(KERN_ERR "SEC HEADSET: Failed to register OPEN send/end interrupt.\n");
		goto err_request_open_send_end_irq;
	}
	disable_irq(pdata->open_send_end_eintr);

	printk("******************* LNT DEBUG *********** SEND END OPEN IRQ registration success, gpio-pin : %d \n ",pdata->open_send_end_gpio ); 

	schedule_work(&jack_detect_work);

	dev_set_drvdata(&pdev->dev, hi);

	return 0;

err_request_open_send_end_irq:
//	free_irq(hi->pdata->short_send_end_eintr,hi);
//err_request_send_end_irq:
	disable_irq_wake(hi->det_irq);
err_enable_irq_wake:
	free_irq(hi->det_irq, hi);
err_request_detect_irq:
	gpio_free(pdata->det_gpio);
err_gpio_request:
	switch_dev_unregister(&switch_sendend);
err_switch_dev_register:
	switch_dev_unregister(&switch_jack_detection);
err_switch_dev_register1:
	input_unregister_device(hi->input);
	wake_lock_destroy(&jack_sendend_wake_lock);
err_register_input_dev:
	input_free_device(input);
err_request_input_dev:
	kfree(hi);
	jack_info=NULL;
err_kzalloc:
	atomic_set(&instantiated, 0);

	return ret;
}

static int sec_jack_remove(struct platform_device *pdev)
{

	struct sec_jack_info *hi = dev_get_drvdata(&pdev->dev);

	pr_info("%s :\n", __func__);
	input_unregister_device(hi->input);
	disable_irq_wake(hi->det_irq);
	free_irq(hi->det_irq, hi);
	platform_device_unregister(hi->send_key_dev);
	wake_lock_destroy(&jack_sendend_wake_lock);
	switch_dev_unregister(&switch_jack_detection);
	switch_dev_unregister(&switch_sendend);
	free_irq(hi->pdata->open_send_end_eintr,hi);
	free_irq(hi->pdata->short_send_end_eintr,hi);
	gpio_free(hi->pdata->det_gpio);
	kfree(hi);
	atomic_set(&instantiated, 0);

	return 0;
}

static struct platform_driver sec_jack_driver = {
	.probe = sec_jack_probe,
	.remove = sec_jack_remove,
	.driver = {
			.name = "sec_jack",
			.owner = THIS_MODULE,
		   },
};
static int __init sec_jack_init(void)
{
	return platform_driver_register(&sec_jack_driver);
}

static void __exit sec_jack_exit(void)
{
	platform_driver_unregister(&sec_jack_driver);
}

module_init(sec_jack_init);
module_exit(sec_jack_exit);

MODULE_AUTHOR("ms17.kim@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Corp Ear-Jack detection driver");
MODULE_LICENSE("GPL");
