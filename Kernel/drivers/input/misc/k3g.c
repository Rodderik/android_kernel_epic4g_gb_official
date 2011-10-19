/*
 *  Copyright (C) 2010, Samsung Electronics Co. Ltd. All Rights Reserved.
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
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#define DEBUG	1
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input/k3g.h>
#include <linux/delay.h>

/* k3g chip id */
#define DEVICE_ID	0xD3
/* k3g gyroscope registers */
#define WHO_AM_I	0x0F
#define CTRL_REG1	0x20  /* power control reg */
#define CTRL_REG2	0x21  /* power control reg */
#define CTRL_REG3	0x22  /* power control reg */
#define CTRL_REG4	0x23  /* interrupt control reg */
#define CTRL_REG5	0x24  /* interrupt control reg */
#define OUT_TEMP	0x26  /* Temperature data */
#define STATUS_REG	0x27
#define AXISDATA_REG	0x28
#define FIFO_CTRL_REG	0x2E
#define PM_OFF		0x00
#define PM_NORMAL	0x08
#define ENABLE_ALL_AXES	0x07
#define BYPASS_MODE	0x00
#define FIFO_MODE	0x20

#define ODR_MASK        0xF0
#define ODR105_BW12_5	0x00  /* ODR = 105Hz; BW = 12.5Hz */
#define ODR105_BW25	0x10  /* ODR = 105Hz; BW = 25Hz   */
#define ODR210_BW12_5	0x40  /* ODR = 210Hz; BW = 12.5Hz */
#define ODR210_BW25	0x50  /* ODR = 210Hz; BW = 25Hz   */
#define ODR210_BW50	0x60  /* ODR = 210Hz; BW = 50Hz   */
#define ODR210_BW70	0x70  /* ODR = 210Hz; BW = 70Hz   */
#define ODR420_BW20	0x80  /* ODR = 420Hz; BW = 20Hz   */
#define ODR420_BW25	0x90  /* ODR = 420Hz; BW = 25Hz   */
#define ODR420_BW50	0xA0  /* ODR = 420Hz; BW = 50Hz   */
#define ODR420_BW110	0xB0  /* ODR = 420Hz; BW = 110Hz  */
#define ODR840_BW30	0xC0  /* ODR = 840Hz; BW = 30Hz   */
#define ODR840_BW35	0xD0  /* ODR = 840Hz; BW = 35Hz   */
#define ODR840_BW50	0xE0  /* ODR = 840Hz; BW = 50Hz   */
#define ODR840_BW110	0xF0  /* ODR = 840Hz; BW = 110Hz  */

#define MIN_ST		175
#define MAX_ST		875
#define AC		(1 << 7) /* register auto-increment bit */

#define READ_REPEAT_SHIFT	2
#define READ_REPEAT		(1 << READ_REPEAT_SHIFT)

#define K3G_MAJOR   102
#define K3G_MINOR   4

/* default register setting for device init */
static const char default_ctrl_regs[] = {
	0x0F,	/* 105HZ, PD, XYZ enable */
	0x00,	/* Normal mode */
	0x04,	/* fifo interrupt */
	0x20,	/* 2000d/s */
	0x40,	/* fifo enable */
};

static const struct odr_delay {
	u8 odr; /* odr reg setting */
	s64 delay_ns; /* odr in ns */
} odr_delay_table[] = {
	{  ODR840_BW110,  1250000LL << READ_REPEAT_SHIFT }, /* 840Hz */
	{  ODR420_BW110,  2500000LL << READ_REPEAT_SHIFT }, /* 420Hz */
	{   ODR210_BW70,  5000000LL << READ_REPEAT_SHIFT }, /* 210Hz */
	{   ODR105_BW25, 10000000LL << READ_REPEAT_SHIFT }, /* 105Hz */
};

/*
 * K3G gyroscope data
 * brief structure containing gyroscope values for yaw, pitch and roll in
 * signed short
 */
struct k3g_t {
	s16 x;
	s16 y;
	s16 z;
};

struct k3g_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mutex lock;
	bool enable;
	bool interruptable;		/* interrupt or polling? */
	u8 ctrl_regs[5];		/* saving reg settings */
	struct workqueue_struct *k3g_wq;/* for polling */
	struct work_struct work;	/* for polling */
	struct hrtimer timer;		/* for polling */
	ktime_t polling_delay;		/* for polling */
};

static struct class *k3g_gyro_dev_class;
static struct k3g_data *gyro;

/* gyroscope data readout */
static int k3g_read_gyro_values(struct i2c_client *client,
				struct k3g_t *data, int burst)
{
	int res;
	unsigned char gyro_data[sizeof(*data) * burst];

	res = i2c_smbus_read_i2c_block_data(client, AXISDATA_REG | AC,
					sizeof(*data) * burst, gyro_data);
	if (res < 0)
		return res;

	burst--;
	/* catch the last fifo entry */
	data->x = (gyro_data[burst*6+1] << 8) | gyro_data[burst*6];
	data->y = (gyro_data[burst*6+3] << 8) | gyro_data[burst*6+2];
	data->z = (gyro_data[burst*6+5] << 8) | gyro_data[burst*6+4];

	return 0;
}

static enum hrtimer_restart k3g_timer_func(struct hrtimer *timer)
{
	struct k3g_data *k3g_data = container_of(timer, struct k3g_data, timer);

	queue_work(k3g_data->k3g_wq, &k3g_data->work);
	hrtimer_forward_now(&k3g_data->timer, k3g_data->polling_delay);
	return HRTIMER_RESTART;
}

static int k3g_report_value(struct k3g_data *k3g_data)
{
	int res;
	int burst = 1;
	struct k3g_t data;

	if (k3g_data->interruptable)
		burst = READ_REPEAT;

	res = k3g_read_gyro_values(k3g_data->client, &data, burst);
		if (res < 0)
			return -EIO;

	input_report_rel(k3g_data->input_dev, REL_RX, data.x);
	input_report_rel(k3g_data->input_dev, REL_RY, data.y);
	input_report_rel(k3g_data->input_dev, REL_RZ, data.z);
	input_sync(k3g_data->input_dev);

	return 0;
}

static void k3g_work_func(struct work_struct *work)
{
	int res;
	struct k3g_data *data = container_of(work, struct k3g_data, work);
	res = k3g_report_value(data);
	if (res < 0)
		pr_err("%s: Cannot report gyro value\n", __func__);
	}

static irqreturn_t k3g_interrupt_func(int irq, void *k3g_data)
{
	int res;
	struct k3g_data *data = k3g_data;

	res = k3g_report_value(data);
	if (res < 0)
		pr_err("%s: Can't report gyro values\n", __func__);

	/* Clear fifo interrupt */
	res = i2c_smbus_write_byte_data(data->client,
			FIFO_CTRL_REG, BYPASS_MODE);
	if (res < 0)
		pr_err("%s: Can't clear fifo int\n", __func__);
	/* Restart fifo after clearing */
	res = i2c_smbus_write_byte_data(data->client,
			FIFO_CTRL_REG, FIFO_MODE | READ_REPEAT);
	if (res < 0)
		pr_err("%s: Can't restart fifo after interrupt\n",
				__func__);

	return IRQ_HANDLED;
}

static ssize_t k3g_show_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", k3g_data->enable);
}

static ssize_t k3g_set_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	bool new_enable;

	if (sysfs_streq(buf, "1"))
		new_enable = true;
	else if (sysfs_streq(buf, "0"))
		new_enable = false;
	else {
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (new_enable == k3g_data->enable)
		return size;

	mutex_lock(&k3g_data->lock);
	if (new_enable) {
		/* turning on and enable x,y,z */
		k3g_data->ctrl_regs[0] = PM_NORMAL | ENABLE_ALL_AXES;
		err = i2c_smbus_write_i2c_block_data(k3g_data->client,
			CTRL_REG1 | AC, sizeof(k3g_data->ctrl_regs),
						k3g_data->ctrl_regs);
		if (err < 0) {
			err = -EIO;
			goto unlock;
		}
		if (k3g_data->interruptable)
			enable_irq(k3g_data->client->irq);
		else
		hrtimer_start(&k3g_data->timer,
				k3g_data->polling_delay, HRTIMER_MODE_REL);
	} else {
		if (k3g_data->interruptable)
			disable_irq(k3g_data->client->irq);
		else {
		hrtimer_cancel(&k3g_data->timer);
		cancel_work_sync(&k3g_data->work);
		}
		/* turning off */
		err = i2c_smbus_write_byte_data(k3g_data->client,
					CTRL_REG1, 0x00);
		if (err < 0)
			goto unlock;
	}
	k3g_data->enable = new_enable;

unlock:
	mutex_unlock(&k3g_data->lock);

	return err ? err : size;
}

static s64 k3g_get_int_delay(struct k3g_data *k3g_data)
{
	int i;
	u8 odr;
	s64 delay = -1;

	odr = k3g_data->ctrl_regs[0] & ODR_MASK;
	for (i = 0; i < ARRAY_SIZE(odr_delay_table); i++) {
		if (odr == odr_delay_table[i].odr) {
			delay = odr_delay_table[i].delay_ns;
			break;
	}
}

	return delay;
}

static ssize_t k3g_show_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	s64 delay;

	if (k3g_data->interruptable)
		delay = k3g_get_int_delay(k3g_data);
	else
		delay = ktime_to_ns(k3g_data->polling_delay);

	return sprintf(buf, "%lld\n", delay);
}

static ssize_t k3g_set_delay(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	int odr_value = ODR105_BW25;
	int res = 0;
	int i;
	s64 delay_ns;

	res = strict_strtoll(buf, 10, &delay_ns);
	if (res < 0)
		return res;

	if (k3g_data->interruptable) {
		/* round to the nearest supported ODR that is less than
		 * the requested value
		 */
		for (i = 0; i < ARRAY_SIZE(odr_delay_table); i++) {
			if (delay_ns <= odr_delay_table[i].delay_ns) {
				odr_value = odr_delay_table[i].odr;
				delay_ns = odr_delay_table[i].delay_ns;
				break;
		}
	}
		mutex_lock(&k3g_data->lock);
		if (odr_value != (k3g_data->ctrl_regs[0] & ODR_MASK)) {
			u8 ctrl = (k3g_data->ctrl_regs[0] & ~ODR_MASK);
			ctrl |= odr_value;
			k3g_data->ctrl_regs[0] = ctrl;
			res = i2c_smbus_write_byte_data(k3g_data->client,
							CTRL_REG1, ctrl);
	}
		mutex_unlock(&k3g_data->lock);
	} else {
		k3g_data->polling_delay = ns_to_ktime(delay_ns);
		mutex_lock(&k3g_data->lock);
		hrtimer_cancel(&k3g_data->timer);
		hrtimer_start(&k3g_data->timer,
			k3g_data->polling_delay, HRTIMER_MODE_REL);
		mutex_unlock(&k3g_data->lock);
}

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
			k3g_show_enable, k3g_set_enable);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
			k3g_show_delay, k3g_set_delay);


/*************************************************************************/
/* K3G Sysfs                                                             */
/*************************************************************************/

/*  i2c write routine for k3g digital gyroscope */
static char k3g_i2c_write(unsigned char reg_addr,
				    unsigned char *data,
				    unsigned char len)
{
	int dummy;
	int i;

	if (gyro->client == NULL)  /*  No global client pointer? */
		return -1;
	for (i = 0; i < len; i++) {
		dummy = i2c_smbus_write_byte_data(gyro->client,
						  reg_addr++, data[i]);
		if (dummy) {
			printk(KERN_INFO "i2c write error\n");
			return dummy;
		}
	}
	return 0;
}

/*  i2c read routine for k3g digital gyroscope */
static char k3g_i2c_read(unsigned char reg_addr,
				   unsigned char *data,
				   unsigned char len)
{
	int dummy = 0;
	int i = 0;

	if (gyro->client == NULL)  /*  No global client pointer? */
		return -1;
	while (i < len) {
		dummy = i2c_smbus_read_word_data(gyro->client, reg_addr++);
		if (dummy >= 0) {
			data[i] = dummy & 0x00ff;
			/* printk("data[%d]=%x\e\n",i,dummy); */
			i++;
		} else {
			printk(KERN_INFO "i2c read error\n ");
			return dummy;
		}
		dummy = len;
	}
	return dummy;
}

/* Device Initialization  */
static int device_init(void)
{
	int res;
	unsigned char buf[5];
	buf[0] = 0x6f;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	res = k3g_i2c_write(CTRL_REG1, &buf[0], 5);
	return res;
}

/* TEST */
/* #define POWER_ON_TEST */
static ssize_t k3g_power_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int count;
	int res;
#ifdef POWER_ON_TEST
	unsigned char gyro_data[6];
	short raw[3];
#endif

	res = device_init();

#ifdef POWER_ON_TEST	/* For Test */
	res = k3g_i2c_read(AXISDATA_REG, &gyro_data[0], 6);

	raw[0] = (short) (((gyro_data[1]) << 8) | gyro_data[0]);
	raw[1] = (short) (((gyro_data[3]) << 8) | gyro_data[2]);
	raw[2] = (short) (((gyro_data[5]) << 8) | gyro_data[4]);

	printk(KERN_INFO "[gyro_self_test] raw[0] = %d\n", raw[0]);
	printk(KERN_INFO "[gyro_self_test] raw[1] = %d\n", raw[1]);
	printk(KERN_INFO "[gyro_self_test] raw[2] = %d\n", raw[2]);
	printk(KERN_INFO "\n");
#endif

	printk(KERN_INFO "[%s] result of device init = %d\n", __func__, res);

	count = sprintf(buf, "%d\n", (res < 0 ? 0 : 1));

	return count;
}

static ssize_t k3g_get_temp(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int count;
	char temp;

	/* before starting self-test, backup register */
	k3g_i2c_read(OUT_TEMP, &temp, 1);

	printk(KERN_INFO "[%s] read temperature : %d\n", __func__, temp);

	count = sprintf(buf, "%d\n", temp);

	return count;
}

static ssize_t k3g_self_test(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int count;
	int res;
	unsigned char temp[1];
	unsigned char gyro_data[6];
	short raw[3];
	int NOST[3], ST[3];
	int differ_x = 0, differ_y, differ_z;
	unsigned char bak_reg[5];
	unsigned char reg[5];
	unsigned char bZYXDA = 0;
	unsigned char pass = 0;
	int i = 0;

	memset(NOST, 0, sizeof(int)*3);
	memset(ST, 0, sizeof(int)*3);

	/* before starting self-test, backup register */
	k3g_i2c_read(CTRL_REG1, &bak_reg[0], 5);

	for (i = 0; i < 5; i++)
		printk(KERN_INFO "[gyro_self_test] "
			"backup reg[%d] = %2x\n", i, bak_reg[i]);

	/* Initialize Sensor, turn on sensor, enable P/R/Y */
	/* Set BDU=1, Set ODR=200Hz, Cut-Off Frequency=50Hz, FS=2000dps */
	reg[0] = 0x6f;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0xA0;
	reg[4] = 0x02;

	k3g_i2c_write(CTRL_REG1, &reg[0], 5);

	/* Power up, wait for 800ms for stable output */
	mdelay(800);

	/* Read 5 samples output before self-test on */
	i = 0;
	while (i < 5) {
		/* check ZYXDA ready bit */
		k3g_i2c_read(STATUS_REG, &temp[0], 1);
		bZYXDA = (unsigned int)temp & 0x08;
		if (!bZYXDA) {
			mdelay(10);
			continue;
		}

		res = k3g_i2c_read(AXISDATA_REG, &gyro_data[0], 6);

		raw[0] = (short) (((gyro_data[1]) << 8) | gyro_data[0]);
		raw[1] = (short) (((gyro_data[3]) << 8) | gyro_data[2]);
		raw[2] = (short) (((gyro_data[5]) << 8) | gyro_data[4]);

		NOST[0] += raw[0];
		NOST[1] += raw[1];
		NOST[2] += raw[2];

		printk(KERN_INFO "[gyro_self_test] raw[0] = %d\n", raw[0]);
		printk(KERN_INFO "[gyro_self_test] raw[1] = %d\n", raw[1]);
		printk(KERN_INFO "[gyro_self_test] raw[2] = %d\n", raw[2]);
		printk(KERN_INFO "\n");

		i++;
	}

	for (i = 0; i < 3; i++)
		printk(KERN_INFO "[gyro_self_test] "
			"SUM of NOST[%d] = %d\n", i, NOST[i]);

	/* calculate average of NOST and covert from ADC to DPS */
	for (i = 0; i < 3; i++) {
		NOST[i] = (NOST[i] / 5) * 70 / 1000;
		printk(KERN_INFO "[gyro_self_test] "
			"AVG of NOST[%d] = %d\n", i, NOST[i]);
	}
	printk(KERN_INFO "\n");

	/* Enable Self Test */
	reg[0] = 0xA2;
	k3g_i2c_write(CTRL_REG4, &reg[0], 1);
	mdelay(100);

	/* Read 5 samples output after self-test on */
	i = 0;
	while (i < 5) {
		/* check ZYXDA ready bit */
		k3g_i2c_read(STATUS_REG, &temp[0], 1);
		bZYXDA = (unsigned int)temp & 0x08;

		if (!bZYXDA) {
			mdelay(10);
			continue;
		}

		res = k3g_i2c_read(AXISDATA_REG, &gyro_data[0], 6);

		raw[0] = (short) (((gyro_data[1]) << 8) | gyro_data[0]);
		raw[1] = (short) (((gyro_data[3]) << 8) | gyro_data[2]);
		raw[2] = (short) (((gyro_data[5]) << 8) | gyro_data[4]);

		ST[0] += raw[0];
		ST[1] += raw[1];
		ST[2] += raw[2];

		printk(KERN_INFO "[gyro_self_test] raw[0] = %d\n", raw[0]);
		printk(KERN_INFO "[gyro_self_test] raw[1] = %d\n", raw[1]);
		printk(KERN_INFO "[gyro_self_test] raw[2] = %d\n", raw[2]);
		printk(KERN_INFO "\n");

		i++;
	}

	for (i = 0; i < 3; i++)
		printk(KERN_INFO "[gyro_self_test] "
			"SUM of ST[%d] = %d\n", i, ST[i]);

	/* calculate average of ST and convert from ADC to dps */
	for (i = 0; i < 3; i++)	{
		/* When FS=2000, 70 mdps/digit */
		ST[i] = (ST[i] / 5) * 70 / 1000;
		printk(KERN_INFO "[gyro_self_test] "
			"AVG of ST[%d] = %d\n", i, ST[i]);
	}

	/* check whether pass or not */
	if (ST[0] >= NOST[0])  /* for x */
		differ_x = ST[0] - NOST[0];
	else
		differ_x = NOST[0] - ST[0];

	if (ST[1] >= NOST[1])  /* for y */
		differ_y = ST[1] - NOST[1];
	else
		differ_y = NOST[1] - ST[1];

	if (ST[2] >= NOST[2])  /* for z */
		differ_z = ST[2] - NOST[2];
	else
		differ_z = NOST[2] - ST[2];

	printk(KERN_INFO "[gyro_self_test] differ x:%d, y:%d, z:%d\n",
		differ_x, differ_y, differ_z);

	if ((MIN_ST <= differ_x && differ_x <= MAX_ST)
		&& (MIN_ST <= differ_y && differ_y <= MAX_ST)
		&& (MIN_ST <= differ_z && differ_z <= MAX_ST))
		pass = 1;

	/* restore backup register */
	k3g_i2c_write(CTRL_REG1, &bak_reg[0], 5);

	printk(KERN_INFO "[gyro_self_test] self-test result : %s\n",
		pass ? "pass" : "fail");
	count = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d\n",
		NOST[0], NOST[1], NOST[2], ST[0], ST[1], ST[2], pass);

	return count;
}

static ssize_t k3g_fs_write(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	/* buf[size]=0; */
	printk(KERN_INFO "input data --> %s\n", buf);

	return size;
}

static DEVICE_ATTR(gyro_power_on, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH,
	k3g_power_on, NULL);
static DEVICE_ATTR(gyro_get_temp, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH,
	k3g_get_temp, NULL);
static DEVICE_ATTR(gyro_selftest, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH,
	k3g_self_test, k3g_fs_write);

static const struct file_operations k3g_fops = {
	.owner = THIS_MODULE,
};


/*************************************************************************/
/* End of K3G Sysfs                                                      */
/*************************************************************************/


static int k3g_probe(struct i2c_client *client,
			       const struct i2c_device_id *devid)
{
	int ret;
	int err = 0;
	struct k3g_data *data;
	struct input_dev *input_dev;
	struct device *dev;
	
	pr_debug("*************GYRO PROBE IN\n *********************");

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto exit;
	}

	data->client = client;

	/* read chip id */
	ret = i2c_smbus_read_byte_data(client, WHO_AM_I);
	if (ret != DEVICE_ID) {
		if (ret < 0) {
			pr_err("%s: i2c for reading chip id failed\n",
								__func__);
			err = ret;
		} else {
			pr_err("%s : Device identification failed\n",
								__func__);
			err = -ENODEV;
		}
		goto err_read_reg;
	}

	mutex_init(&data->lock);

	/* allocate gyro input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		err = -ENOMEM;
		goto err_input_allocate_device;
	}

	data->input_dev = input_dev;
	input_set_drvdata(input_dev, data);
	input_dev->name = "gyro";
	/* X */
	input_set_capability(input_dev, EV_REL, REL_RX);
	input_set_abs_params(input_dev, REL_RX, -2048, 2047, 0, 0);
	/* Y */
	input_set_capability(input_dev, EV_REL, REL_RY);
	input_set_abs_params(input_dev, REL_RY, -2048, 2047, 0, 0);
	/* Z */
	input_set_capability(input_dev, EV_REL, REL_RZ);
	input_set_abs_params(input_dev, REL_RZ, -2048, 2047, 0, 0);

	err = input_register_device(input_dev);
	if (err < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(data->input_dev);
		goto err_input_register_device;
	}

	if (device_create_file(&input_dev->dev,
				&dev_attr_enable) < 0) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_enable.attr.name);
		goto err_device_create_file;
	}

	if (device_create_file(&input_dev->dev,
				&dev_attr_poll_delay) < 0) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_poll_delay.attr.name);
		goto err_device_create_file2;
	}

	memcpy(&data->ctrl_regs, &default_ctrl_regs, sizeof(default_ctrl_regs));

	if (data->client->irq >= 0) { /* interrupt */
		data->interruptable = true;
		/* fifo mode */
		ret = i2c_smbus_write_byte_data(data->client,
				FIFO_CTRL_REG, FIFO_MODE | READ_REPEAT);
		if (ret < 0) {
			pr_err("%s: can't write fifo mode to reg\n", __func__);
			goto err_write_reg;
	}
		if (request_threaded_irq(data->client->irq, NULL,
					k3g_interrupt_func, IRQF_TRIGGER_RISING,
					"k3g", data)) {
			pr_err("%s: can't allocate irq.\n", __func__);
			goto err_request_irq;
	}
		disable_irq(data->client->irq);

	} else { /* polling */
		data->ctrl_regs[2] = 0x00; /* disable interrupt */
		/* hrtimer settings.  we poll for gyro values using a timer. */
		hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		data->polling_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
		data->timer.function = k3g_timer_func;

		/* the timer just fires off a work queue request.
		   We need a thread to read i2c (can be slow and blocking). */
		data->k3g_wq = create_singlethread_workqueue("k3g_wq");
		if (!data->k3g_wq) {
			err = -ENOMEM;
			pr_err("%s: could not create workqueue\n", __func__);
			goto err_create_workqueue;
	}
		/* this is the thread function we run on the work queue */
		INIT_WORK(&data->work, k3g_work_func);
	}

	i2c_set_clientdata(client, data);
	dev_set_drvdata(&input_dev->dev, data);

	gyro = data;

	/* register a char dev */
	printk(KERN_INFO "%s : register a char dev\n", __func__);
	err = register_chrdev(K3G_MAJOR,
			      "k3g",
			      &k3g_fops);

	/* create k3g-dev device class */
	k3g_gyro_dev_class = class_create(THIS_MODULE, "K3G_GYRO-dev");
	if (IS_ERR(k3g_gyro_dev_class))
		err = PTR_ERR(k3g_gyro_dev_class);

	printk(KERN_INFO "%s : Dopo class_create\n", __func__);

	/* create device node for k3g digital gyroscope */
	dev = device_create(k3g_gyro_dev_class, NULL,
		MKDEV(K3G_MAJOR, 0),
		NULL,
		"k3g");

	if (IS_ERR(dev))
		err = PTR_ERR(dev);

	if (device_create_file(dev, &dev_attr_gyro_power_on) < 0) {
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_gyro_power_on.attr.name);
	}

	if (device_create_file(dev, &dev_attr_gyro_get_temp) < 0) {
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_gyro_get_temp.attr.name);
	}

	if (device_create_file(dev, &dev_attr_gyro_selftest) < 0) {
		printk(KERN_ERR "Failed to create device file(%s)!\n",
			dev_attr_gyro_selftest.attr.name);
	}

	printk(KERN_INFO "%s : K3G device created successfully\n", __func__);

	return 0;

err_write_reg:
err_request_irq:
err_create_workqueue:
	device_remove_file(&input_dev->dev, &dev_attr_poll_delay);
err_device_create_file2:
	device_remove_file(&input_dev->dev, &dev_attr_enable);
err_device_create_file:
	input_unregister_device(data->input_dev);
err_input_register_device:
err_input_allocate_device:
	mutex_destroy(&data->lock);
err_read_reg:
	kfree(data);
exit:
	return err;
}

static int k3g_remove(struct i2c_client *client)
{
	int err = 0;
	struct k3g_data *k3g_data = i2c_get_clientdata(client);

	device_remove_file(&k3g_data->input_dev->dev, &dev_attr_enable);
	device_remove_file(&k3g_data->input_dev->dev, &dev_attr_poll_delay);

	if (k3g_data->enable)
		err = i2c_smbus_write_byte_data(k3g_data->client,
					CTRL_REG1, 0x00);
	if (k3g_data->interruptable) {
		if (!k3g_data->enable) /* no disable_irq before free_irq */
			enable_irq(k3g_data->client->irq);
		free_irq(k3g_data->client->irq, k3g_data);

	} else {
		hrtimer_cancel(&k3g_data->timer);
		cancel_work_sync(&k3g_data->work);
	destroy_workqueue(k3g_data->k3g_wq);
	}

	input_unregister_device(k3g_data->input_dev);
	mutex_destroy(&k3g_data->lock);
	kfree(k3g_data);

	return err;
}

static int k3g_suspend(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct k3g_data *k3g_data = i2c_get_clientdata(client);

	if (k3g_data->enable) {
		mutex_lock(&k3g_data->lock);
		if (!k3g_data->interruptable) {
			hrtimer_cancel(&k3g_data->timer);
			cancel_work_sync(&k3g_data->work);
		}
		err = i2c_smbus_write_byte_data(k3g_data->client,
						CTRL_REG1, 0x00);
		mutex_unlock(&k3g_data->lock);
	}

	return err;
}

static int k3g_resume(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct k3g_data *k3g_data = i2c_get_clientdata(client);

	if (k3g_data->enable) {
		mutex_lock(&k3g_data->lock);
		if (!k3g_data->interruptable)
			hrtimer_start(&k3g_data->timer,
				k3g_data->polling_delay, HRTIMER_MODE_REL);
		err = i2c_smbus_write_i2c_block_data(client,
				CTRL_REG1 | AC, sizeof(k3g_data->ctrl_regs),
							k3g_data->ctrl_regs);
		mutex_unlock(&k3g_data->lock);
	}

	return err;
}

static const struct dev_pm_ops k3g_pm_ops = {
	.suspend = k3g_suspend,
	.resume = k3g_resume
};

static const struct i2c_device_id k3g_id[] = {
	{ "k3g", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, k3g_id);

static struct i2c_driver k3g_driver = {
	.probe = k3g_probe,
	.remove = __devexit_p(k3g_remove),
	.id_table = k3g_id,
	.driver = {
		.pm = &k3g_pm_ops,
		.owner = THIS_MODULE,
		.name = "k3g"
	},
};

static int __init k3g_init(void)
{
	return i2c_add_driver(&k3g_driver);
}

static void __exit k3g_exit(void)
{
	i2c_del_driver(&k3g_driver);
}

module_init(k3g_init);
module_exit(k3g_exit);

MODULE_DESCRIPTION("k3g digital gyroscope driver");
MODULE_AUTHOR("Tim SK Lee Samsung Electronics <tim.sk.lee@samsung.com>");
MODULE_LICENSE("GPL");
