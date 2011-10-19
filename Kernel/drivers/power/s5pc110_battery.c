/*
 * linux/drivers/power/s3c6410_battery.c
 *
 * Battery measurement code for S3C6410 platform.
 *
 * based on palmtx_battery.c
 *
 * Copyright (C) 2009 Samsung Electronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <asm/mach-types.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mfd/max8998.h>
#include <linux/mfd/max8998-private.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <mach/battery.h>
#include <mach/hardware.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>
#include <mach/adc.h>
#include <plat/gpio-cfg.h>
#include <linux/android_alarm.h>
#include "s5pc110_battery.h"

#define BAT_POLLING_INTERVAL	10000
#define BAT_WAITING_INTERVAL	20000	/* 20 sec */
#define BAT_WAITING_COUNT	(BAT_WAITING_INTERVAL / BAT_POLLING_INTERVAL)
#define ADC_TOTAL_COUNT		10
#define ADC_DATA_ARR_SIZE	6

#define USE_CALL        (0x1 << 0)
#define USE_VIDEO       (0x1 << 1)
#define USE_MUSIC       (0x1 << 2)
#define USE_BROWSER     (0x1 << 3)
#define USE_HOTSPOT     (0x1 << 4)
#define USE_CAMERA      (0x1 << 5)
#define USE_DATA_CALL   (0x1 << 6)
#define USE_WIMAX       (0x1 << 7)

#define DISCONNECT_BAT_FULL		0x1
#define DISCONNECT_TEMP_OVERHEAT	0x2
#define DISCONNECT_TEMP_FREEZE		0x4
#define DISCONNECT_OVER_TIME		0x8
#define DISCONNECT_UNSPEC_FAILURE	0x16

#define ATTACH_USB	1
#define ATTACH_TA	2

#define USE_MODULE_TIMEOUT      (10*60*1000)    // 10 min (DG05_FINAL: 1 min -> 10 min)

#ifdef __SOC_TEST__
static int soc_test = 100;
#endif

#if 0
#define bat_dbg(fmt, ...) printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#define bat_info(fmt, ...) printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#else
#define bat_dbg(fmt, ...)
#define bat_info(fmt, ...) printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif
#ifdef CONFIG_KERNEL_DEBUG_SEC
extern void kernel_sec_clear_upload_magic_number(void); 
extern void kernel_sec_set_upload_magic_number(void);   
#endif
struct s5p_batt_block_temp *s5p_battery_block_temp; 
struct battery_info {
	u32 batt_temp;		/* Battery Temperature (C) from ADC */
	u32 batt_temp_adc;	/* Battery Temperature ADC value */
	u32 batt_health;	/* Battery Health (Authority) */
	u32 chg_current_adc;	/* Charge Current ADC value */
	u32 dis_reason;
	u32 batt_vcell;
	u32 batt_soc;
	u32 charging_status;
	bool batt_is_full;      /* 0 : Not full 1: Full */
	u32 decimal_point_level;        // lobat pwroff
};

struct adc_sample_info {
	unsigned int cnt;
	int total_adc;
	int average_adc;
	int adc_arr[ADC_TOTAL_COUNT];
	int index;
};

struct chg_data {
	struct device		*dev;
	struct max8998_dev	*iodev;
	struct work_struct	bat_work;
	struct max8998_charger_data *pdata;

	struct power_supply	psy_bat;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;
	struct workqueue_struct *monitor_wqueue;
	struct wake_lock	vbus_wake_lock;
	struct wake_lock	work_wake_lock;
	struct wake_lock	lowbat_wake_lock;
	struct adc_sample_info	adc_sample[ENDOFADC];
	struct battery_info	bat_info;
	struct mutex		mutex;
	struct timer_list	bat_work_timer;
	struct timer_list	bat_use_timer;

	enum cable_type_t	cable_status;
	bool			jig_status;
	int			charging;
	bool			set_charge_timeout;
	bool			lowbat_warning;
	int			present;
	u8			esafe;
	bool			set_batt_full;
	unsigned long		discharging_time;
	struct max8998_charger_callbacks callbacks;
	struct adc_channel_type s3c_adc_channel;
	uint			batt_use;
	uint			batt_use_wait;
#ifdef SPRINT_SLATE_TEST
        bool                    slate_test_mode;
#endif
};

static struct chg_data *pchg = NULL;	// pointer to chg initialized in probe function

static bool lpm_charging_mode;

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property max8998_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property s3c_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

//static void use_wimax_timer_func(unsigned long unused);
//static struct timer_list use_wimax_timer;

static ssize_t s3c_bat_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf);

static ssize_t s3c_bat_store_attrs(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count);

#define SEC_BATTERY_ATTR(_name)								\
{											\
	.attr = { .name = #_name, .mode = 0664, .owner = THIS_MODULE },	\
	.show = s3c_bat_show_attrs,							\
	.store = s3c_bat_store_attrs,								\
}

static struct device_attribute s3c_battery_attrs[] = {
  	SEC_BATTERY_ATTR(batt_vol),
  	SEC_BATTERY_ATTR(batt_vol_adc),
	SEC_BATTERY_ATTR(batt_temp),
	SEC_BATTERY_ATTR(batt_temp_adc),
	SEC_BATTERY_ATTR(charging_source),
	SEC_BATTERY_ATTR(fg_soc),
	SEC_BATTERY_ATTR(reset_soc),
	SEC_BATTERY_ATTR(fg_point_level),       // lobat pwroff
	SEC_BATTERY_ATTR(charging_mode_booting),
	SEC_BATTERY_ATTR(batt_temp_check),
	SEC_BATTERY_ATTR(batt_full_check),
        SEC_BATTERY_ATTR(auth_battery),	// Returns valid result if __VZW_AUTH_CHECK__ is defined.
        SEC_BATTERY_ATTR(batt_chg_current_aver),
	SEC_BATTERY_ATTR(batt_type), //to check only
#ifdef __SOC_TEST__
        SEC_BATTERY_ATTR(soc_test),
#endif
#ifdef SPRINT_SLATE_TEST
        SEC_BATTERY_ATTR(slate_test_mode),
#endif
#ifdef CONFIG_MACH_VICTORY
	SEC_BATTERY_ATTR(batt_v_f_adc),
	SEC_BATTERY_ATTR(call),
	SEC_BATTERY_ATTR(video),
	SEC_BATTERY_ATTR(music),
	SEC_BATTERY_ATTR(browser),
	SEC_BATTERY_ATTR(hotspot),
	SEC_BATTERY_ATTR(camera),
	SEC_BATTERY_ATTR(data_call),
	SEC_BATTERY_ATTR(wimax),
	SEC_BATTERY_ATTR(batt_use),
#endif
};

/* Wimax  may need it */
/*Currently not in use */
/*
struct s3c_battery_info {
        int present;
        int polling;
        unsigned int polling_interval;
        unsigned int device_state;

        struct battery_info bat_info;
#ifdef LPM_MODE
        unsigned int charging_mode_booting;
#endif
};
static struct s3c_battery_info s3c_bat_info;

#if defined (CONFIG_MACH_VICTORY)
static void use_wimax_timer_func(unsigned long unused)
{
        s3c_bat_info.device_state = s3c_bat_info.device_state & (~USE_WIMAX);
        printk("%s: OFF (0x%x) \n", __func__, s3c_bat_info.device_state);
}

int s3c_bat_use_wimax(int onoff)
{
        if (onoff)
        {
                del_timer_sync(&use_wimax_timer);
                s3c_bat_info.device_state = s3c_bat_info.device_state | USE_WIMAX;
                printk("%s: ON (0x%x) \n", __func__, s3c_bat_info.device_state);
        }
        else
        {
                mod_timer(&use_wimax_timer, jiffies + msecs_to_jiffies(USE_MODULE_TIMEOUT));
        }

        return s3c_bat_info.device_state;
}
EXPORT_SYMBOL(s3c_bat_use_wimax);
#endif
*/

static void max8998_lowbat_config(struct chg_data *chg, int on)
{
	struct i2c_client *i2c = chg->iodev->i2c;

	if (on) {
		if (!chg->lowbat_warning)
			max8998_update_reg(i2c, MAX8998_REG_ONOFF3, 0x1, 0x1); //lowbat1
		max8998_update_reg(i2c, MAX8998_REG_ONOFF3, 0x2, 0x2); //lowbat2
	} else
		max8998_update_reg(i2c, MAX8998_REG_ONOFF3, 0x0, 0x3);
}

static void max8998_lowbat_warning(struct chg_data *chg)
{
	bat_info("%s\n", __func__);
	if (chg->bat_info.decimal_point_level == 0)
                return ;        // already requested...

	wake_lock_timeout(&chg->lowbat_wake_lock, 5 * HZ);
	chg->lowbat_warning = 1;
}

static void max8998_lowbat_critical(struct chg_data *chg)
{
	if (chg->bat_info.decimal_point_level == 0)
                return ;        // already requested...
	
	bat_info("%s\n", __func__);
	wake_lock_timeout(&chg->lowbat_wake_lock, 30 * HZ);
	chg->bat_info.batt_soc = 0;
	chg->bat_info.decimal_point_level = 0;
}

static void s3c_clean_chg_current(struct chg_data *chg)
{
	int i;
	int channel = chg->s3c_adc_channel.s3c_adc_chg_current;

	/* clean data */
	for (i = 0; i < ADC_TOTAL_COUNT; i++)
		chg->adc_sample[channel].adc_arr[i] = 0;
	chg->adc_sample[channel].cnt = 0;
	chg->adc_sample[channel].total_adc = 0;
	chg->adc_sample[channel].average_adc = 0;
	chg->adc_sample[channel].index = 0;

	chg->bat_info.chg_current_adc = 0;
}

static int max8998_charging_control(struct chg_data *chg)
{
        struct i2c_client *i2c = chg->iodev->i2c;
	static int prev_charging = -1, prev_cable = -1;
	int ret;
	int topoff;

	if ((prev_charging == chg->charging) && (prev_cable == chg->cable_status))
		return 0;

	bat_info("%s : chg(%d) cable(%d) dis(%X) bat(%d,%d,%d), esafe(%d)\n", __func__,
		chg->charging, chg->cable_status, chg->bat_info.dis_reason,
		chg->bat_info.batt_soc, chg->set_batt_full, chg->bat_info.batt_is_full,
		chg->esafe);

	if (!chg->charging) {
		/* disable charging */
		s3c_clean_chg_current(chg);

		ret = max8998_write_reg(i2c, MAX8998_REG_CHGR2,
			(chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
			(MAX8998_CHGTIME_7HR	<< MAX8998_SHIFT_FT) |
			(MAX8998_CHGEN_DISABLE	<< MAX8998_SHIFT_CHGEN));
		if (ret < 0)
			goto err;
	} else {
		/* enable charging */
		if (chg->cable_status == CABLE_TYPE_AC) {
			/* termination current adc NOT used to detect full-charging */
			if (chg->pdata->termination_curr_adc > 0)
				topoff = MAX8998_TOPOFF_10;
			else
				topoff = MAX8998_TOPOFF_30;

			/* ac */
			ret = max8998_write_reg(i2c, MAX8998_REG_CHGR1,
				(topoff	<< MAX8998_SHIFT_TOPOFF) |
				(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
				(MAX8998_ICHG_600	<< MAX8998_SHIFT_ICHG));
			if (ret < 0)
				goto err;

			ret = max8998_write_reg(i2c, MAX8998_REG_CHGR2,
				(chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
				(MAX8998_CHGTIME_7HR	<< MAX8998_SHIFT_FT) |
				(MAX8998_CHGEN_ENABLE	<< MAX8998_SHIFT_CHGEN));
			if (ret < 0)
				goto err;
		} else {
			if (chg->pdata->termination_curr_adc > 0)
				topoff = MAX8998_TOPOFF_10;
			else
				topoff = MAX8998_TOPOFF_35;

			/* usb */
			ret = max8998_write_reg(i2c, MAX8998_REG_CHGR1,
				(topoff	<< MAX8998_SHIFT_TOPOFF) |
				(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
				(MAX8998_ICHG_475	<< MAX8998_SHIFT_ICHG));
			if (ret < 0)
				goto err;

			ret = max8998_write_reg(i2c, MAX8998_REG_CHGR2,
				(chg->esafe		<< MAX8998_SHIFT_ESAFEOUT) |
				(MAX8998_CHGTIME_7HR	<< MAX8998_SHIFT_FT) |
				(MAX8998_CHGEN_ENABLE	<< MAX8998_SHIFT_CHGEN));
			if (ret < 0)
				goto err;
		}
	}

	prev_charging = chg->charging;
	prev_cable = chg->cable_status;

	return 0;
err:
	pr_err("max8998_read_reg error\n");
	return ret;
}

static int max8998_check_vdcin(struct chg_data *chg)
{
        struct i2c_client *i2c = chg->iodev->i2c;
	u8 data = 0;
	int ret;

	ret = max8998_read_reg(i2c, MAX8998_REG_STATUS2, &data);

	if (ret < 0) {
		pr_err("max8998_read_reg error\n");
		return ret;
	}

	return (data & MAX8998_MASK_VDCIN) ? 1 : 0;
}

/*
static int max8998_check_valid_battery(struct chg_data *chg)
{
        struct i2c_client *i2c = chg->iodev->i2c;
	u8 data = 0;
	int ret;

	ret = max8998_read_reg(i2c, MAX8998_REG_STATUS2, &data);

	if (ret < 0) {
		pr_err("max8998_read_reg error\n");
		return ret;
	}

	return (data & MAX8998_MASK_DETBAT) ? 0 : 1;
}
*/

static void max8998_set_jig(struct max8998_charger_callbacks *ptr, bool attached)
{
	struct chg_data *chg = container_of(ptr, struct chg_data, callbacks);
	chg->jig_status = attached;
	bat_info("%s %d\n", __func__, (int)attached);
}


#ifdef __VZW_AUTH_CHECK__
/* debug info */
extern int vzw_rcode[8];
extern int vzw_crc1;
extern int vzw_crc2;
#endif

static void max8998_set_cable(struct max8998_charger_callbacks *ptr,
				enum cable_type_t status)
{
	struct chg_data *chg = container_of(ptr, struct chg_data, callbacks);
	chg->cable_status = status;
	chg->lowbat_warning = false;
	if (chg->esafe == MAX8998_ESAFE_ALLOFF)
		chg->esafe = MAX8998_USB_VBUS_AP_ON;
	bat_info("%s : cable_status = %d\n", __func__, status);

#ifdef __VZW_AUTH_CHECK__
	/* debug info */
	if ((status != CABLE_TYPE_NONE)
		&& (chg->bat_info.batt_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)) {
		int i;
		printk("/BATT_ID/ rom code =");
		for (i = 0; i < 8; i++)
			printk(" %d", vzw_rcode[i]);
		printk("\n");
		printk("/BATT_ID/ rom code crc = %d\n", vzw_crc1);
		printk("/BATT_ID/ crc = %d", vzw_crc2);
	}
#endif

	if (lpm_charging_mode &&
	    (max8998_check_vdcin(chg) != 1) &&
	    pm_power_off)
		pm_power_off();

	power_supply_changed(&chg->psy_ac);
	power_supply_changed(&chg->psy_usb);
	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);
}

static bool max8998_set_esafe(struct max8998_charger_callbacks *ptr, u8 esafe)
{
	struct chg_data *chg = container_of(ptr, struct chg_data, callbacks);
	if (esafe > 3) {
		pr_err("%s : esafe value must not be bigger than 3\n", __func__);
		return 0;
	}
	chg->esafe = esafe;
	max8998_update_reg(chg->iodev->i2c, MAX8998_REG_CHGR2,
		(esafe << MAX8998_SHIFT_ESAFEOUT), MAX8998_MASK_ESAFEOUT);
	bat_info("%s : esafe = %d\n", __func__, esafe);
	return 1;
}

static bool max8998_get_vdcin(struct max8998_charger_callbacks *ptr)
{
	struct chg_data *chg = container_of(ptr, struct chg_data, callbacks);
	return (max8998_check_vdcin(chg) == 1);
}

static void check_lpm_charging_mode(struct chg_data *chg)
{
	if (readl(S5P_INFORM5)) {
		lpm_charging_mode = 1;
		if (max8998_check_vdcin(chg) != 1)
			if (pm_power_off)
				pm_power_off();
	} else
		lpm_charging_mode = 0;

	bat_info("%s : lpm_charging_mode(%d)\n", __func__, lpm_charging_mode);
}

bool charging_mode_get(void)
{
	return lpm_charging_mode;
}
EXPORT_SYMBOL(charging_mode_get);

static int s3c_bat_get_property(struct power_supply *bat_ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chg_data *chg = container_of(bat_ps, struct chg_data, psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chg->bat_info.charging_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chg->bat_info.batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = chg->present;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = chg->bat_info.batt_temp;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* battery is always online */
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (chg->pdata &&
			 chg->pdata->psy_fuelgauge &&
			 chg->pdata->psy_fuelgauge->get_property &&
			 chg->pdata->psy_fuelgauge->get_property(
				chg->pdata->psy_fuelgauge, psp, val) < 0)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
#ifdef __SOC_TEST__
		chg->bat_info.batt_soc = soc_test;
#else 
		if ((chg->bat_info.batt_soc == 0) || (chg->bat_info.decimal_point_level == 0))
			val->intval = 0;
		else if (chg->set_batt_full)
			val->intval = 100;
		else if (chg->pdata &&
			 chg->pdata->psy_fuelgauge &&
			 chg->pdata->psy_fuelgauge->get_property &&
			 chg->pdata->psy_fuelgauge->get_property(
				chg->pdata->psy_fuelgauge, psp, val) < 0)
			return -EINVAL;
#endif
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s3c_usb_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chg_data *chg = container_of(ps, struct chg_data, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = ((chg->cable_status == CABLE_TYPE_USB) &&
			max8998_check_vdcin(chg));
	return 0;
}

static int s3c_ac_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct chg_data *chg = container_of(ps, struct chg_data, psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	val->intval = (chg->cable_status == CABLE_TYPE_AC);

	return 0;
}
unsigned int s3c_adc_tempreture;
static inline int s3c_adc_get_adc_data_ex(int channel) {
#ifdef CONFIG_MACH_CRESPO
        if (channel == s3c_adc_tempreture)
                return ((4096 - s3c_adc_get_adc_data(channel)) >> 4);
        else
                return (s3c_adc_get_adc_data(channel) >> 2);
#else
	/* Atlas, Victory and Garnett */
	if (channel == s3c_adc_tempreture)
		return (s3c_adc_get_adc_data(channel));
	else
		return (s3c_adc_get_adc_data(channel) >> 2);
#endif
}

static int s3c_bat_get_adc_data(int adc_ch)
{
	int adc_data;
	int adc_max = 0;
	int adc_min = 0;
	int adc_total = 0;
	int i;

	for (i = 0; i < ADC_DATA_ARR_SIZE; i++) {
		adc_data = s3c_adc_get_adc_data_ex(adc_ch);
		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (ADC_DATA_ARR_SIZE - 2);
}

static unsigned long calculate_average_adc(unsigned char channel,int adc, struct chg_data *chg)
{
	unsigned int cnt = 0;
	int total_adc = 0;
	int average_adc = 0;
	int index = 0;

	cnt = chg->adc_sample[channel].cnt;
	total_adc = chg->adc_sample[channel].total_adc;

	if (adc <= 0) {
		pr_err("%s : invalid adc : %d\n", __func__, adc);
		adc = chg->adc_sample[channel].average_adc;
	}

	if (cnt < ADC_TOTAL_COUNT) {
		chg->adc_sample[channel].adc_arr[cnt] = adc;
		chg->adc_sample[channel].index = cnt;
		chg->adc_sample[channel].cnt = ++cnt;

		total_adc += adc;
		average_adc = total_adc / cnt;
	} else {
		index = chg->adc_sample[channel].index;
		if (++index >= ADC_TOTAL_COUNT)
			index = 0;

		total_adc = total_adc - chg->adc_sample[channel].adc_arr[index] + adc;
		average_adc = total_adc / ADC_TOTAL_COUNT;

		chg->adc_sample[channel].adc_arr[index] = adc;
		chg->adc_sample[channel].index = index;
	}

	chg->adc_sample[channel].total_adc = total_adc;
	chg->adc_sample[channel].average_adc = average_adc;

#ifdef CONFIG_MACH_CRESPO
	chg->bat_info.batt_temp_adc = average_adc;
#endif

	return average_adc;
}

static unsigned long s3c_read_temp(struct chg_data *chg)
{
	int adc = 0;

	adc = s3c_bat_get_adc_data(chg->s3c_adc_channel.s3c_adc_temperature);

	return calculate_average_adc(chg->s3c_adc_channel.s3c_adc_temperature, adc, chg);
}

static unsigned long s3c_read_chg_current(struct chg_data *chg)
{
	int adc = 0;

	adc = s3c_bat_get_adc_data(chg->s3c_adc_channel.s3c_adc_chg_current);

	return calculate_average_adc(chg->s3c_adc_channel.s3c_adc_chg_current, adc, chg);
}

static int s3c_get_chg_current(struct chg_data *chg)
{
	static int count = 0;

	if (chg->charging && (chg->cable_status != CABLE_TYPE_NONE)) {
		chg->bat_info.chg_current_adc = s3c_read_chg_current(chg);
		//pr_info("%s: adc = %d\n", __func__, chg->bat_info.chg_current_adc);

		/* termination current adc NOT used to detect full-charging */
		if (chg->pdata->termination_curr_adc <= 0)
			goto skip;

		if ((chg->bat_info.batt_vcell / 1000 > 4000)
			&& (chg->bat_info.chg_current_adc < chg->pdata->termination_curr_adc)) {
			count++;
			if (count > 2) {
				chg->bat_info.batt_is_full = true;
				chg->set_batt_full = true;
				count = 0;
			}
		} else
			count = 0;
	} else {
		chg->bat_info.chg_current_adc = 0;
		count = 0;
	}

skip:
	return chg->bat_info.chg_current_adc;
}

static int s3c_get_bat_temp(struct chg_data *chg)
{
	int temp = 0;
	int temp_adc = s3c_read_temp(chg);
	int health = chg->bat_info.batt_health;
	int left_side = 0;
	int right_side = chg->pdata->adc_array_size - 1;
	int mid;
	int temp_high_block = s5p_battery_block_temp->temp_high_block;
	int temp_high_recover = s5p_battery_block_temp->temp_high_recover;
	int temp_low_recover = s5p_battery_block_temp->temp_low_recover;
	int temp_low_block = s5p_battery_block_temp->temp_low_block;
	int temp_curr;

#ifndef CONFIG_MACH_CRESPO
	/* Atlas, Victory and Garnett */
#ifdef CONFIG_S5PV210_GARNETT_DELTA
	/* to reduce deviation.. (hw team requirement) */
	temp_adc = (4096 - temp_adc) >> 2;
#else
	/* Atlas and Victory */
	temp_adc = (4096 - temp_adc) >> 4;
#endif
#endif	/* CONFIG_MACH_CRESPO */

	/* Celcius mapping */
	while (left_side <= right_side) {
		mid = (left_side + right_side) / 2 ;
		if (mid == 0 ||
		    mid == chg->pdata->adc_array_size - 1 ||
		    (chg->pdata->adc_table[mid].adc_value <= temp_adc &&
		     chg->pdata->adc_table[mid+1].adc_value > temp_adc)) {
			temp = chg->pdata->adc_table[mid].temperature;
			break;
		} else if (temp_adc - chg->pdata->adc_table[mid].adc_value > 0)
			left_side = mid + 1;
		else
			right_side = mid - 1;
	}

	chg->bat_info.batt_temp = temp;

#ifdef CONFIG_MACH_CRESPO
	temp_curr = temp;

#else
	chg->bat_info.batt_temp_adc = temp_adc;
	temp_curr = temp_adc;

#ifdef CONFIG_MACH_VICTORY
	if (chg->batt_use) {
		temp_high_block = s5p_battery_block_temp->temp_high_event_block;
	}
#endif

	/* Temperature control */
	if (lpm_charging_mode) {
		temp_high_block = s5p_battery_block_temp->temp_high_block_lpm;
		temp_high_recover = s5p_battery_block_temp->temp_high_recover_lpm;
		temp_low_recover = s5p_battery_block_temp->temp_low_recover_lpm;
		temp_low_block = s5p_battery_block_temp->temp_low_block_lpm;
	}
#endif	/* !CONFIG_MACH_CRESPO */

	if (temp_curr >= temp_high_block) {
		if (health != POWER_SUPPLY_HEALTH_OVERHEAT &&
		    health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (temp_curr <= temp_high_recover && temp_curr >= temp_low_recover) {
		if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
		    health == POWER_SUPPLY_HEALTH_COLD)
			chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (temp_curr <= temp_low_block) {
		if (health != POWER_SUPPLY_HEALTH_COLD &&
		    health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_COLD;
	}

	return temp;
}

static void s3c_bat_discharge_reason(struct chg_data *chg)
{
	int discharge_reason;
	ktime_t ktime;
	struct timespec cur_time;
	union power_supply_propval value;
	int temp_high_recover = s5p_battery_block_temp->temp_high_recover;
	int temp_low_recover = s5p_battery_block_temp->temp_low_recover;
	static int recharge_count = 0;

	if (chg->pdata &&
	    chg->pdata->psy_fuelgauge &&
	    chg->pdata->psy_fuelgauge->get_property) {
		chg->pdata->psy_fuelgauge->get_property(
			chg->pdata->psy_fuelgauge, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
		chg->bat_info.batt_vcell = value.intval;

		chg->pdata->psy_fuelgauge->get_property(
			chg->pdata->psy_fuelgauge, POWER_SUPPLY_PROP_CAPACITY, &value);
		if ((chg->bat_info.charging_status != POWER_SUPPLY_STATUS_DISCHARGING) ||
		    (chg->bat_info.batt_soc > value.intval))
#ifndef __SOC_TEST__
			chg->bat_info.batt_soc = value.intval;
#else
			chg->bat_info.batt_soc = soc_test;
#endif
	}

#ifndef CONFIG_MACH_CRESPO
	if (lpm_charging_mode) {
		temp_high_recover = s5p_battery_block_temp->temp_high_recover_lpm;
		temp_low_recover = s5p_battery_block_temp->temp_low_recover_lpm;
	}
#endif

	discharge_reason = chg->bat_info.dis_reason & 0xf;

	if ((discharge_reason & DISCONNECT_BAT_FULL) &&
	    chg->bat_info.batt_vcell < RECHARGE_COND_VOLTAGE) {
		if (recharge_count < BAT_WAITING_COUNT)
			recharge_count++;
		else {
			chg->bat_info.dis_reason &= ~DISCONNECT_BAT_FULL;
			recharge_count = 0;
			bat_info("batt recharging (vol=%d)\n", chg->bat_info.batt_vcell);
		}
	} else if ((discharge_reason & DISCONNECT_BAT_FULL) &&
	    chg->bat_info.batt_vcell >= RECHARGE_COND_VOLTAGE)
		recharge_count = 0;

#ifdef CONFIG_MACH_CRESPO
	if ((discharge_reason & DISCONNECT_TEMP_OVERHEAT) &&
	    chg->bat_info.batt_temp <= temp_high_recover)
		chg->bat_info.dis_reason &= ~DISCONNECT_TEMP_OVERHEAT;

	if ((discharge_reason & DISCONNECT_TEMP_FREEZE) &&
	    chg->bat_info.batt_temp >= temp_low_recover)
		chg->bat_info.dis_reason &= ~DISCONNECT_TEMP_FREEZE;
#else
	/* Atlas, Victory and Garnett */
	if ((discharge_reason & DISCONNECT_TEMP_OVERHEAT) &&
	    chg->bat_info.batt_temp_adc <= temp_high_recover)
		chg->bat_info.dis_reason &= ~DISCONNECT_TEMP_OVERHEAT;

	if ((discharge_reason & DISCONNECT_TEMP_FREEZE) &&
	    chg->bat_info.batt_temp_adc >= temp_low_recover)
		chg->bat_info.dis_reason &= ~DISCONNECT_TEMP_FREEZE;
#endif

	if ((discharge_reason & DISCONNECT_OVER_TIME) &&
	    chg->bat_info.batt_vcell < RECHARGE_COND_VOLTAGE) {
		if (recharge_count < BAT_WAITING_COUNT)
			recharge_count++;
		else {
			chg->bat_info.dis_reason &= ~DISCONNECT_OVER_TIME;
			recharge_count = 0;
			bat_info("batt recharging (vol=%d)\n", chg->bat_info.batt_vcell);
		}
	} else if ((discharge_reason & DISCONNECT_OVER_TIME) &&
	    chg->bat_info.batt_vcell >= RECHARGE_COND_VOLTAGE)
		recharge_count = 0;

	if (chg->bat_info.batt_is_full)
		chg->bat_info.dis_reason |= DISCONNECT_BAT_FULL;

	if (chg->bat_info.batt_health != POWER_SUPPLY_HEALTH_GOOD)
		chg->bat_info.dis_reason |=
			(chg->bat_info.batt_health == POWER_SUPPLY_HEALTH_OVERHEAT) ?
			DISCONNECT_TEMP_OVERHEAT : DISCONNECT_TEMP_FREEZE;

#ifdef __VZW_AUTH_CHECK__
	if (chg->bat_info.batt_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
		chg->bat_info.dis_reason |= DISCONNECT_UNSPEC_FAILURE;
#endif

	ktime = alarm_get_elapsed_realtime();
	cur_time = ktime_to_timespec(ktime);

	if (chg->discharging_time &&
	    cur_time.tv_sec > chg->discharging_time) {
		chg->set_charge_timeout = true;
		chg->bat_info.dis_reason |= DISCONNECT_OVER_TIME;
		chg->set_batt_full = true;
	}

	bat_dbg("bat(%d,%d) tmp(%d,%d) full(%d,%d) cable(%d) chg(%d) dis(%X)\n",
		chg->bat_info.batt_soc, chg->bat_info.batt_vcell/1000,
		chg->bat_info.batt_temp, chg->bat_info.batt_temp_adc,
		chg->set_batt_full, chg->bat_info.batt_is_full,
		chg->cable_status, chg->bat_info.charging_status, chg->bat_info.dis_reason);
}

#ifdef __VZW_AUTH_CHECK__
extern int verizon_batt_auth_full_check(void);
extern int verizon_batt_auth_check(void);

static int batt_auth_full_check = 0;

static int s3c_bat_check_v_f(struct chg_data *chg)
{
	int retval = 0;

	if (batt_auth_full_check == 0) {
		retval = verizon_batt_auth_full_check();
		batt_auth_full_check = 1;
		if (!retval) {
			chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			bat_info("/BATT_ID/ %s failed\n", __func__);
			return 0;
		}
		
		bat_info("/BATT_ID/ %s passed\n", __func__);
	} else {
		retval = verizon_batt_auth_check();
		if (!retval)
			return 0;
	}

	return 1;
}
#else

#ifndef  CONFIG_MACH_FORTE
/* Sprint V_F check */
static int s3c_bat_check_v_f(struct chg_data *chg)
{
	int retval = 0;
	int adc_vf = 0;

	adc_vf = s3c_bat_get_adc_data(chg->s3c_adc_channel.s3c_adc_v_f);

	if ((0 <= adc_vf) && (adc_vf <= 50)){
		if (chg->bat_info.batt_health ==  POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
		chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
		retval = 1;
             }
	else {
		chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		bat_info("/V_F/ %s failed (adc = %d)\n", __func__, adc_vf);
		retval = 0;
	}

	return retval;
}
#endif

#endif

extern int set_tsp_for_ta_detect(int state);
extern int fsa9480_get_dock_status(void);

static int s3c_cable_status_update(struct chg_data *chg)
{
	int ret;
	bool vdc_status;
	ktime_t ktime;
	struct timespec cur_time;
	static bool prev_vdc_status = 0;

	/* if max8998 has detected vdcin */
	if (max8998_check_vdcin(chg) == 1) {
		vdc_status = 1;
		#ifdef CONFIG_KERNEL_DEBUG_SEC
        	// Clear upload magic number to protect from sudden power-off...
        	if ((chg->bat_info.batt_soc <= LOW_BATT_COND_LEVEL) &&
                    (chg->bat_info.batt_vcell/1000 + 35 <= LOW_BATT_COND_VOLTAGE))          
        		{
                		kernel_sec_clear_upload_magic_number();
        		}
		#endif

		pr_debug("%s: cable_status = %d, doc_status = %d\n", __func__, chg->cable_status, fsa9480_get_dock_status());
		if (fsa9480_get_dock_status() && chg->cable_status == CABLE_TYPE_NONE) {
			chg->cable_status = CABLE_TYPE_AC;
			chg->lowbat_warning = false;
			if (chg->esafe == MAX8998_ESAFE_ALLOFF)
				chg->esafe = MAX8998_USB_VBUS_AP_ON;
			pr_debug("%s : cable_status = AC\n", __func__);
			power_supply_changed(&chg->psy_ac);
			power_supply_changed(&chg->psy_usb);
		}

		if (chg->bat_info.dis_reason) {
			/* have vdcin, but cannot charge */
			chg->charging = 0;
			ret = max8998_charging_control(chg);
			if (ret < 0)
				goto err;
			if (chg->bat_info.dis_reason & 
			    (DISCONNECT_TEMP_OVERHEAT | DISCONNECT_TEMP_FREEZE))
				chg->set_batt_full = false;
			chg->bat_info.charging_status = chg->set_batt_full ?
				POWER_SUPPLY_STATUS_FULL :
				POWER_SUPPLY_STATUS_NOT_CHARGING;
			chg->discharging_time = 0;
			chg->bat_info.batt_is_full = false;
			goto update;
		} else if (chg->discharging_time == 0) {
			ktime = alarm_get_elapsed_realtime();
			cur_time = ktime_to_timespec(ktime);
			chg->discharging_time =
				(chg->set_batt_full || chg->set_charge_timeout) ?
				cur_time.tv_sec + TOTAL_RECHARGING_TIME :
				cur_time.tv_sec + TOTAL_CHARGING_TIME;
		}

		/* able to charge */
#ifdef SPRINT_SLATE_TEST
                if (chg->slate_test_mode){
                     chg->charging =0;
                     chg->cable_status = CABLE_TYPE_NONE;
                     }
                else
                     chg->charging =1;
#else
		chg->charging = 1;
#endif
		chg->bat_info.decimal_point_level = 1;  // lobat pwroff
		ret = max8998_charging_control(chg);
		if (ret < 0)
			goto err;
#ifdef SPRINT_SLATE_TEST
                chg->bat_info.charging_status = chg->set_batt_full ?
                        POWER_SUPPLY_STATUS_FULL : (chg->slate_test_mode ? POWER_SUPPLY_STATUS_DISCHARGING :POWER_SUPPLY_STATUS_CHARGING);
#else
		chg->bat_info.charging_status = chg->set_batt_full ?
			POWER_SUPPLY_STATUS_FULL : POWER_SUPPLY_STATUS_CHARGING;
#endif
	} else {
		vdc_status = 0;
		/* no vdc in, not able to charge */
		chg->charging = 0;

		pr_debug("%s: cable_status = %d, doc_status = %d\n", __func__, chg->cable_status, fsa9480_get_dock_status());
		if (fsa9480_get_dock_status() && chg->cable_status != CABLE_TYPE_NONE) {
			chg->cable_status = CABLE_TYPE_NONE;
			chg->lowbat_warning = false;
			if (chg->esafe == MAX8998_ESAFE_ALLOFF)
				chg->esafe = MAX8998_USB_VBUS_AP_ON;
			pr_debug("%s : cable_status = NONE\n", __func__);

			if (lpm_charging_mode &&
					(max8998_check_vdcin(chg) != 1) &&
					pm_power_off)
				pm_power_off();

			power_supply_changed(&chg->psy_ac);
			power_supply_changed(&chg->psy_usb);
		}

		ret = max8998_charging_control(chg);
		if (ret < 0)
			goto err;

		chg->bat_info.charging_status = POWER_SUPPLY_STATUS_DISCHARGING;

		chg->bat_info.batt_is_full = false;
		chg->set_charge_timeout = false;
		chg->set_batt_full = false;
		chg->bat_info.dis_reason = 0;
		chg->discharging_time = 0;

#if 0
		if (lpm_charging_mode && pm_power_off)
			pm_power_off();
#endif
	}

update:
	if (vdc_status)
		wake_lock(&chg->vbus_wake_lock);
	else
		wake_lock_timeout(&chg->vbus_wake_lock, HZ / 5);
//		wake_unlock(&chg->vbus_wake_lock);
	
	if (vdc_status != prev_vdc_status) {
		set_tsp_for_ta_detect(vdc_status);
		prev_vdc_status = vdc_status;
	}

	return 0;
err:
	return ret;
}

static void s3c_bat_work(struct work_struct *work)
{
	struct chg_data *chg = container_of(work, struct chg_data, bat_work);
	int ret;
	static int cnt = 0;
	mutex_lock(&chg->mutex);

	s3c_get_bat_temp(chg);
  #ifndef CONFIG_MACH_FORTE
	s3c_bat_check_v_f(chg);
  #endif
	s3c_bat_discharge_reason(chg);

#if defined( CONFIG_MACH_ATLAS) || defined(CONFIG_MACH_FORTE)
	/* low battery check by voltage */
	if ((chg->bat_info.batt_vcell < 3400000) && (chg->cable_status == CABLE_TYPE_NONE))
		cnt++;
	else
		cnt = 0;

	if (cnt > 2) {
		bat_info("%s: low battery\n", __func__);
		chg->bat_info.decimal_point_level = 0;
		if (cnt > 10)
			cnt = 10;
	}
#endif

	ret = s3c_cable_status_update(chg);
	if (ret < 0)
		goto err;

	s3c_get_chg_current(chg);

	mutex_unlock(&chg->mutex);

	power_supply_changed(&chg->psy_bat);

	mod_timer(&chg->bat_work_timer, jiffies + msecs_to_jiffies(BAT_POLLING_INTERVAL));

	wake_unlock(&chg->work_wake_lock);
	return;
err:
	mutex_unlock(&chg->mutex);
	wake_unlock(&chg->work_wake_lock);
	pr_err("battery workqueue fail\n");
}

static void s3c_bat_work_timer_func(unsigned long param)
{
	struct chg_data *chg = (struct chg_data *)param;

	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);
}



static void s3c_bat_use_timer_func(unsigned long param)
{
	struct chg_data *chg = (struct chg_data *)param;

	chg->batt_use &= (~chg->batt_use_wait);
	pr_info("/BATT_USE/ timer expired (0x%x)\n", chg->batt_use);
}

static void s3c_bat_use_module(struct chg_data *chg, int module, int enable)
{
	del_timer_sync(&chg->bat_use_timer);
	chg->batt_use &= (~chg->batt_use_wait); // apply previous clear request

	if (enable) {
		chg->batt_use_wait = 0;
		chg->batt_use |= module;
		pr_info("/BATT_USE/ use module 0x%x\n", chg->batt_use);
	} else {
		if (chg->batt_use == 0)
			return;	/* nothing to clear */
		chg->batt_use_wait = module;
		mod_timer(&chg->bat_use_timer, jiffies + msecs_to_jiffies(USE_MODULE_TIMEOUT));
		pr_info("/BATT_USE/ start timer (curr 0x%x, wait 0x%x)\n",
			chg->batt_use, chg->batt_use_wait);
	}
}

#ifdef CONFIG_MACH_VICTORY
int s3c_bat_use_wimax(int onoff)
{
	struct chg_data *chg = pchg;

	s3c_bat_use_module(chg, USE_WIMAX, onoff);
}
EXPORT_SYMBOL(s3c_bat_use_wimax);
#else
int s3c_bat_use_wimax(int onoff)
{
	return 0;
}
EXPORT_SYMBOL(s3c_bat_use_wimax);
#endif

extern void max17040_reset_soc(void);

static ssize_t s3c_bat_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct chg_data *chg = container_of(psy, struct chg_data, psy_bat);
	int i = 0;
	const ptrdiff_t off = attr - s3c_battery_attrs;
	union power_supply_propval value;

	switch (off) {
	case BATT_VOL:
		if (chg->pdata &&
		    chg->pdata->psy_fuelgauge &&
		    chg->pdata->psy_fuelgauge->get_property) {
			chg->pdata->psy_fuelgauge->get_property(
				chg->pdata->psy_fuelgauge,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
			chg->bat_info.batt_vcell = value.intval;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_vcell/1000);
		break;
	case BATT_VOL_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 0);
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_temp);
		break;
	case BATT_TEMP_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_temp_adc);
		break;
	case BATT_CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->cable_status);
		break;
	case BATT_FG_SOC:
		if (chg->bat_info.decimal_point_level == 0)     // lobat pwroff
		{
                        i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                                chg->bat_info.batt_soc);   // maybe 0	
                }
		else {
			if (chg->pdata &&
			    chg->pdata->psy_fuelgauge &&
			    chg->pdata->psy_fuelgauge->get_property) {
				chg->pdata->psy_fuelgauge->get_property(
					chg->pdata->psy_fuelgauge,
					POWER_SUPPLY_PROP_CAPACITY, &value);
#ifndef __SOC_TEST__
				chg->bat_info.batt_soc = value.intval;
#else
				chg->bat_info.batt_soc = soc_test;
#endif
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_soc);
		}
		break;
	case BATT_DECIMAL_POINT_LEVEL:  // lobat pwroff
           i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                        chg->bat_info.decimal_point_level);
                break;
	case CHARGING_MODE_BOOTING:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", lpm_charging_mode);
		break;
	case BATT_TEMP_CHECK:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", chg->bat_info.batt_health);
		break;
	case BATT_FULL_CHECK:
		if (chg->bat_info.charging_status == POWER_SUPPLY_STATUS_FULL)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 1);
		else 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 0);
		break;
        case AUTH_BATTERY: 
#ifdef  __VZW_AUTH_CHECK__
		if (chg->jig_status)
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 1);
		else
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				(chg->bat_info.batt_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) ?
				0 : 1);
#else
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 1);
#endif
                break;
        case BATT_CHG_CURRENT_AVER:
                i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			chg->charging ? chg->bat_info.chg_current_adc : 0);
//                                s3c_bat_get_adc_data(chg->s3c_adc_channel.s3c_adc_chg_current));
                break;
	case BATT_TYPE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "SDI_SDI\n");
		break;
#ifdef __SOC_TEST__
        case SOC_TEST:
                i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                        soc_test);
                break;
#endif
#ifdef CONFIG_MACH_VICTORY
	case BATT_V_F_ADC:
                i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
                                s3c_bat_get_adc_data(chg->s3c_adc_channel.s3c_adc_v_f));
		break;
#endif

#ifdef CONFIG_MACH_VICTORY
	case BATT_USE_CALL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_CALL) ? 1 : 0);
		break;
	case BATT_USE_VIDEO:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_VIDEO) ? 1 : 0);
		break;
	case BATT_USE_MUSIC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_MUSIC) ? 1 : 0);
		break;
	case BATT_USE_BROWSER:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_BROWSER) ? 1 : 0);
		break;
	case BATT_USE_HOTSPOT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_HOTSPOT) ? 1 : 0);
		break;
	case BATT_USE_CAMERA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_CAMERA) ? 1 : 0);
		break;
	case BATT_USE_DATA_CALL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_DATA_CALL) ? 1 : 0);
		break;
	case BATT_USE_WIMAX:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(chg->batt_use & USE_WIMAX) ? 1 : 0);
		break;
	case BATT_USE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			chg->batt_use);
		break;
#endif
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t s3c_bat_store_attrs(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct chg_data *chg = container_of(psy, struct chg_data, psy_bat);
	int x = 0;
	int ret = 0;
	const ptrdiff_t off = attr - s3c_battery_attrs;

	switch (off) {
	case BATT_RESET_SOC:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 1)
				max17040_reset_soc();
			ret = count;
		}
		break;
	case CHARGING_MODE_BOOTING:
		if (sscanf(buf, "%d\n", &x) == 1) {
			lpm_charging_mode = x;
			ret = count;
		}
		break;
#ifdef __SOC_TEST__
        case SOC_TEST:
                if (sscanf(buf, "%d\n", &x) == 1) {
                        soc_test = x;
                        ret = count;
                }
                break;
#endif
#ifdef SPRINT_SLATE_TEST
        case SLATE_TEST_MODE:
             if(strncmp(buf, "1", 1) == 0)
                    chg->slate_test_mode =true;
             else
                    chg->slate_test_mode =false;
             break;                   
#endif
#ifdef CONFIG_MACH_VICTORY
	case BATT_USE_CALL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_CALL, x);
			ret = count;
		}
		break;
	case BATT_USE_VIDEO:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_VIDEO, x);
			ret = count;
		}
		break;
	case BATT_USE_MUSIC:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_MUSIC, x);
			ret = count;
		}
		break;
	case BATT_USE_BROWSER:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_BROWSER, x);
			ret = count;
		}
		break;
	case BATT_USE_HOTSPOT:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_HOTSPOT, x);
			ret = count;
		}
		break;
	case BATT_USE_CAMERA:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_CAMERA, x);
			ret = count;
		}
		break;
	case BATT_USE_DATA_CALL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_DATA_CALL, x);
			ret = count;
		}
		break;
	case BATT_USE_WIMAX:
		if (sscanf(buf, "%d\n", &x) == 1) {
			s3c_bat_use_module(chg, USE_WIMAX, x);
			ret = count;
		}
		break;
#endif
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int s3c_bat_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(s3c_battery_attrs); i++) {
		rc = device_create_file(dev, &s3c_battery_attrs[i]);
		if (rc)
			goto s3c_attrs_failed;
	}
	goto succeed;

s3c_attrs_failed:
	while (i--)
		device_remove_file(dev, &s3c_battery_attrs[i]);
succeed:
	return rc;
}

static irqreturn_t max8998_int_work_func(int irq, void *max8998_chg)
{
	int ret;
	int topoff;
	u8 data2=0;
	u8 data[MAX8998_NUM_IRQ_REGS];

	struct chg_data *chg = max8998_chg;
        struct i2c_client *i2c = chg->iodev->i2c;

	bat_info("%s\n", __func__);

	ret = max8998_bulk_read(i2c, MAX8998_REG_IRQ1, MAX8998_NUM_IRQ_REGS, data);
	if (ret < 0)
		goto err;

	wake_lock(&chg->work_wake_lock);

	if (data[MAX8998_REG_IRQ3] & MAX8998_IRQ_TOPOFFR_MASK) {
		bat_info("%s : topoff intr(%d)\n", __func__, chg->set_batt_full);
#ifndef CONFIG_MACH_FORTE
		if (s3c_bat_check_v_f(chg) == 0)
			goto end;
#else
	ret = max8998_read_reg(i2c, MAX8998_REG_STATUS2, &data2);//FET disable flag is set after battery removal
	if(data2 & 0x28)
	{
		
		bat_info("%s : topoff stop charging ******* %d \n", __func__,data2);
		writel(readl(S5P_INFORM6)& 0xFFFF00FF, S5P_INFORM6);
		#ifdef CONFIG_KERNEL_DEBUG_SEC
		kernel_sec_clear_upload_magic_number();
		
		#endif
		chg->charging =0;
		max8998_charging_control(chg);
		goto end;
	}
#endif

		if (chg->set_batt_full)
			chg->bat_info.batt_is_full = true;
		else {
			chg->set_batt_full = true;

			if (chg->pdata->termination_curr_adc > 0)
				topoff = MAX8998_TOPOFF_10;
			else if (chg->cable_status == CABLE_TYPE_AC)
				topoff = MAX8998_TOPOFF_30;
			else 
				topoff = MAX8998_TOPOFF_35;

			if (chg->cable_status == CABLE_TYPE_AC)
				max8998_write_reg(i2c, MAX8998_REG_CHGR1,
					(topoff	<< MAX8998_SHIFT_TOPOFF) |
					(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
					(MAX8998_ICHG_600	<< MAX8998_SHIFT_ICHG));
			else if (chg->cable_status == CABLE_TYPE_USB)
				max8998_write_reg(i2c, MAX8998_REG_CHGR1,
					(topoff	<< MAX8998_SHIFT_TOPOFF) |
					(MAX8998_RSTR_DISABLE	<< MAX8998_SHIFT_RSTR) |
					(MAX8998_ICHG_475	<< MAX8998_SHIFT_ICHG));
		}
	}

	if (data[MAX8998_REG_IRQ4] & MAX8998_IRQ_LOBAT1_MASK)
		max8998_lowbat_warning(chg);

	if (data[MAX8998_REG_IRQ4] & MAX8998_IRQ_LOBAT2_MASK)
		max8998_lowbat_critical(chg);

end:
	queue_work(chg->monitor_wqueue, &chg->bat_work);

	return IRQ_HANDLED;
err:
	pr_err("%s : pmic read error\n", __func__);
	return IRQ_HANDLED;
}

static __devinit int max8998_charger_probe(struct platform_device *pdev)
{
	struct max8998_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max8998_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct chg_data *chg;
        struct i2c_client *i2c = iodev->i2c;
	int ret = 0;

	bat_info("%s : MAX8998 Charger Driver Loading\n", __func__);

	chg = kzalloc(sizeof(*chg), GFP_KERNEL);
	if (!chg)
		return -ENOMEM;

	chg->iodev = iodev;
	chg->pdata = pdata->charger;

	if (!chg->pdata || !chg->pdata->adc_table || !pdata->s3c_adc_channel) {
		pr_err("%s : No platform data & adc_table & adc channel supplied\n", __func__);
		ret = -EINVAL;
		goto err_bat_table;
	}
	
	chg->psy_bat.name = "battery";
	chg->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	chg->psy_bat.properties = max8998_battery_props;
	chg->psy_bat.num_properties = ARRAY_SIZE(max8998_battery_props);
	chg->psy_bat.get_property = s3c_bat_get_property;

	chg->psy_usb.name = "usb";
	chg->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	chg->psy_usb.supplied_to = supply_list;
	chg->psy_usb.num_supplicants = ARRAY_SIZE(supply_list);
	chg->psy_usb.properties = s3c_power_properties;
	chg->psy_usb.num_properties = ARRAY_SIZE(s3c_power_properties);
	chg->psy_usb.get_property = s3c_usb_get_property;

	chg->psy_ac.name = "ac";
	chg->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	chg->psy_ac.supplied_to = supply_list;
	chg->psy_ac.num_supplicants = ARRAY_SIZE(supply_list);
	chg->psy_ac.properties = s3c_power_properties;
	chg->psy_ac.num_properties = ARRAY_SIZE(s3c_power_properties);
	chg->psy_ac.get_property = s3c_ac_get_property;

	chg->present = 1;
	chg->bat_info.batt_health = POWER_SUPPLY_HEALTH_GOOD;
	chg->bat_info.batt_is_full = false;
	chg->bat_info.decimal_point_level = 1;  // lobat pwroff
	chg->set_batt_full = false;
	chg->set_charge_timeout = false;
	
	chg->cable_status = CABLE_TYPE_NONE;
	chg->jig_status = false;
	chg->esafe = MAX8998_USB_VBUS_AP_ON;

	chg->s3c_adc_channel.s3c_adc_voltage = pdata->s3c_adc_channel->s3c_adc_voltage;
	chg->s3c_adc_channel.s3c_adc_chg_current = pdata->s3c_adc_channel->s3c_adc_chg_current;
	chg->s3c_adc_channel.s3c_adc_temperature = pdata->s3c_adc_channel->s3c_adc_temperature;
#ifdef CONFIG_MACH_VICTORY
	chg->s3c_adc_channel.s3c_adc_v_f = pdata->s3c_adc_channel->s3c_adc_v_f;
	chg->s3c_adc_channel.s3c_adc_hw_version = pdata->s3c_adc_channel->s3c_adc_hw_version;
#endif

	chg->batt_use = 0;

        s3c_adc_tempreture = pdata->s3c_adc_channel->s3c_adc_temperature;
        s5p_battery_block_temp = pdata->s5pc110_batt_block_temp;
	mutex_init(&chg->mutex);

	platform_set_drvdata(pdev, chg);

	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM1,
		~(MAX8998_IRQ_DCINR_MASK | MAX8998_IRQ_DCINF_MASK));
	if (ret < 0)
		goto err_kfree;

	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM2, 0xFF);
	if (ret < 0)
		goto err_kfree;

	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM3, ~MAX8998_IRQ_TOPOFFR_MASK);
	if (ret < 0)
		goto err_kfree;

	ret = max8998_write_reg(i2c, MAX8998_REG_IRQM4,
		~(MAX8998_IRQ_LOBAT2_MASK | MAX8998_IRQ_LOBAT1_MASK));
	if (ret < 0)
		goto err_kfree;

	ret = max8998_update_reg(i2c, MAX8998_REG_ONOFF3,
		(1 << MAX8998_SHIFT_ENBATTMON), MAX8998_MASK_ENBATTMON);
	if (ret < 0)
		goto err_kfree;

	ret = max8998_update_reg(i2c, MAX8998_REG_ONOFF3,
		(1 << MAX8998_SHIFT_ELBCNFG1), MAX8998_MASK_ELBCNFG2);
	if (ret < 0)
		goto err_kfree;

	ret = max8998_update_reg(i2c, MAX8998_REG_LBCNFG1, 0x7, 0x37); //3.57V
	if (ret < 0)
		goto err_kfree;

	ret = max8998_update_reg(i2c, MAX8998_REG_LBCNFG2, 0x5, 0x37); //3.4V
	if (ret < 0)
		goto err_kfree;
	max8998_lowbat_config(chg, 0);

	wake_lock_init(&chg->vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
	wake_lock_init(&chg->work_wake_lock, WAKE_LOCK_SUSPEND, "max8998-charger");
	wake_lock_init(&chg->lowbat_wake_lock, WAKE_LOCK_SUSPEND, "max8998-lowbat");

	INIT_WORK(&chg->bat_work, s3c_bat_work);
	setup_timer(&chg->bat_work_timer, s3c_bat_work_timer_func, (unsigned long)chg);
	setup_timer(&chg->bat_use_timer, s3c_bat_use_timer_func, (unsigned long)chg);

	chg->monitor_wqueue = create_freezeable_workqueue(dev_name(&pdev->dev));
	if (!chg->monitor_wqueue) {
		pr_err("%s : Failed to create freezeable workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_wake_lock;
	}

	check_lpm_charging_mode(chg);

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &chg->psy_bat);
	if (ret) {
		pr_err("%s : Failed to register power supply psy_bat\n", __func__);
		goto err_wqueue;
	}

	ret = power_supply_register(&pdev->dev, &chg->psy_usb);
	if (ret) {
		pr_err("%s : Failed to register power supply psy_usb\n", __func__);
		goto err_supply_unreg_bat;
	}

	ret = power_supply_register(&pdev->dev, &chg->psy_ac);
	if (ret) {
		pr_err("%s : Failed to register power supply psy_ac\n", __func__);
		goto err_supply_unreg_usb;
	}

	ret = request_threaded_irq(iodev->i2c->irq, NULL, max8998_int_work_func,
				   IRQF_TRIGGER_FALLING, "max8998-charger", chg);
	if (ret) {
		pr_err("%s : Failed to request pmic irq\n", __func__);
		goto err_supply_unreg_ac;
	}

	ret = enable_irq_wake(iodev->i2c->irq);
	if (ret) {
		pr_err("%s : Failed to enable pmic irq wake\n", __func__);
		goto err_irq;
	}

	ret = s3c_bat_create_attrs(chg->psy_bat.dev);
	if (ret) {
		pr_err("%s : Failed to create_attrs\n", __func__);
		goto err_irq;
	}

	chg->callbacks.set_cable = max8998_set_cable;
	chg->callbacks.set_jig = max8998_set_jig;
	chg->callbacks.set_esafe = max8998_set_esafe;
	chg->callbacks.get_vdcin = max8998_get_vdcin;
	if (chg->pdata->register_callbacks)
		chg->pdata->register_callbacks(&chg->callbacks);

	pchg = chg;	// pointer...

	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);

	return 0;

err_irq:
	free_irq(iodev->i2c->irq, NULL);
err_supply_unreg_ac:
	power_supply_unregister(&chg->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(&chg->psy_usb);
err_supply_unreg_bat:
	power_supply_unregister(&chg->psy_bat);
err_wqueue:
	destroy_workqueue(chg->monitor_wqueue);
	cancel_work_sync(&chg->bat_work);
err_wake_lock:
	wake_lock_destroy(&chg->work_wake_lock);
	wake_lock_destroy(&chg->vbus_wake_lock);
	wake_lock_destroy(&chg->lowbat_wake_lock);
err_kfree:
	mutex_destroy(&chg->mutex);
err_bat_table:
	kfree(chg);
	return ret;
}

static int __devexit max8998_charger_remove(struct platform_device *pdev)
{
	struct chg_data *chg = platform_get_drvdata(pdev);

	free_irq(chg->iodev->i2c->irq, NULL);
	flush_workqueue(chg->monitor_wqueue);
	destroy_workqueue(chg->monitor_wqueue);
	power_supply_unregister(&chg->psy_bat);
	power_supply_unregister(&chg->psy_usb);
	power_supply_unregister(&chg->psy_ac);

	wake_lock_destroy(&chg->work_wake_lock);
	wake_lock_destroy(&chg->vbus_wake_lock);
	wake_lock_destroy(&chg->lowbat_wake_lock);
	mutex_destroy(&chg->mutex);
	kfree(chg);

	return 0;
}

static int max8998_charger_suspend(struct device *dev)
{
	struct chg_data *chg = dev_get_drvdata(dev);
	max8998_lowbat_config(chg, 1);
	del_timer_sync(&chg->bat_work_timer);
	return 0;
}

static void max8998_charger_resume(struct device *dev)
{
	struct chg_data *chg = dev_get_drvdata(dev);
	max8998_lowbat_config(chg, 0);
	wake_lock(&chg->work_wake_lock);
	queue_work(chg->monitor_wqueue, &chg->bat_work);
}

static const struct dev_pm_ops max8998_charger_pm_ops = {
	.prepare        = max8998_charger_suspend,
	.complete       = max8998_charger_resume,
};

static struct platform_driver max8998_charger_driver = {
	.driver = {
		.name = "max8998-charger",
		.owner = THIS_MODULE,
		.pm = &max8998_charger_pm_ops,
	},
	.probe = max8998_charger_probe,
	.remove = __devexit_p(max8998_charger_remove),
};

static int __init max8998_charger_init(void)
{
	return platform_driver_register(&max8998_charger_driver);
}

static void __exit max8998_charger_exit(void)
{
	platform_driver_register(&max8998_charger_driver);
}

late_initcall(max8998_charger_init);
module_exit(max8998_charger_exit);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("S3C6410 battery driver");
MODULE_LICENSE("GPL");

