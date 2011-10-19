/* linux/arch/arm/mach-s5pv210/mach-forte.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max8998.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/i2c/qt602240_ts.h>
#include <linux/clk.h>
#include <linux/usb/ch9.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/skbuff.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/gpio-forte.h>
#include <mach/gpio-forte-settings.h>
#include <mach/adc.h>
#include <mach/param.h>
#include <mach/system.h>
#include <mach/sec_switch.h>

#include <linux/usb/gadget.h>
#include <linux/fsa9480.h>
#include <linux/vibrator-pwm.h>
#include <linux/pn544.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/wlan_plat.h>
#include <linux/mfd/wm8994/wm8994_pdata.h>

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#include <plat/media.h>
#include <mach/media.h>
#endif

#ifdef CONFIG_S5PV210_POWER_DOMAIN
#include <mach/power-domain.h>
#endif

#include <media/common_camera_platform.h>
#include <plat/regs-serial.h>
#include <plat/s5pv210.h>
#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/fb.h>
#include <plat/mfc.h>
#include <plat/iic.h>
#include <plat/pm.h>

#include <plat/sdhci.h>
#include <plat/fimc.h>
#include <plat/jpeg.h>
#include <plat/clock.h>
#include <plat/regs-otg.h>
#include <linux/gp2a.h>

#include <../../../drivers/video/samsung/s3cfb.h>

#ifdef CONFIG_KERNEL_DEBUG_SEC
#include <linux/kernel_sec_common.h>
#endif
#include <mach/forte/sec_jack.h>
#include <linux/input/mxt224.h>
#include <linux/max17040_battery.h>
#include <linux/mfd/max8998.h>
#include <linux/switch.h>

#include "aries.h"

#define RECOVERY_SYSFS

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

struct device *gps_dev = NULL;
EXPORT_SYMBOL(gps_dev);

unsigned int HWREV =0;
EXPORT_SYMBOL(HWREV);

void (*sec_set_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_set_param_value);

void (*sec_get_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_get_param_value);

//recovery interface, Thomas Ryu
#ifdef RECOVERY_SYSFS
static int __init recovery_class_init(void);
#endif

#define KERNEL_REBOOT_MASK      0xFFFFFFFF
#define REBOOT_MODE_FAST_BOOT		7

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)
#define IRQ_TOUCH_INT (IRQ_EINT_GROUP18_BASE+5) /* J0_5 */

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static int forte_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	int mode = REBOOT_MODE_NONE;
	unsigned int temp;
   
	if ((code == SYS_RESTART) && _cmd) {
		if (!strcmp((char *)_cmd, "arm11_fota"))
			mode = REBOOT_MODE_ARM11_FOTA;
		else if (!strcmp((char *)_cmd, "arm9_fota"))
			mode = REBOOT_MODE_ARM9_FOTA;
		else if (!strcmp((char *)_cmd, "recovery"))
		{
			mode = REBOOT_MODE_RECOVERY;
#ifdef CONFIG_KERNEL_DEBUG_SEC // Added for  
			//etinum.factory.reboot disable uart msg in bootloader for
			// factory reset 2nd ack
			kernel_sec_set_upload_cause(BLK_UART_MSG_FOR_FACTRST_2ND_ACK);
#endif	
		}
		else if (!strcmp((char *)_cmd, "bootloader"))
			mode = REBOOT_MODE_FAST_BOOT;
		else if (!strcmp((char *)_cmd, "download"))
			mode = REBOOT_MODE_DOWNLOAD;
#ifdef CONFIG_KERNEL_DEBUG_SEC 
		//etinum.factory.reboot disable uart msg in bootloader for
		// factory reset 2nd ack
		// Added for F A
		else if (!strcmp((char *)_cmd, "factory_reboot")) { 
			mode = REBOOT_MODE_NONE;
			kernel_sec_set_upload_cause(BLK_UART_MSG_FOR_FACTRST_2ND_ACK);
		}
#endif
		else
			mode = REBOOT_MODE_NONE;
	}
	if (code != SYS_POWER_OFF) {
		if (sec_set_param_value) {
			sec_set_param_value(__REBOOT_MODE, &mode);
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block forte_reboot_notifier = {
	.notifier_call = forte_notifier_call,
};

static ssize_t hwrev_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "%x\n", HWREV);
}

static DEVICE_ATTR(hwrev, S_IRUGO, hwrev_show, NULL);

static void gps_gpio_init(void)
{
	struct device *gps_dev;

	gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("Failed to create device(gps)!\n");
		goto err;
	}
	
        if (device_create_file(gps_dev, &dev_attr_hwrev) < 0)
                pr_err("Failed to create device file(%s)!\n",
                       dev_attr_hwrev.attr.name);

	gpio_request(GPIO_GPS_nRST, "GPS_nRST");	/* XMMC3CLK */
	s3c_gpio_setpull(GPIO_GPS_nRST, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_nRST, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_nRST, 1);

	gpio_request(GPIO_GPS_PWR_EN, "GPS_PWR_EN");	/* XMMC3CLK */
	s3c_gpio_setpull(GPIO_GPS_PWR_EN, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_GPS_PWR_EN, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_GPS_PWR_EN, 0);

	s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
	gpio_export(GPIO_GPS_nRST, 1);
	gpio_export(GPIO_GPS_PWR_EN, 1);

	gpio_export_link(gps_dev, "GPS_nRST", GPIO_GPS_nRST);
	gpio_export_link(gps_dev, "GPS_PWR_EN", GPIO_GPS_PWR_EN);

 err:
	return;
}

static void uart_switch_init(void)
{
	int ret;
	struct device *uartswitch_dev;

	uartswitch_dev = device_create(sec_class, NULL, 0, NULL, "uart_switch");
	if (IS_ERR(uartswitch_dev)) {
		pr_err("Failed to create device(uart_switch)!\n");
		return;
	}

	ret = gpio_request(GPIO_UART_SEL, "UART_SEL");
	if (ret < 0) {
		pr_err("Failed to request GPIO_UART_SEL!\n");
		return;
	}
	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_UART_SEL, S3C_GPIO_OUTPUT);
	gpio_direction_output(GPIO_UART_SEL, 1);

	gpio_export(GPIO_UART_SEL, 1);

	gpio_export_link(uartswitch_dev, "UART_SEL", GPIO_UART_SEL);
}

static void forte_switch_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class))
		pr_err("Failed to create class(sec)!\n");

	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");
};

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define S5PV210_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define S5PV210_ULCON_DEFAULT	S3C2410_LCON_CS8

#define S5PV210_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

static struct s3c2410_uartcfg forte_uartcfgs[] __initdata = {
	{
		.hwport		= 0,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
		.wake_peer	= aries_bt_uart_wake_peer,
	},
	{
		.hwport		= 1,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
#ifndef CONFIG_FIQ_DEBUGGER
	{
		.hwport		= 2,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
#endif
	{
		.hwport		= 3,
		.flags		= 0,
		.ucon		= S5PV210_UCON_DEFAULT,
		.ulcon		= S5PV210_ULCON_DEFAULT,
		.ufcon		= S5PV210_UFCON_DEFAULT,
	},
};

static struct s3cfb_lcd s6d16a0x = {
	.width = 320,
	.height = 480,  
	.p_width = 52,
	.p_height = 86,
	.bpp = 32,
	.freq = 95,

	.timing = {
		.h_fp = 16,
		.h_bp = 24,
		.h_sw = 16,
		.v_fp = 2,
		.v_fpe = 1,
		.v_bp = 8,
		.v_bpe = 1,
		.v_sw = 2,
	},
	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};

#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (5600 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (9900 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (5600 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (13312 * SZ_1K) // 13MB	(32768 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (21504 * SZ_1K) // 21MB	(32768 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD (3072 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (320 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_PMEM (5550 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_GPU1 (3300 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_ADSP (1500 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXTSTREAM  (3000 * SZ_1K)
static struct s5p_media_device forte_media_devs[] = {
		[0] = {
		.id = S5P_MDEV_FIMD,
		.name = "fimd",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD,
		.paddr = 0x4FC00000,
	},
	[1] = {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0,
		.paddr = 0,
	},
	[2] = {
		.id = S5P_MDEV_MFC,
		.name = "mfc",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1,
		.paddr = 0,
	},
	[3] = {
		.id = S5P_MDEV_FIMC0,
		.name = "fimc0",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0,
		.paddr = 0,
	},
	[4] = {
		.id = S5P_MDEV_FIMC1,
		.name = "fimc1",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1,
		.paddr = 0,
	},
	[5] = {
		.id = S5P_MDEV_FIMC2,
		.name = "fimc2",
		.bank = 1,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2,
		.paddr = 0,
	},
	[6] = {
		.id = S5P_MDEV_JPEG,
		.name = "jpeg",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG,
		.paddr = 0,
	},
	[7] = {
		.id = S5P_MDEV_PMEM,
		.name = "pmem",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_PMEM,
		.paddr = 0,
	},
	[8] = {
		.id = S5P_MDEV_PMEM_GPU1,
		.name = "pmem_gpu1",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_GPU1,
		.paddr = 0,
	},
	[9] = {
		.id = S5P_MDEV_PMEM_ADSP,
		.name = "pmem_adsp",
		.bank = 0,
		.memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_ADSP,
		.paddr = 0,
	},
        [10] = {
                .id = S5P_MDEV_TEXSTREAM,
                .name = "s3c_bc",
                .bank = 1,
                .memsize = S5PV210_VIDEO_SAMSUNG_MEMSIZE_TEXTSTREAM,
                .paddr = 0,
        },
};

static struct regulator_consumer_supply ldo3_consumer[] = {
	{	.supply	= "usb_io", },

};

static struct regulator_consumer_supply ldo4_consumer[] = {
	{	.supply	= "VADC", },
};

static struct regulator_consumer_supply ldo5_consumer[] = {
	{	.supply	= "vtf", },
};



static struct regulator_consumer_supply ldo7_consumer[] = {
        {	.supply = "vlcd", },
};

static struct regulator_consumer_supply ldo8_consumer[] = {
	{	.supply	= "usb_core", },
};

static struct regulator_consumer_supply ldo11_consumer[] = {
	{	.supply	= "cam_main_af", },
};

static struct regulator_consumer_supply ldo12_consumer[] = {
	{	.supply	= "vibrator", },
};

static struct regulator_consumer_supply ldo13_consumer[] = {
	{	.supply	= "cam_main_vdd_a", },
};

static struct regulator_consumer_supply ldo14_consumer[] = {
	{	.supply	= "cam_main_vdd_io", },
};

static struct regulator_consumer_supply ldo15_consumer[] = {
	{	.supply	= "cam_main_vdd_d", },
};

static struct regulator_consumer_supply ldo16_consumer[] = {
	{	.supply	= "vga_avdd", },
};

static struct regulator_consumer_supply ldo17_consumer[] = {
	{       .supply = "vcc_lcd", },
};

static struct regulator_consumer_supply buck1_consumer[] = {
	{	.supply	= "vddarm", },
};

static struct regulator_consumer_supply buck2_consumer[] = {
	{	.supply	= "vddint", },
};

static struct regulator_consumer_supply buck4_consumer[] = {
	{	.supply	= "cam_isp_core", },
};

static struct regulator_consumer_supply esafeout2_consumer[] = {
        {       .supply = "esafeout2", },
};

static struct regulator_init_data forte_ldo2_data = {
	.constraints	= {
		.name		= "VALIVE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled = 1,
		},
	},
};

static struct regulator_init_data forte_ldo3_data = {
	.constraints	= {
		.name           = "VUSB_1.1V",
		.min_uV		= 1100000,
		.max_uV		= 1100000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo3_consumer),
	.consumer_supplies	= ldo3_consumer,
};

static struct regulator_init_data forte_ldo4_data = {
	.constraints	= {
		.name		= "VADC_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
                .always_on      = 1,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo4_consumer),
	.consumer_supplies	= ldo4_consumer,
};

static struct regulator_init_data forte_ldo5_data = {
	.constraints	= {
		.name		= "VTF_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo5_consumer),
	.consumer_supplies	= ldo5_consumer,
};

static struct regulator_init_data forte_ldo7_data = {
	.constraints	= {
		.name		= "VLCD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
//                .always_on      = 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumer),
	.consumer_supplies	= ldo7_consumer,
};

static struct regulator_init_data forte_ldo8_data = {
	.constraints	= {
		.name		= "VUSB_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo8_consumer),
	.consumer_supplies	= ldo8_consumer,
};

static struct regulator_init_data forte_ldo9_data = {
	.constraints	= {
		.name		= "VCC_2.8V_PDA",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
	},
};

static struct regulator_init_data forte_ldo11_data = {
	.constraints	= {
		.name		= "CAM_MAIN_AF_3.0V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo11_consumer),
	.consumer_supplies	= ldo11_consumer,
};

static struct regulator_init_data forte_ldo12_data = {
	.constraints	= {
		.name		= "vibrator_supply_3.3V",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo12_consumer),
	.consumer_supplies	= ldo12_consumer,
};

static struct regulator_init_data forte_ldo13_data = {
	.constraints	= {
		.name		= "CAM_MAIN_VDDA_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo13_consumer),
	.consumer_supplies	= ldo13_consumer,
};

static struct regulator_init_data forte_ldo14_data = {
	.constraints	= {
		.name		= "CAM_MAIN_VDDIO_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo14_consumer),
	.consumer_supplies	= ldo14_consumer,
};

static struct regulator_init_data forte_ldo15_data = {
	.constraints	= {
		.name		= "CAM_MAIN_VDDD_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo15_consumer),
	.consumer_supplies	= ldo15_consumer,
};

static struct regulator_init_data forte_ldo16_data = {
	.constraints	= {
		.name		= "VGA_AVDD_2.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo16_consumer),
	.consumer_supplies	= ldo16_consumer,
};

static struct regulator_init_data forte_ldo17_data = {
	.constraints	= {
		.name		= "VCC_3.0V_LCD",
		.min_uV		= 1600000,
		.max_uV		= 3600000,
//		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
                .always_on      = 1,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo17_consumer),
	.consumer_supplies	= ldo17_consumer,
};

static struct regulator_init_data forte_buck1_data = {
	.constraints	= {
		.name		= "VDD_ARM",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1250000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck1_consumer),
	.consumer_supplies	= buck1_consumer,
};

static struct regulator_init_data forte_buck2_data = {
	.constraints	= {
		.name		= "VDD_INT",
		.min_uV		= 750000,
		.max_uV		= 1500000,
//		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV	= 1100000,
			.mode	= REGULATOR_MODE_NORMAL,
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck2_consumer),
	.consumer_supplies	= buck2_consumer,
};

static struct regulator_init_data forte_buck3_data = {
	.constraints	= {
		.name		= "VCC_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
	},
};

static struct regulator_init_data forte_buck4_data = {
	.constraints	= {
		.name		= "CAM_ISP_CORE_1.2V",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(buck4_consumer),
	.consumer_supplies	= buck4_consumer,
};

static struct regulator_init_data forte_esafeout2_data = {
        .constraints    = {
                .name           = "ESAFEOUT2",
                .valid_ops_mask = REGULATOR_CHANGE_STATUS,
        },

        .num_consumer_supplies  = ARRAY_SIZE(esafeout2_consumer),
        .consumer_supplies      = esafeout2_consumer,
};

static struct max8998_regulator_data forte_regulators[] = {
	{ MAX8998_LDO2,  &forte_ldo2_data },
	{ MAX8998_LDO3,  &forte_ldo3_data },
	{ MAX8998_LDO4,  &forte_ldo4_data },  //
	{ MAX8998_LDO5,  &forte_ldo5_data },  //
	{ MAX8998_LDO7,  &forte_ldo7_data },  //
	{ MAX8998_LDO8,  &forte_ldo8_data },
	{ MAX8998_LDO9,  &forte_ldo9_data },
	{ MAX8998_LDO11, &forte_ldo11_data },
	{ MAX8998_LDO12, &forte_ldo12_data },
	{ MAX8998_LDO13, &forte_ldo13_data },
	{ MAX8998_LDO14, &forte_ldo14_data },
	{ MAX8998_LDO15, &forte_ldo15_data },
	{ MAX8998_LDO16, &forte_ldo16_data },
	{ MAX8998_LDO17, &forte_ldo17_data }, //
	{ MAX8998_BUCK1, &forte_buck1_data }, //
	{ MAX8998_BUCK2, &forte_buck2_data }, // 
	{ MAX8998_BUCK3, &forte_buck3_data },
	{ MAX8998_BUCK4, &forte_buck4_data },
	{ MAX8998_ESAFEOUT2, &forte_esafeout2_data },
};

static struct max8998_adc_table_data temper_table[] =  {
	{ 151,  -70 },
        { 153,  -60 },
        { 154,  -50 },
        { 156,  -40 },
        { 157,  -30 },
        { 159,  -20 },
        { 160,  -10 },
        { 162,  0 },
        { 163,  10 },
        { 165,  20 },
        { 166,  30 },
        { 168,  40 },
        { 169,  50 },
        { 171,  60 },
        { 172,  70 },
        { 174,  80 },
        { 175,  90 },
        { 177,  100 },
        { 178,  110 },
        { 180,  120 },
        { 181,  130 },
        { 183,  140 },
        { 184,  150 },
        { 186,  160 },
        { 187,  170 },
        { 188,  180 },
        { 190,  190 },
        { 191,  200 },
        { 193,  210 },
 	{ 194,  220 },
        { 196,  230 },
        { 197,  240 },
        { 199,  250 },
        { 200,  260 },
        { 202,  270 },
        { 203,  280 },
        { 205,  290 },
        { 206,  300 },
        { 207,  310 },
        { 209,  320 },
        { 210,  330 },
        { 212,  340 },
        { 214,  350 },
        { 215,  360 },
        { 216,  370 },
        { 218,  380 },
        { 219,  390 },
        { 221,  400 },
        { 222,  410 },
        { 224,  420 },
        { 225,  430 },
        { 227,  440 },
        { 229,  450 },
        { 230,  460 },
        { 232,  470 },
        { 233,  480 },
        { 235,  490 },
        { 236,  500 },
        { 238,  510 },
        { 239,  520 },
        { 241,  530 },
	{ 242,  540 },
        { 244,  550 },
        { 245,  560 },
        { 247,  570 },
        { 248,  580 },
        { 250,  590 },
        { 251,  600 },
        { 253,  610 },
        { 254,  620 },
        { 256,  630 },
        { 257,  640 },
        { 259,  650 },
        { 260,  660 },
        { 262,  670 },
};
struct s5p_batt_block_temp forte_batt_block_temp = {
	.temp_high_block	= 224,
	.temp_high_recover	= 191,
	.temp_low_recover	= 115,
	.temp_low_block	= 108,
	.temp_high_block_lpm	= 229,
	.temp_high_recover_lpm	= 195,
	.temp_low_recover_lpm	= 116,
	.temp_low_block_lpm	= 107,
};
struct max8998_charger_callbacks *callbacks;
static enum cable_type_t set_cable_status;
static bool jig_attached = 0;

static void max8998_charger_register_callbacks(
		struct max8998_charger_callbacks *ptr)
{
	callbacks = ptr;
	/* if there was a cable status change before the charger was
	ready, send this now */
	if ((set_cable_status != 0) && callbacks && callbacks->set_cable)
		callbacks->set_cable(callbacks, set_cable_status);

	if ((jig_attached != 0) && callbacks && callbacks->set_jig)
		callbacks->set_jig(callbacks, jig_attached);
}

static struct max8998_charger_data forte_charger = {
	.register_callbacks = &max8998_charger_register_callbacks,
	.adc_table		= temper_table,
	.adc_array_size		= ARRAY_SIZE(temper_table),
	.termination_curr_adc	= 0,
};

static struct adc_channel_type s3c_adc_channel = {
        .s3c_adc_voltage = 1,
        .s3c_adc_chg_current = 2,
        .s3c_adc_temperature = 6,
};

static struct max8998_platform_data max8998_pdata = {
	.num_regulators = ARRAY_SIZE(forte_regulators),
	.regulators     = forte_regulators,
	.charger        = &forte_charger,
	.buck1_set1             = GPIO_BUCK_1_EN_A,
	.buck1_set2             = GPIO_BUCK_1_EN_B,
	.buck2_set3             = GPIO_BUCK_2_EN,
	.buck1_voltage_set      = { 1275000, 1200000, 1050000, 950000 },
        .buck2_voltage_set      = { 1100000, 1000000 },
	.s3c_adc_channel	= &s3c_adc_channel,
        .s5pc110_batt_block_temp    = &forte_batt_block_temp,
};



struct platform_device sec_device_dpram = {
	.name	= "dpram-device",
	.id	= -1,
};

static void s6d16a0x_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
	}

	/* mDNIe SEL: why we shall write 0x2 ? */
//#ifdef CONFIG_FB_S3C_MDNIE
	writel(0x2, S5P_MDNIE_SEL);
//#endif

	/* DISPLAY_CS */
	s3c_gpio_cfgpin(GPIO_DISPLAY_CS, S3C_GPIO_SFN(1));
	/* DISPLAY_CLK */
	s3c_gpio_cfgpin(GPIO_DISPLAY_CLK, S3C_GPIO_SFN(1));
	/* DISPLAY_SO */
//	s3c_gpio_cfgpin(GPIO_DUMMY, S3C_GPIO_SFN(1));
	/* DISPLAY_SI */
	s3c_gpio_cfgpin(GPIO_DISPLAY_SI, S3C_GPIO_SFN(1));

	/* DISPLAY_CS */
	s3c_gpio_setpull(GPIO_DISPLAY_CS, S3C_GPIO_PULL_NONE);
	/* DISPLAY_CLK */
	s3c_gpio_setpull(GPIO_DISPLAY_CLK, S3C_GPIO_PULL_NONE);
	/* DISPLAY_SO */
//	s3c_gpio_setpull(GPIO_DUMMY, S3C_GPIO_PULL_NONE);
	/* DISPLAY_SI */
	s3c_gpio_setpull(GPIO_DISPLAY_SI, S3C_GPIO_PULL_NONE);
}

void lcd_cfg_gpio_early_suspend(void)
{

	int i;
        printk("[%s]\n", __func__);

        /* DISPLAY_HSYNC */
        s3c_gpio_cfgpin(GPIO_DISPLAY_HSYNC, S3C_GPIO_OUTPUT);
        s3c_gpio_setpull(GPIO_DISPLAY_HSYNC, S3C_GPIO_PULL_NONE);
        s3c_gpio_setpin(GPIO_DISPLAY_HSYNC, 0);

        /* DISPLAY_VSYNC */
        s3c_gpio_cfgpin(GPIO_DISPLAY_VSYNC, S3C_GPIO_OUTPUT);
        s3c_gpio_setpull(GPIO_DISPLAY_VSYNC, S3C_GPIO_PULL_NONE);
        s3c_gpio_setpin(GPIO_DISPLAY_VSYNC, 0);

        /* DISPLAY_DE */
        s3c_gpio_cfgpin(GPIO_DISPLAY_DE, S3C_GPIO_OUTPUT);
        s3c_gpio_setpull(GPIO_DISPLAY_DE, S3C_GPIO_PULL_NONE);
        s3c_gpio_setpin(GPIO_DISPLAY_DE, 0);

        /* DISPLAY_PCLK */
        s3c_gpio_cfgpin(GPIO_DISPLAY_PCLK, S3C_GPIO_OUTPUT);
        s3c_gpio_setpull(GPIO_DISPLAY_PCLK, S3C_GPIO_PULL_NONE);
        s3c_gpio_setpin(GPIO_DISPLAY_PCLK, 0);

        /*
                WARNING:
                        This code works on situation that LCD data pin is
                        set serially by hardware
         */
        for (i = 0; i < 24; i++)        {
                s3c_gpio_cfgpin(GPIO_LCD_D0 + i, S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(GPIO_LCD_D0 + i, S3C_GPIO_PULL_NONE);
                s3c_gpio_setpin(GPIO_LCD_D0 + i, 0);
        }


	/* drive strength to min */
	writel(0x00000000, S5P_VA_GPIO + 0x12c); /* GPF0DRV */
	writel(0x00000000, S5P_VA_GPIO + 0x14c); /* GPF1DRV */
	writel(0x00000000, S5P_VA_GPIO + 0x16c); /* GPF2DRV */
	writel(0x00000000, S5P_VA_GPIO + 0x18c); /* GPF3DRV */

	/* DISPLAY_CS */
	s3c_gpio_cfgpin(GPIO_DISPLAY_CS, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_DISPLAY_CS, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_DISPLAY_CS, 0);

	/* DISPLAY_CLK */
	s3c_gpio_cfgpin(GPIO_DISPLAY_CLK, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_DISPLAY_CLK, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_DISPLAY_CLK, 0);

	/* DISPLAY_SI */
	s3c_gpio_cfgpin(GPIO_DISPLAY_SI, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_DISPLAY_SI, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_DISPLAY_SI, 0);
 
}
EXPORT_SYMBOL(lcd_cfg_gpio_early_suspend);

void lcd_cfg_gpio_late_resume(void)
{
 printk("[%s]\n", __func__);

}
EXPORT_SYMBOL(lcd_cfg_gpio_late_resume);

static int s6d16a0x_reset_lcd(struct platform_device *pdev)
{
       int err;
       printk(KERN_ERR "s6d16a0x_reset_lcd");

       s3c_gpio_cfgpin(GPIO_MLCD_RST, S3C_GPIO_SFN(1));

        gpio_set_value(GPIO_MLCD_RST, 0);
       msleep(10);
        gpio_set_value(GPIO_MLCD_RST, 1);
        msleep(10);
	return 0;
}

static int s6d16a0x_backlight_on(struct platform_device *pdev)
{
       printk("Back Light On\n");
	return 0;
}

static struct s3c_platform_fb s6d16a0x_data __initdata = {
	.hw_ver		= 0x62,
	.clk_name	= "sclk_fimd",
	.nr_wins	= 5,
	.default_win	= CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap		= FB_SWAP_HWORD | FB_SWAP_WORD,

	.lcd = &s6d16a0x,   
	.cfg_gpio	= s6d16a0x_cfg_gpio,
	.backlight_on	= s6d16a0x_backlight_on,
	.reset_lcd	= s6d16a0x_reset_lcd,
};


static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias	= "s6d16a0x",
		.platform_data	= NULL,   //&aries_panel_data,
		.max_speed_hz	= 1200000,
		.bus_num	= 3,   //LCD_BUS_NUM,
		.chip_select	= 0,
		.mode		= SPI_MODE_3,
		.controller_data = (void *)GPIO_DISPLAY_CS,
	},
};

static struct spi_gpio_platform_data s6d16a0x_spi_gpio_data = {
	.sck	= GPIO_DISPLAY_CLK,
	.mosi	= GPIO_DISPLAY_SI,
	.miso	= -1,
	.num_chipselect = 1,  //2,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= 3 ,  //LCD_BUS_NUM,
	.dev	= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &s6d16a0x_spi_gpio_data,
	},
};

struct vibrator_platform_data vibrator_data = {

       .timer_id = 1,
       .vib_enable_gpio = GPIO_VIBTONE_EN1,
};

static struct platform_device s3c_device_vibrator = {
       .name = "cm4040_cs",
       .id   = -1,
        .dev  = {
                 .platform_data = &vibrator_data,
               },
};

static  struct  i2c_gpio_platform_data  i2c4_platdata = {
	.sda_pin		= GPIO_AP_SDA_18V,
	.scl_pin		= GPIO_AP_SCL_18V,
	.udelay			= 2,    /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c4 = {
	.name			= "i2c-gpio",
	.id			= 4,
	.dev.platform_data	= &i2c4_platdata,
};

static  struct  i2c_gpio_platform_data  i2c5_platdata = {
	.sda_pin		= GPIO_AP_SDA_28V,
	.scl_pin		= GPIO_AP_SCL_28V,
	.udelay			= 2,    /* 250KHz */
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device s3c_device_i2c5 = {
	.name			= "i2c-gpio",
	.id			= 5,
	.dev.platform_data	= &i2c5_platdata,
};

static struct i2c_gpio_platform_data i2c6_platdata = {
	.sda_pin                = GPIO_AP_PMIC_SDA,
	.scl_pin                = GPIO_AP_PMIC_SCL,
	.udelay                 = 2,    /* 250KHz */
	.sda_is_open_drain      = 0,
	.scl_is_open_drain      = 0,
	.scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c6 = {
	.name			= "i2c-gpio",
	.id			= 6,
	.dev.platform_data      = &i2c6_platdata,
};
#if 0
static  struct  i2c_gpio_platform_data  i2c7_platdata = {
	.sda_pin                = GPIO_USB_SDA_28V,
	.scl_pin                = GPIO_USB_SCL_28V,
	.udelay                 = 2,    /* 250KHz */
	.sda_is_open_drain      = 0,
	.scl_is_open_drain      = 0,
	.scl_is_output_only     = 0,
};
#endif
static  struct  i2c_gpio_platform_data  i2c13_platdata = {
        .sda_pin                = GPIO_ALS_SDA_28V,
        .scl_pin                = GPIO_ALS_SCL_28V,
        .udelay                 = 2,    /* 250KHz */
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c13 = {
        .name                   = "i2c-gpio",
        .id                     = 13,
        .dev.platform_data      = &i2c13_platdata,
};
static  struct  i2c_gpio_platform_data  i2c9_platdata = {
	.sda_pin                = FUEL_SDA_18V,
	.scl_pin                = FUEL_SCL_18V,
	.udelay                 = 2,    /* 250KHz */
	.sda_is_open_drain      = 0,
	.scl_is_open_drain      = 0,
	.scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c9 = {
	.name			= "i2c-gpio",
	.id			= 9,
	.dev.platform_data	= &i2c9_platdata,
};
#if 0
static  struct  i2c_gpio_platform_data  i2c10_platdata = {
	.sda_pin                = _3_TOUCH_SDA_28V,
	.scl_pin                = _3_TOUCH_SCL_28V,
	.udelay                 = 0,    /* 250KHz */
	.sda_is_open_drain      = 0,
	.scl_is_open_drain      = 0,
	.scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c10 = {
	.name			= "i2c-gpio",
	.id			= 10,
	.dev.platform_data	= &i2c10_platdata,
};
#endif
static  struct  i2c_gpio_platform_data  i2c11_platdata = {
	.sda_pin                = GPIO_BL_SDA_28V,
	.scl_pin                = GPIO_BL_SCL_28V,
	.udelay                 = 0,  //2,    /* 250KHz */
	.sda_is_open_drain      = 0,
	.scl_is_open_drain      = 0,
	.scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c11 = {
	.name			= "i2c-gpio",
	.id			= 11,
	.dev.platform_data	= &i2c11_platdata,
};
#if 0
static void touch_keypad_gpio_init(void)
{
	int ret = 0;

	ret = gpio_request(_3_GPIO_TOUCH_EN, "TOUCH_EN");
	if (ret)
		printk(KERN_ERR "Failed to request gpio touch_en.\n");
}

static void touch_keypad_onoff(int onoff)
{
	gpio_direction_output(_3_GPIO_TOUCH_EN, onoff);

	if (onoff == TOUCHKEY_OFF)
		msleep(30);
	else
		msleep(25);
}
#endif
#if 0
static const int touch_keypad_code[] = {
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
	KEY_SEARCH
};

static struct touchkey_platform_data touchkey_data = {
	.keycode_cnt = ARRAY_SIZE(touch_keypad_code),
	.keycode = touch_keypad_code,
	.touchkey_onoff = touch_keypad_onoff,
};
#endif
#if 1
static struct platform_pwm_backlight_data smdk_backlight_data = {
        .pwm_id  = 3,
        .max_brightness = 255,
        .dft_brightness = 255,
        .pwm_period_ns  = 78770,
};

static struct platform_device smdk_backlight_device = {
        .name      = "pwm-backlight",
        .id        = -1,
        .dev        = {
                .parent = &s3c_device_timer[3].dev,
                .platform_data = &smdk_backlight_data,
        },
};
static void __init smdk_backlight_register(void)
{
        int ret = platform_device_register(&smdk_backlight_device);
        if (ret)
                printk(KERN_ERR "smdk: failed to register backlight device: %d\n", ret);
}

#endif
#if 1
static struct gpio_event_direct_entry forte_keypad_key_map[] = {
        {
                .gpio   = S5PV210_GPH2(6),
                .code   = KEY_POWER,
        },
};

static struct gpio_event_input_info forte_keypad_key_info = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = true,
	.debounce_time.tv.nsec = 5 * NSEC_PER_MSEC,
	.type = EV_KEY,
	.keymap = forte_keypad_key_map,
	.keymap_size = ARRAY_SIZE(forte_keypad_key_map)
};

static struct gpio_event_info *forte_input_info[] = {
	&forte_keypad_key_info.info,
};


static struct gpio_event_platform_data forte_input_data = {
	.names = {
		"forte-keypad",
		NULL,
	},
	.info = forte_input_info,
	.info_count = ARRAY_SIZE(forte_input_info),
};

static struct platform_device forte_input_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev = {
		.platform_data = &forte_input_data,
	},
};
#endif
#ifdef CONFIG_S5P_ADC
static struct s3c_adc_mach_info s3c_adc_platform __initdata = {
	/* s5pc110 support 12-bit resolution */
	.delay  = 10000,
	.presc  = 65,
	.resolution = 12,
};
#endif

/* in revisions before 0.9, there is a common mic bias gpio */

static DEFINE_SPINLOCK(mic_bias_lock);
static bool wm8994_mic_bias;
static bool jack_mic_bias;
static void set_shared_mic_bias(void)
{
	gpio_set_value(GPIO_MICBIAS_EN, wm8994_mic_bias || jack_mic_bias);
	 /* high : earjack, low: TV_OUT */
	gpio_set_value(GPIO_EARPATH_SEL, wm8994_mic_bias || jack_mic_bias);
}

static void wm8994_set_mic_bias(bool on)
{
	if (system_rev < 0x09) {
		unsigned long flags;
		spin_lock_irqsave(&mic_bias_lock, flags);
		wm8994_mic_bias = on;
		set_shared_mic_bias();
		spin_unlock_irqrestore(&mic_bias_lock, flags);
	} else
		gpio_set_value(GPIO_MICBIAS_EN, on);
}

static void sec_jack_set_micbias_state(bool on)
{
#if 0
	if (system_rev < 0x09) {
		unsigned long flags;
		spin_lock_irqsave(&mic_bias_lock, flags);
		jack_mic_bias = on;
		set_shared_mic_bias();
		spin_unlock_irqrestore(&mic_bias_lock, flags);
	} else
		gpio_set_value(GPIO_EAR_MICBIAS_EN, on);
#endif
}

static struct wm8994_platform_data wm8994_pdata = {
	.ldo = GPIO_CODEC_LDO_EN,
	.ear_sel = GPIO_EARPATH_SEL,
	.set_mic_bias = wm8994_set_mic_bias,
};



/*
 * Guide for Camera Configuration
*/
static struct regulator *cam_main_vdd_a;
static struct regulator *cam_main_vdd_d;
static struct regulator *cam_main_vdd_io;
static struct regulator *cam_main_af;

// cmk 2010.10.15 setup
#ifdef CONFIG_VIDEO_S5K5CCGX
/****************************************************************************
 * s5k5ccgx_ldo_en()
 *
 * @param     en             LDO enable
 * @return    void
 * @remark    Function     Set_MAX8998_PM_OUTPUT_Voltage, Set_MAX8998_PM_REG, 
 *
 * Oct 15, 2010  initial revision
 *****************************************************************************/
static void s5k5ccgx_ldo_en(bool en)
{
	int err = 0;

	if( IS_ERR_OR_NULL(cam_main_vdd_a) ||
        IS_ERR_OR_NULL(cam_main_vdd_d) ||
        IS_ERR_OR_NULL(cam_main_vdd_io) ||
        IS_ERR_OR_NULL(cam_main_af))
    {
        pr_err("Camera regulators were not initialized\n");
		return;
	}

	if(!en)
	{
		goto CAM_POWER_OFF;
    }
    
	/* VDDA : CAM_A_2.8V (LDO13) */		
	err = regulator_enable(cam_main_vdd_a);
	if(err) 
	{
		pr_err("Failed to enable regulator cam_main_vdd_a\n");
		goto CAM_POWER_OFF;
	}
	udelay(5);

	/* VDD_REG : CAM_3M_1.2V (LDO15) */
	err = regulator_enable(cam_main_vdd_d);
	if(err) 
	{
		pr_err("Failed to enable regulator cam_main_vdd_d\n");
		goto CAM_POWER_OFF;
	}
	udelay(5);

	/* VDDIO : CAM_IO_2.8V (LDO14) */
	err = regulator_enable(cam_main_vdd_io);
	if(err) 
	{
		pr_err("Failed to enable regulator cam_main_vdd_io\n");
		goto CAM_POWER_OFF;
	}
	udelay(5);

	/* VAF : CAM_AF_2.8V or 3.0V (LDO11) */
	err = regulator_enable(cam_main_af);
	if(err) 
	{
		pr_err("Failed to enable regulator cam_main_af\n");
		goto CAM_POWER_OFF;
	}
    return;

CAM_POWER_OFF:

	/* VAF : CAM_AF_2.8V or 3.0V (LDO11) */
	err = regulator_disable(cam_main_af);
	if(err) 
	{
		pr_err("Failed to disable regulator cam_main_af\n");
	}
    udelay(10);

	/* VDDIO : CAM_IO_2.8V (LDO14) */
	err = regulator_disable(cam_main_vdd_io);
	if(err) 
	{
		pr_err("Failed to disable regulator cam_main_vdd_io\n");
	}
    udelay(5);

	/* VDD_REG : CAM_3M_1.2V (LDO15) */
	err = regulator_disable(cam_main_vdd_d);
	if(err) 
	{
		pr_err("Failed to disable regulator cam_main_vdd_d\n");
	}
    udelay(5);

	/* VDDA : CAM_A_2.8V (LDO13) */		
	err = regulator_disable(cam_main_vdd_a);
	if(err) 
	{
		pr_err("Failed to disable regulator cam_main_vdd_a\n");
	}
}

/****************************************************************************
 * s5k5ccgx_power_on_sequence()
 *
 * @param     void 
 * @return    0
 * @remark    Function
 *
 * Oct 15, 2010  initial revision
 *****************************************************************************/
static int s5k5ccgx_power_on_sequence(void)
{
	int err;
	
	printk("camera_power_on\n");
	
	/* CAM_MEGA_EN - GPJ0(6) => GPA0(6)*/
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPA0");
	if(err)
	{
		printk(KERN_ERR "failed to request GPIO for camera standby\n");
		return err;
	}

	/* CAM_MEGA_nRST - GPJ1(5) => GPE1(4) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPE1");
	if(err) 
	{
		printk(KERN_ERR "failed to request GPE1(4) for camera control\n");

		gpio_free(GPIO_CAM_MEGA_EN);
		return err;
	}

	gpio_direction_output(GPIO_CAM_MEGA_EN, 0);
	gpio_direction_output(GPIO_CAM_MEGA_nRST, 0);
	
    // MAIN CAM : standby low
    gpio_set_value(GPIO_CAM_MEGA_EN, 0);
    // MAIN CAM : reset low
	gpio_set_value(GPIO_CAM_MEGA_nRST, 0);
	msleep(1);
	
    // Turn LDOs on
	s5k5ccgx_ldo_en(true);
    udelay(100);

    // Enable MCLK
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0x02));
    udelay(10);

    // MAIN CAM : standby high
    gpio_set_value(GPIO_CAM_MEGA_EN, 1);
    udelay(50);

    // MAIN CAM : reset high
	gpio_set_value(GPIO_CAM_MEGA_nRST, 1);
	msleep(15);

	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);

	return 0;
}

/****************************************************************************
 * s5k5ccgx_power_off_sequence()
 *
 * @param     void 
 * @return    err
 * @remark    Function
 *
 * Oct 15, 2010  initial revision
 *****************************************************************************/
static int s5k5ccgx_power_off_sequence(void)
{
	int err;
	
	printk("camera_power_off\n");

	/* CAM_MEGA_EN - GPJ0(6) => GPA0(6)*/
	err = gpio_request(GPIO_CAM_MEGA_EN, "GPA0");
	if(err)
	{
		printk(KERN_ERR "failed to request GPIO for camera standby\n");
		return err;
	}

	/* CAM_MEGA_nRST - GPJ1(5) => GPE1(4) */
	err = gpio_request(GPIO_CAM_MEGA_nRST, "GPE1");
	if(err) 
	{
		printk(KERN_ERR "failed to request GPE1(4) for camera control\n");

		gpio_free(GPIO_CAM_MEGA_EN);
		return err;
	}

    // MAIN CAM : reset low
	gpio_set_value(GPIO_CAM_MEGA_nRST, 0);
	udelay(100);

	// Disable MCLK
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	udelay(5);

    // MAIN CAM : standby low
	gpio_set_value(GPIO_CAM_MEGA_EN, 0); 
	udelay(5);

    // Turn LDOs off
	s5k5ccgx_ldo_en(false);
	msleep(1);

	gpio_free(GPIO_CAM_MEGA_EN);
	gpio_free(GPIO_CAM_MEGA_nRST);

	return 0;
}

static int s5k5ccgx_regulator_init(void)
{
    /* LDO11 */
	if(IS_ERR_OR_NULL(cam_main_af)) 
	{
		cam_main_af = regulator_get(NULL, "cam_main_af");
		if(IS_ERR_OR_NULL(cam_main_af)) 
		{
			pr_err("failed to get cam_main_af regulator");
			return -EINVAL;
		}
	}
	
    /* LDO13 */
	if(IS_ERR_OR_NULL(cam_main_vdd_a)) 
	{
		cam_main_vdd_a = regulator_get(NULL, "cam_main_vdd_a");
		if(IS_ERR_OR_NULL(cam_main_vdd_a)) 
		{
			pr_err("failed to get cam_main_vdd_a regulator");
			return -EINVAL;
		}
	}
	
    /* LDO14 */
	if(IS_ERR_OR_NULL(cam_main_vdd_io)) 
	{
		cam_main_vdd_io = regulator_get(NULL, "cam_main_vdd_io");
		if(IS_ERR_OR_NULL(cam_main_vdd_io)) 
		{
			pr_err("failed to get cam_main_vdd_io regulator");
			return -EINVAL;
		}
	}

    /* LDO15 */
	if(IS_ERR_OR_NULL(cam_main_vdd_d)) 
	{
		cam_main_vdd_d = regulator_get(NULL, "cam_main_vdd_d");
		if(IS_ERR_OR_NULL(cam_main_vdd_d)) 
		{
			pr_err("failed to get cam_main_vdd_d regulator");
			return -EINVAL;
		}
	}
	return 0;
}

/****************************************************************************
 * s5k5ccgx_power_en()
 *
 * @param     on/off 
 * @return    err
 * @remark    Function
 *
 * Oct 15, 2010  initial revision
 *****************************************************************************/
static int s5k5ccgx_power_en(int onoff)
{
	if(s5k5ccgx_regulator_init() < 0) 
	{
		pr_err("Failed to initialize camera regulators\n");
		return -EINVAL;
	}

	if(onoff)
	{
		s5k5ccgx_power_on_sequence();
	} 
	else 
	{
		s5k5ccgx_power_off_sequence();
	}
	return 0;
}

static struct camera_platform_data s5k5ccgx_platform_data = {
	.default_width  = 1024,
	.default_height = 768,
	.pixelformat    = V4L2_PIX_FMT_UYVY,
	.mclk_freq      = 24000000,
	.is_mipi        = 0,
};

static struct i2c_board_info s5k5ccgx_i2c_board_info = 
{	
    I2C_BOARD_INFO("S5K5CCGX", (0x78 >> 1)),
    .platform_data = &s5k5ccgx_platform_data,
};

static struct s3c_platform_camera s5k5ccgx_platform_camera = 
{
	.id             = CAMERA_PAR_A,
	.type           = CAM_TYPE_ITU,
	.fmt            = ITU_601_YCBCR422_8BIT,
	.order422       = CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum     = 0,
	.info           = &s5k5ccgx_i2c_board_info,
	.pixelformat    = V4L2_PIX_FMT_UYVY,
	.srclk_name     = "xusbxti",					
	.clk_name       = "sclk_cam", // "sclk_cam0",
	.clk_rate       = 24000000,
	.line_length    = 1536,
	.width          = 1024,
	.height         = 768,
	.window = 
	{
		.left 	    = 0,
		.top        = 0,
		.width      = 1024,
		.height     = 768,
	},

	/* Polarity */
	.inv_pclk       = 0,
	.inv_vsync      = 1,
	.inv_href       = 0,
	.inv_hsync      = 0,
	.initialized    = 0,
	.cam_power      = s5k5ccgx_power_en,
};
#endif // CONFIG_VIDEO_S5K5CCGX

/* Interface setting */
static struct s3c_platform_fimc fimc_plat_lsi = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "sclk_fimc_lclk",
	.clk_rate	= 166750000,
	.default_cam	= CAMERA_PAR_A,
	.camera		= {
#ifdef CONFIG_VIDEO_S5K5CCGX
		&s5k5ccgx_platform_camera,
#endif 
	},
	.hw_ver		= 0x43,
};

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
	.max_main_width	    = 320,
	.max_main_height	= 240,
	.max_thumb_width	= 0,
	.max_thumb_height	= 0,
};
#endif

/* I2C0 */
static struct i2c_board_info i2c_devs0[] __initdata = {
};

static struct i2c_board_info i2c_devs4[] __initdata = {
	{
		I2C_BOARD_INFO("wm8994", (0x34>>1)),
		.platform_data = &wm8994_pdata,
	},
};

/* I2C1 */
static struct i2c_board_info i2c_devs1[] __initdata = {
};

struct qt602240_platform_data qt602240_data = {
	.irq 			= IRQ_TOUCH_INT,
	.touch_int 		= GPIO_TOUCH_INT,
	.touch_en  		= GPIO_TOUCH_EN,
        .x_size 		= 1024,  // 799,
        .y_size 		= 1024,  //479,
	.orient			= 0x2,
};

/* I2C2 */
static struct i2c_board_info i2c_devs2[] __initdata = {
	{  
                 I2C_BOARD_INFO("qt602240_ts", 0x4a),
                .platform_data  = &qt602240_data,

	},
};
static void __init qt_touch_init(void)
{
        int gpio, irq;

        gpio = GPIO_TOUCH_EN;
        gpio_request(gpio, "TOUCH_EN");
        s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
        gpio_direction_output(gpio, 0);
        gpio_free(gpio);

        gpio = GPIO_TOUCH_INT;
        gpio_request(gpio, "TOUCH_INT");
        s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
        s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
        irq = gpio_to_irq(gpio);
        gpio_free(gpio);

        i2c_devs2[1].irq = irq;

}
#if 0
/* I2C2 */
static struct i2c_board_info i2c_devs10[] __initdata = {
	{
		I2C_BOARD_INFO(CYPRESS_TOUCHKEY_DEV_NAME, 0x20),
		.platform_data  = &touchkey_data,
		.irq = (IRQ_EINT_GROUP22_BASE + 1),
	},
};
static struct i2c_board_info i2c_devs8[] __initdata = {
        {
		I2C_BOARD_INFO("ags04_driver", (0xD4 >> 1)),
        },
};
#endif
static int fsa9480_init_flag = 0;
static bool mtp_off_status;
static void fsa9480_usb_cb(bool attached)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);

	if (gadget) {
		if (attached)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}
	mtp_off_status = false;
	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;
	if (callbacks && callbacks->set_cable)
		callbacks->set_cable(callbacks, set_cable_status);
}

static void fsa9480_charger_cb(bool attached)
{
	set_cable_status = attached ? CABLE_TYPE_AC : CABLE_TYPE_NONE;
	if (callbacks && callbacks->set_cable)
		callbacks->set_cable(callbacks, set_cable_status);
}

static void fsa9480_jig_cb(bool attached)
{
	jig_attached = attached;
	if (callbacks && callbacks->set_jig)
		callbacks->set_jig(callbacks, attached);
}

static struct switch_dev switch_dock = {
	.name = "dock",
};

static void fsa9480_deskdock_cb(bool attached)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);

	if (attached)
		switch_set_state(&switch_dock, 1);
	else
		switch_set_state(&switch_dock, 0);
	mtp_off_status = false;

        
	if (gadget) {
		if (attached)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}

	//set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;
	if (callbacks && callbacks->set_cable)
		callbacks->set_cable(callbacks, set_cable_status);
}

static void fsa9480_cardock_cb(bool attached)
{
	if (attached)
		switch_set_state(&switch_dock, 2);
	else
		switch_set_state(&switch_dock, 0);
}

static void fsa9480_reset_cb(void)
{
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("Failed to register dock switch. %d\n", ret);
}

static void fsa9480_set_init_flag(void)
{
	fsa9480_init_flag = 1;
}
static struct fsa9480_platform_data fsa9480_pdata = {
	.usb_cb = fsa9480_usb_cb,
	.charger_cb = fsa9480_charger_cb,
	.deskdock_cb = fsa9480_deskdock_cb,
	.cardock_cb = fsa9480_cardock_cb,
	.reset_cb = fsa9480_reset_cb,
	.jig_cb = fsa9480_jig_cb,
	.set_init_flag = fsa9480_set_init_flag,
	.spk_switch = SWITCH_PORT_AUDIO,
};
static struct i2c_board_info i2c_devs5[] __initdata = {
        {
               I2C_BOARD_INFO("yas529",0x2e),
        },

        {
                I2C_BOARD_INFO("kr3dh", 0x19),
        },
        {
                I2C_BOARD_INFO("fsa9480", (0x4A >> 1)), //0x19),
                .platform_data = &fsa9480_pdata,
               .irq = IRQ_EINT(23),
        },
};

#if 0
static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("fsa9480", 0x4A >> 1),
		.platform_data = &fsa9480_pdata,
		.irq = IRQ_EINT(23),
	},
};
#endif
static struct i2c_board_info i2c_devs6[] __initdata = {
#ifdef CONFIG_REGULATOR_MAX8998
	{
		/* The address is 0xCC used since SRAD = 0 */
		I2C_BOARD_INFO("max8998", (0xCC >> 1)),
		.platform_data	= &max8998_pdata,
		.irq		= IRQ_EINT7,
	}, {
		I2C_BOARD_INFO("rtc_max8998", (0x0D >> 1)),
	},
#endif
};
#if 0
static struct pn544_i2c_platform_data pn544_pdata = {
	.irq_gpio = NFC_IRQ,
	.ven_gpio = NFC_EN,
	.firm_gpio = NFC_FIRM,
};

static struct i2c_board_info i2c_devs14[] __initdata = {
	{
		I2C_BOARD_INFO("pn544", 0x2b),
		.irq = IRQ_EINT(12),
		.platform_data = &pn544_pdata,
	},
};
#endif
static int max17040_power_supply_register(struct device *parent,
	struct power_supply *psy)
{
	forte_charger.psy_fuelgauge = psy;
	return 0;
}

static void max17040_power_supply_unregister(struct power_supply *psy)
{
	forte_charger.psy_fuelgauge = NULL;
}

static struct max17040_platform_data max17040_pdata = {
	.power_supply_register = max17040_power_supply_register,
	.power_supply_unregister = max17040_power_supply_unregister,
	.rcomp_value = 0xB000,
};

static struct i2c_board_info i2c_devs9[] __initdata = {
	{
		I2C_BOARD_INFO("max17040", (0x6D >> 1)),
		.platform_data = &max17040_pdata,
	},
};
static struct i2c_board_info i2c_devs11[] __initdata = {
        {
                I2C_BOARD_INFO("bd60910-i2c", (0xEC >> 1)),
        },
};


static int gp2a_power(bool on)
{
        /* this controls the power supply rail to the gp2a IC */
        gpio_direction_output(GPIO_PS_ON, on);
        return 0;
}

static int gp2a_light_adc_value(void)
{
        return s3c_adc_get_adc_data(8);
}

static struct gp2a_platform_data gp2a_pdata = {
        .power = gp2a_power,
        .p_out = GPIO_PS_VOUT,
        .light_adc_value = gp2a_light_adc_value
};

static void gp2a_gpio_init(void)
{
        int ret = gpio_request(GPIO_PS_ON, "gp2a_power_supply_on");
        if (ret)
                printk(KERN_ERR "Failed to request gpio gp2a power supply.\n");

        s3c_gpio_cfgpin(GPIO_PS_VOUT, S3C_GPIO_SFN(GPIO_PS_VOUT_AF));
        s3c_gpio_setpull(GPIO_PS_VOUT, S3C_GPIO_PULL_NONE);
        set_irq_type(IRQ_EINT1, IRQ_TYPE_EDGE_BOTH);
        gp2a_pdata.p_irq = gpio_to_irq(GPIO_PS_VOUT);
        gp2a_pdata.p_out = GPIO_PS_VOUT;



}

static struct i2c_board_info i2c_devs13[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", (0x88 >> 1)),
		.platform_data = &gp2a_pdata,
	},
};
#if 0
static struct i2c_board_info i2c_devs12[] __initdata = {
	{
		I2C_BOARD_INFO("yas529", 0x2e),
	},
};
#endif
static struct resource ram_console_resource[] = {
	{
		.flags = IORESOURCE_MEM,
	}
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ram_console_resource),
	.resource = ram_console_resource,
};

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name = "pmem",
	.no_allocator = 1,
	.cached = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
	.name = "pmem_gpu1",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct android_pmem_platform_data pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.no_allocator = 1,
	.cached = 1,
	.buffered = 1,
	.start = 0,
	.size = 0,
};

static struct platform_device pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &pmem_pdata },
};

static struct platform_device pmem_gpu1_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &pmem_gpu1_pdata },
};

static struct platform_device pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &pmem_adsp_pdata },
};

static void __init android_pmem_set_platdata(void)
{
	pmem_pdata.start = (u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM, 0);
	pmem_pdata.size = (u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM, 0);

	pmem_gpu1_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_GPU1, 0);
	pmem_gpu1_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_GPU1, 0);

	pmem_adsp_pdata.start =
		(u32)s5p_get_media_memory_bank(S5P_MDEV_PMEM_ADSP, 0);
	pmem_adsp_pdata.size =
		(u32)s5p_get_media_memsize_bank(S5P_MDEV_PMEM_ADSP, 0);
}
#endif

struct platform_device sec_device_battery = {
	.name	= "sec-battery",
	.id	= -1,
};

static int sec_switch_get_cable_status(void)
{
	return mtp_off_status ? CABLE_TYPE_NONE : set_cable_status;
	//return set_cable_status;
}

static int sec_switch_get_phy_init_status(void)
{
	return fsa9480_init_flag;
}

static void sec_switch_set_vbus_status(u8 mode)
{
	if (mode == USB_VBUS_ALL_OFF)
                mtp_off_status = true;

	if (callbacks && callbacks->set_esafe)
		callbacks->set_esafe(callbacks, mode);
}

static void sec_switch_set_usb_gadget_vbus(bool en)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);

	if (gadget) {
		if (en)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}
}

static struct sec_switch_platform_data sec_switch_pdata = {
	.set_vbus_status = sec_switch_set_vbus_status,
	.set_usb_gadget_vbus = sec_switch_set_usb_gadget_vbus,
	.get_cable_status = sec_switch_get_cable_status,
	.get_phy_init_status = sec_switch_get_phy_init_status,
};

struct platform_device sec_device_switch = {
	.name	= "sec_switch",
	.id	= 1,
	.dev	= {
		.platform_data	= &sec_switch_pdata,
	}
};
static struct platform_device sec_device_rfkill = {
	.name	= "bt_rfkill",
	.id	= -1,
};

static struct platform_device sec_device_btsleep = {
	.name	= "bt_sleep",
	.id	= -1,
};

static struct sec_jack_port sec_jack_port[] = {
        {
                { // HEADSET detect info
                        .eint       =IRQ_EINT6,
                        .gpio       = GPIO_DET_35,
                        .gpio_af    = GPIO_DET_35_AF  ,
                        .low_active = 1
                },
                { // SEND/END info
                        .eint       = IRQ_EINT13,
                        .gpio       = S5PV210_GPH1(5),
                        .gpio_af    = 0xFF,
                        .low_active = 1
                },
            { // Open_SEND_END info  added for Froyo A
                        .eint           = IRQ_EINT9,
                        .gpio           = S5PV210_GPH1(1),
                        .gpio_af        = 0xFF,
                        .low_active     = 0
                }
        }
};

static struct sec_jack_platform_data sec_jack_data = {
        .port           = sec_jack_port,
        .nheadsets      = ARRAY_SIZE(sec_jack_port),
};

static struct platform_device sec_device_jack = {
        .name           = "sec_jack",
        .id             = -1,
        .dev            = {
                .platform_data  = &sec_jack_data,
        },
};

void s3c_setup_keypad_cfg_gpio(int rows, int columns)
{
        unsigned int gpio;
        unsigned int end;
    /* KEYPAD SENSE/ROW */
        end = S5PV210_GPH3(rows);

        /* Set all the necessary GPH2 pins to special-function 0 */
    for (gpio = S5PV210_GPH3(0); gpio < end; gpio++) {
                s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
                s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
    }

    printk(KERN_ERR "%s columns = %d\n", __FUNCTION__, columns) ;
    /* KEYPAD SCAN/COLUMN */
    columns -= 2 ;  /* except scan[6~7] */
    end = S5PV210_GPH2(columns);

        /* Set all the necessary GPK pins to special-function 0 */
    for (gpio = S5PV210_GPH2(0); gpio < end; gpio++) {
                s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
                s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
    }

    s3c_gpio_cfgpin(GPIO_KEYSCAN6, S3C_GPIO_OUTPUT) ;
        s3c_gpio_setpull(GPIO_KEYSCAN6, S3C_GPIO_PULL_NONE);
    s3c_gpio_cfgpin(GPIO_KEYSCAN7, S3C_GPIO_OUTPUT) ;
        s3c_gpio_setpull(GPIO_KEYSCAN7, S3C_GPIO_PULL_NONE);


}
EXPORT_SYMBOL(s3c_setup_keypad_cfg_gpio);

#define S5PV210_PS_HOLD_CONTROL_REG (S3C_VA_SYS+0xE81C)
static void forte_power_off(void)
{
	int err;
	int mode = REBOOT_MODE_NONE;
	char reset_mode = 'r';
	int phone_wait_cnt = 0;

	/* Change this API call just before power-off to take the dump. */
	/* kernel_sec_clear_upload_magic_number(); */
	err = gpio_request(GPIO_PHONE_ACTIVE, "GPIO_PHONE_ACTIVE");
	/* will never be freed */
	WARN(err, "failed to request GPIO_PHONE_ACTIVE");
	gpio_direction_input(GPIO_nPOWER);
	gpio_direction_input(GPIO_PHONE_ACTIVE);

	/* prevent phone reset when AP off */
	gpio_set_value(GPIO_PHONE_ON, 0);

	/* confirm phone off */
	while (1) {
		if (gpio_get_value(GPIO_PHONE_ACTIVE)) {
			if (phone_wait_cnt > 10) {
				printk(KERN_EMERG
				       "%s: Try to Turn Phone Off by CP_RST\n",
				       __func__);
				gpio_set_value(GPIO_CP_RST, 0);
			}
			if (phone_wait_cnt > 12) {
				printk(KERN_EMERG "%s: PHONE OFF Failed\n",
				       __func__);
				break;
			}
			phone_wait_cnt++;
			msleep(1000);
		} else {
			printk(KERN_EMERG "%s: PHONE OFF Success\n", __func__);
			break;
		}
	}

	while (1) {
		/* Check reboot charging */
		if (callbacks &&
		    callbacks->get_vdcin &&
		    callbacks->get_vdcin(callbacks)) {
			/* watchdog reset */
			pr_info("%s: charger connected, rebooting\n", __func__);
			mode = REBOOT_MODE_CHARGING;
			if (sec_set_param_value)
				sec_set_param_value(__REBOOT_MODE, &mode);
			kernel_sec_clear_upload_magic_number();
			kernel_sec_hw_reset(1);
			arch_reset('r', NULL);
			pr_crit("%s: waiting for reset!\n", __func__);
			while (1);
		}

		kernel_sec_clear_upload_magic_number();
		/* wait for power button release */
		if (gpio_get_value(GPIO_nPOWER)) {
			pr_info("%s: set PS_HOLD low\n", __func__);

			/* PS_HOLD high  PS_HOLD_CONTROL, R/W, 0xE010_E81C */
			writel(readl(S5PV210_PS_HOLD_CONTROL_REG) & 0xFFFFFEFF,
			       S5PV210_PS_HOLD_CONTROL_REG);

			pr_crit("%s: should not reach here!\n", __func__);
		}

		/* if power button is not released, wait and check TA again */
		pr_info("%s: PowerButton is not released.\n", __func__);
		mdelay(1000);
	}	
}

static void config_gpio_table(int array_size, unsigned int (*gpio_table)[4])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];

		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));

		if (gpio_table[i][2] != S3C_GPIO_SETPIN_NONE)
			gpio_set_value(gpio, gpio_table[i][2]);
                      s3c_gpio_setpull(gpio, gpio_table[i][3]);
                    s3c_gpio_set_drvstrength(gpio,gpio_table[i][4]);
//Sunish
#if 0		/* Alive part */
		else if((gpio <= S5PV210_GPH3(7)) && (gpio >= S5PV210_GPH0(0))) {
        	        s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
                	s3c_gpio_setpull(gpio, gpio_table[i][3]);

                	if (gpio_table[i][2] != S3C_GPIO_SETPIN_NONE)
                    		gpio_set_value(gpio, gpio_table[i][2]);

                	s3c_gpio_set_drvstrength(gpio, gpio_table[i][4]);
                	s3c_gpio_set_slewrate(gpio, gpio_table[i][5]);
		}
#endif
}
}
static void config_sleep_gpio_table(int array_size, unsigned int (*gpio_table)[3])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_slp_cfgpin(gpio, gpio_table[i][1]);
		s3c_gpio_slp_setpull_updown(gpio, gpio_table[i][2]);
	}
}

static void config_init_gpio(void)
{
	config_gpio_table(ARRAY_SIZE(initial_gpio_table), initial_gpio_table);
}

void config_sleep_gpio(void)
{

	config_gpio_table(ARRAY_SIZE(sleep_alive_gpio_table), sleep_alive_gpio_table);

	config_sleep_gpio_table(ARRAY_SIZE(sleep_gpio_table), sleep_gpio_table);
//    s3c_gpio_setpull(GPIO_PS_VOUT, S3C_GPIO_PULL_DOWN);
#if 0  
	if (gpio_get_value(GPIO_PS_ON)) {
		s3c_gpio_slp_setpull_updown(GPIO_ALS_SDA_28V, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_setpull_updown(GPIO_ALS_SCL_28V, S3C_GPIO_PULL_NONE);
	} else {
		s3c_gpio_setpull(GPIO_PS_VOUT, S3C_GPIO_PULL_DOWN);
	}
	printk(KERN_DEBUG "SLPGPIO : BT(%d) WLAN(%d) BT+WIFI(%d)\n",
		gpio_get_value(GPIO_BT_nRST), gpio_get_value(GPIO_WLAN_nRST), gpio_get_value(GPIO_WLAN_BT_EN));
	printk(KERN_DEBUG "SLPGPIO : CODEC_LDO_EN(%d) MICBIAS_EN(%d) EARPATH_SEL(%d)\n",
		gpio_get_value(GPIO_CODEC_LDO_EN), gpio_get_value(GPIO_MICBIAS_EN), gpio_get_value(GPIO_EARPATH_SEL));
#if !defined(CONFIG_ARIES_NTT)
	printk(KERN_DEBUG "SLPGPIO : PS_ON(%d) FM_RST(%d) UART_SEL(%d)\n",
		gpio_get_value(GPIO_PS_ON), gpio_get_value(GPIO_FM_RST), gpio_get_value(GPIO_UART_SEL));
#endif
#endif
}
EXPORT_SYMBOL(config_sleep_gpio);

static unsigned int wlan_sdio_on_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, GPIO_WLAN_SDIO_CLK_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, GPIO_WLAN_SDIO_CMD_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, GPIO_WLAN_SDIO_D0_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, GPIO_WLAN_SDIO_D1_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, GPIO_WLAN_SDIO_D2_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, GPIO_WLAN_SDIO_D3_AF, GPIO_LEVEL_NONE,
		S3C_GPIO_PULL_NONE},
};

static unsigned int wlan_sdio_off_table[][4] = {
	{GPIO_WLAN_SDIO_CLK, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_CMD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_WLAN_SDIO_D3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static int wlan_power_en(int onoff)
{
	if (onoff) {
		s3c_gpio_cfgpin(GPIO_WLAN_HOST_WAKE,
				S3C_GPIO_SFN(GPIO_WLAN_HOST_WAKE_AF));
		s3c_gpio_setpull(GPIO_WLAN_HOST_WAKE, S3C_GPIO_PULL_DOWN);
/*
		s3c_gpio_cfgpin(GPIO_WLAN_WAKE,
				S3C_GPIO_SFN(GPIO_WLAN_WAKE_AF));
		s3c_gpio_setpull(GPIO_WLAN_WAKE, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_WAKE, GPIO_LEVEL_LOW);
*/
		s3c_gpio_cfgpin(GPIO_WLAN_nRST,
				S3C_GPIO_SFN(GPIO_WLAN_nRST_AF));
		s3c_gpio_setpull(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

		s3c_gpio_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_WLAN_BT_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
		s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN,
					S3C_GPIO_PULL_NONE);

		msleep(80);
	} else {
		gpio_set_value(GPIO_WLAN_nRST, GPIO_LEVEL_LOW);
		s3c_gpio_slp_cfgpin(GPIO_WLAN_nRST, S3C_GPIO_SLP_OUT0);
		s3c_gpio_slp_setpull_updown(GPIO_WLAN_nRST, S3C_GPIO_PULL_NONE);

		if (gpio_get_value(GPIO_BT_nRST) == 0) {
			gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
			s3c_gpio_slp_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
			s3c_gpio_slp_setpull_updown(GPIO_WLAN_BT_EN,
						S3C_GPIO_PULL_NONE);
		}
	}
	return 0;
}

static int wlan_reset_en(int onoff)
{
	gpio_set_value(GPIO_WLAN_nRST,
			onoff ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
	return 0;
}

static int wlan_carddetect_en(int onoff)
{
	u32 i;
	u32 sdio;

	if (onoff) {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_on_table); i++) {
			sdio = wlan_sdio_on_table[i][0];
			s3c_gpio_cfgpin(sdio,
					S3C_GPIO_SFN(wlan_sdio_on_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_on_table[i][3]);
			if (wlan_sdio_on_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_on_table[i][2]);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(wlan_sdio_off_table); i++) {
			sdio = wlan_sdio_off_table[i][0];
			s3c_gpio_cfgpin(sdio,
				S3C_GPIO_SFN(wlan_sdio_off_table[i][1]));
			s3c_gpio_setpull(sdio, wlan_sdio_off_table[i][3]);
			if (wlan_sdio_off_table[i][2] != GPIO_LEVEL_NONE)
				gpio_set_value(sdio, wlan_sdio_off_table[i][2]);
		}
	}
	udelay(5);

	sdhci_s3c_force_presence_change(&s3c_device_hsmmc1);
	return 0;
}

static struct resource wifi_resources[] = {
	[0] = {
		.name	= "bcm4329_wlan_irq",
		.start	= IRQ_EINT(12),
		.end	= IRQ_EINT(12),
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct wifi_mem_prealloc wifi_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

static void *aries_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wifi_mem_array[section].size < size)
		return NULL;

	return wifi_mem_array[section].mem_ptr;
}

#define DHD_SKB_HDRSIZE 		336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)
int __init forte_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}
	
	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}
	
	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wifi_mem_array[i].mem_ptr =
				kmalloc(wifi_mem_array[i].size, GFP_KERNEL);

		if (!wifi_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wifi_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
static struct wifi_platform_data wifi_pdata = {
	.set_power		= wlan_power_en,
	.set_reset		= wlan_reset_en,
	.set_carddetect		= wlan_carddetect_en,
	.mem_prealloc		= aries_mem_prealloc,
};

static struct platform_device sec_device_wifi = {
	.name			= "bcm4329_wlan",
	.id			= 1,
	.num_resources		= ARRAY_SIZE(wifi_resources),
	.resource		= wifi_resources,
	.dev			= {
		.platform_data = &wifi_pdata,
	},
};

static struct platform_device watchdog_device = {
	.name = "watchdog",
	.id = -1,
};

static struct platform_device *forte_devices[] __initdata = {
	&watchdog_device,
#ifdef CONFIG_FIQ_DEBUGGER
	&s5pv210_device_fiqdbg_uart2,
#endif
	&s5pc110_device_onenand,
#ifdef CONFIG_RTC_DRV_S3C
	&s5p_device_rtc,
#endif
	&s3c_device_keypad,
	//&atlas_input_device,
	&forte_input_device,
	&s5pv210_device_iis0,
	&s3c_device_wdt,
#ifdef CONFIG_FB_S3C
	&s3c_device_fb,
#endif

#ifdef CONFIG_VIDEO_MFC50
	&s3c_device_mfc,
#endif
#ifdef	CONFIG_S5P_ADC
	&s3c_device_adc,
#endif
#ifdef CONFIG_VIDEO_FIMC
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
#endif
#ifdef CONFIG_VIDEO_JPEG_V2
	&s3c_device_jpeg,
#endif
	&s3c_device_g3d,
	&s3c_device_lcd,
	&s3c_device_spi_gpio,
	&sec_device_jack,
	&s3c_device_vibrator,

	&s3c_device_i2c0,
	&s3c_device_i2c1,
	&s3c_device_i2c2,
	&s3c_device_i2c4,
	&s3c_device_i2c5, 
	&s3c_device_i2c6,
	&s3c_device_i2c9,  
	&s3c_device_i2c11, 
	&s3c_device_i2c13, 
#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
#ifdef CONFIG_USB_ANDROID_MASS_STORAGE
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif
	&sec_device_dpram,

	&sec_device_battery,
	&sec_device_switch,  // samsung switch driver

#ifdef CONFIG_S5PV210_POWER_DOMAIN
	&s5pv210_pd_audio,
	&s5pv210_pd_cam,
	&s5pv210_pd_tv,
	&s5pv210_pd_lcd,
	&s5pv210_pd_g3d,
	&s5pv210_pd_mfc,
#endif
#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
	&pmem_adsp_device,
#endif

#ifdef CONFIG_HAVE_PWM
	&s3c_device_timer[0],
	&s3c_device_timer[1],
	&s3c_device_timer[2],
	&s3c_device_timer[3],
#endif
	&sec_device_rfkill,
	&sec_device_btsleep,
	&ram_console_device,
	&sec_device_wifi,	
#ifdef CONFIG_SND_S5P_RP
	&s5p_device_rp,
#endif
};

static void __init forte_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s5pv210_gpiolib_init();
	s3c24xx_init_uarts(forte_uartcfgs, ARRAY_SIZE(forte_uartcfgs));
	s5p_reserve_bootmem(forte_media_devs, ARRAY_SIZE(forte_media_devs));
        
#ifdef CONFIG_MTD_ONENAND
	s5pc110_device_onenand.name = "s5pc110-onenand";
#endif
}

unsigned int pm_debug_scratchpad;

static unsigned int ram_console_start;
static unsigned int ram_console_size;

static void __init forte_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline,
		struct meminfo *mi)
{
	mi->bank[0].start = 0x30000000;
	mi->bank[0].size = 80 * SZ_1M;
	mi->bank[0].node = 0;
	mi->bank[1].start = 0x40000000;
	mi->bank[1].size = 256 * SZ_1M;
	mi->bank[1].node = 1;
	mi->bank[2].start = 0x50000000;
	/* 1M for ram_console buffer */
	mi->bank[2].size = 127 * SZ_1M;
	mi->bank[2].node = 2;
	mi->nr_banks = 3;

	ram_console_start = mi->bank[2].start + mi->bank[2].size;
	ram_console_size = SZ_1M - SZ_4K;

	pm_debug_scratchpad = ram_console_start + ram_console_size;
}

/* this function are used to detect s5pc110 chip version temporally */
int s5pc110_version ;

void _hw_version_check(void)
{
	void __iomem *phy_address ;
	int temp;

	phy_address = ioremap(0x40, 1);

	temp = __raw_readl(phy_address);

	if (temp == 0xE59F010C)
		s5pc110_version = 0;
	else
		s5pc110_version = 1;

	printk(KERN_INFO "S5PC110 Hardware version : EVT%d\n",
				s5pc110_version);

	iounmap(phy_address);
}

/*
 * Temporally used
 * return value 0 -> EVT 0
 * value 1 -> evt 1
 */

int hw_version_check(void)
{
	return s5pc110_version ;
}
EXPORT_SYMBOL(hw_version_check);

static void __init fsa9480_gpio_init(void)
{
	s3c_gpio_cfgpin(GPIO_USB_SEL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_USB_SEL, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_UART_SEL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);
	
	printk("fsa9480_gpio_init: configuring GPIO_UART_SEL1\n");
        s3c_gpio_cfgpin(GPIO_UART_SEL1, S3C_GPIO_OUTPUT);
        s3c_gpio_setpull(GPIO_UART_SEL1, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_JACK_nINT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_JACK_nINT, S3C_GPIO_PULL_NONE);
}

static void __init max8998_gpio_init(void) 
{
	s3c_gpio_cfgpin(GPIO_AP_PMIC_IRQ, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_AP_PMIC_IRQ, S3C_GPIO_PULL_NONE);
}

static void __init setup_ram_console_mem(void)
{
	ram_console_resource[0].start = ram_console_start;
	ram_console_resource[0].end = ram_console_start + ram_console_size - 1;
}

static void __init sound_init(void)
{
	u32 reg;

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x3 << 8);
	reg |= 3 << 8;
	__raw_writel(reg, S5P_OTHERS);

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~(0x1f << 12);
	reg |= 19 << 12;
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~0x1;
	reg |= 0x1;
	__raw_writel(reg, S5P_CLK_OUT);

	gpio_request(GPIO_MICBIAS_EN, "micbias_enable");
}

static void __init onenand_init()
{
	struct clk *clk = clk_get(NULL, "onenand");
	BUG_ON(!clk);
	clk_enable(clk);
}

static void __init forte_machine_init(void)
{
	setup_ram_console_mem();
	s3c_usb_set_serial();
	platform_add_devices(forte_devices, ARRAY_SIZE(forte_devices));

	/* Find out S5PC110 chip version */
	_hw_version_check();



	pm_power_off = forte_power_off ;
#if 0
	s3c_gpio_cfgpin(GPIO_HWREV_MODE0, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE0, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_HWREV_MODE1, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE1, S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(GPIO_HWREV_MODE2, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE2, S3C_GPIO_PULL_NONE);
	system_rev = 0;
	system_rev  = gpio_get_value(GPIO_HWREV_MODE0);
	system_rev = system_rev | (gpio_get_value(GPIO_HWREV_MODE1) << 1);
	system_rev = system_rev | (gpio_get_value(GPIO_HWREV_MODE2) << 2);
	s3c_gpio_cfgpin(GPIO_HWREV_MODE3, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_HWREV_MODE3, S3C_GPIO_PULL_NONE);
	system_rev = system_rev | (gpio_get_value(GPIO_HWREV_MODE3) << 3);
	printk(KERN_INFO "system_rev is 0x%x\n", system_rev);
#endif	
	/*initialise the gpio's*/
	config_init_gpio();

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif
//	gpio_request(GPIO_TOUCH_EN, "touch en");
	/* i2c */
	s3c_i2c0_set_platdata(NULL);
	s3c_i2c1_set_platdata(NULL);
	s3c_i2c2_set_platdata(NULL);
	/* H/W I2C lines */
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
        qt_touch_init();
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));
	/* wm8994 codec */
	sound_init();
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
        fsa9480_gpio_init();
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
	max8998_gpio_init();
	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));
	i2c_register_board_info(9, i2c_devs9, ARRAY_SIZE(i2c_devs9));
//        i2c_register_board_info(11, i2c_devs11, ARRAY_SIZE(i2c_devs11));
	gp2a_gpio_init();
        i2c_register_board_info(13, i2c_devs13, ARRAY_SIZE(i2c_devs13));

#ifdef CONFIG_FB_S3C_S6D16A0X
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
        s3cfb_set_platdata(&s6d16a0x_data);
#endif

#if defined(CONFIG_S5P_ADC)
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

//#if defined(CONFIG_PM)
	s3c_pm_init();
//#endif

#ifdef CONFIG_VIDEO_FIMC
	/* fimc */
	s3c_fimc0_set_platdata(&fimc_plat_lsi);
	s3c_fimc1_set_platdata(&fimc_plat_lsi);
	s3c_fimc2_set_platdata(&fimc_plat_lsi);
#endif

#ifdef CONFIG_VIDEO_JPEG_V2
	s3c_jpeg_set_platdata(&jpeg_plat);
#endif

#ifdef CONFIG_VIDEO_MFC50
	/* mfc */
	s3c_mfc_set_platdata(NULL);
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
	s5pv210_default_sdhci0();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s5pv210_default_sdhci1();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s5pv210_default_sdhci2();
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s5pv210_default_sdhci3();
#endif
#ifdef CONFIG_S5PV210_SETUP_SDHCI
	s3c_sdhci_set_platdata();
#endif
        s3c_gpio_cfgpin( GPIO_AP_SCL_28V, 1 );
        s3c_gpio_setpull( GPIO_AP_SCL_28V, S3C_GPIO_PULL_UP);
        s3c_gpio_cfgpin( GPIO_AP_SDA_28V, 1 );
        s3c_gpio_setpull( GPIO_AP_SDA_28V, S3C_GPIO_PULL_UP);

	register_reboot_notifier(&forte_reboot_notifier);
//         smdk_backlight_register();
	forte_switch_init();
	gps_gpio_init();

	forte_init_wifi_mem();

	onenand_init();
	if (gpio_is_valid(GPIO_MSENSE_nRST)) {
		if (gpio_request(GPIO_MSENSE_nRST, "GPB"))
			printk(KERN_ERR "Failed to request GPIO_MSENSE_nRST!\n");
		gpio_direction_output(GPIO_MSENSE_nRST, 1);
	}
	gpio_free(GPIO_MSENSE_nRST);

#ifdef RECOVERY_SYSFS
	recovery_class_init();
#endif
}

#ifdef CONFIG_USB_SUPPORT
/* Initializes OTG Phy. */
void otg_phy_init(void)
{
	/* USB PHY0 Enable */
	writel(readl(S5P_USB_PHY_CONTROL) | (0x1<<0),
			S5P_USB_PHY_CONTROL);
	writel((readl(S3C_USBOTG_PHYPWR) & ~(0x3<<3) & ~(0x1<<0)) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	writel((readl(S3C_USBOTG_PHYCLK) & ~(0x5<<2)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	writel((readl(S3C_USBOTG_RSTCON) & ~(0x3<<1)) | (0x1<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);
	writel(readl(S3C_USBOTG_RSTCON) & ~(0x7<<0),
			S3C_USBOTG_RSTCON);
	msleep(1);

	/* rising/falling time */
	writel(readl(S3C_USBOTG_PHYTUNE) | (0x1<<20),
			S3C_USBOTG_PHYTUNE);

	/* set DC level as 6 (6%) */
	writel((readl(S3C_USBOTG_PHYTUNE) & ~(0xf)) | (0x1<<2) | (0x1<<1),
			S3C_USBOTG_PHYTUNE);
}
EXPORT_SYMBOL(otg_phy_init);

/* USB Control request data struct must be located here for DMA transfer */
struct usb_ctrlrequest usb_ctrl __attribute__((aligned(64)));

/* OTG PHY Power Off */
void otg_phy_off(void)
{
	writel(readl(S3C_USBOTG_PHYPWR) | (0x3<<3),
			S3C_USBOTG_PHYPWR);
	writel(readl(S5P_USB_PHY_CONTROL) & ~(1<<0),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(otg_phy_off);

void usb_host_phy_init(void)
{
	struct clk *otg_clk;

	otg_clk = clk_get(NULL, "otg");
	clk_enable(otg_clk);

	if (readl(S5P_USB_PHY_CONTROL) & (0x1<<1))
		return;

	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) | (0x1<<1),
			S5P_USB_PHY_CONTROL);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYPWR)
			& ~(0x1<<7) & ~(0x1<<6)) | (0x1<<8) | (0x1<<5),
			S3C_USBOTG_PHYPWR);
	__raw_writel((__raw_readl(S3C_USBOTG_PHYCLK) & ~(0x1<<7)) | (0x3<<0),
			S3C_USBOTG_PHYCLK);
	__raw_writel((__raw_readl(S3C_USBOTG_RSTCON)) | (0x1<<4) | (0x1<<3),
			S3C_USBOTG_RSTCON);
	__raw_writel(__raw_readl(S3C_USBOTG_RSTCON) & ~(0x1<<4) & ~(0x1<<3),
			S3C_USBOTG_RSTCON);
}
EXPORT_SYMBOL(usb_host_phy_init);

void usb_host_phy_off(void)
{
	__raw_writel(__raw_readl(S3C_USBOTG_PHYPWR) | (0x1<<7)|(0x1<<6),
			S3C_USBOTG_PHYPWR);
	__raw_writel(__raw_readl(S5P_USB_PHY_CONTROL) & ~(1<<1),
			S5P_USB_PHY_CONTROL);
}
EXPORT_SYMBOL(usb_host_phy_off);
#endif

MACHINE_START(FORTE, "FORTE")
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup		= forte_fixup,
	.init_irq	= s5pv210_init_irq,
	.map_io		= forte_map_io,
	.init_machine	= forte_machine_init,
	.timer		= &s5p_systimer,
MACHINE_END

void s3c_setup_uart_cfg_gpio(unsigned char port)
{
	switch (port) {
	case 0:
		s3c_gpio_cfgpin(GPIO_BT_RXD, S3C_GPIO_SFN(GPIO_BT_RXD_AF));
		s3c_gpio_setpull(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_TXD, S3C_GPIO_SFN(GPIO_BT_TXD_AF));
		s3c_gpio_setpull(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_CTS, S3C_GPIO_SFN(GPIO_BT_CTS_AF));
		s3c_gpio_setpull(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_BT_RTS, S3C_GPIO_SFN(GPIO_BT_RTS_AF));
		s3c_gpio_setpull(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
		
        //      s3c_gpio_slp_cfgpin(GPIO_BT_RXD, S3C_GPIO_SLP_PREV);
	//	s3c_gpio_slp_setpull_updown(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
	//	s3c_gpio_slp_cfgpin(GPIO_BT_TXD, S3C_GPIO_SLP_PREV);
	//	s3c_gpio_slp_setpull_updown(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
	///	s3c_gpio_slp_cfgpin(GPIO_BT_CTS, S3C_GPIO_SLP_PREV);
	//	s3c_gpio_slp_setpull_updown(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
	//	s3c_gpio_slp_cfgpin(GPIO_BT_RTS, S3C_GPIO_SLP_PREV);
	//	s3c_gpio_slp_setpull_updown(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
		break;
	case 1:
		s3c_gpio_cfgpin(GPIO_GPS_RXD, S3C_GPIO_SFN(GPIO_GPS_RXD_AF));
		s3c_gpio_setpull(GPIO_GPS_RXD, S3C_GPIO_PULL_UP);
		s3c_gpio_cfgpin(GPIO_GPS_TXD, S3C_GPIO_SFN(GPIO_GPS_TXD_AF));
		s3c_gpio_setpull(GPIO_GPS_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_CTS, S3C_GPIO_SFN(GPIO_GPS_CTS_AF));
		s3c_gpio_setpull(GPIO_GPS_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_GPS_RTS, S3C_GPIO_SFN(GPIO_GPS_RTS_AF));
		s3c_gpio_setpull(GPIO_GPS_RTS, S3C_GPIO_PULL_NONE);
		break;
	case 2:
		s3c_gpio_cfgpin(GPIO_AP_RXD, S3C_GPIO_SFN(GPIO_AP_RXD_AF));
		s3c_gpio_setpull(GPIO_AP_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_AP_TXD, S3C_GPIO_SFN(GPIO_AP_TXD_AF));
		s3c_gpio_setpull(GPIO_AP_TXD, S3C_GPIO_PULL_NONE);
		break;
	case 3:
		s3c_gpio_cfgpin(GPIO_FLM_RXD, S3C_GPIO_SFN(GPIO_FLM_RXD_AF));
		s3c_gpio_setpull(GPIO_FLM_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_cfgpin(GPIO_FLM_TXD, S3C_GPIO_SFN(GPIO_FLM_TXD_AF));
		s3c_gpio_setpull(GPIO_FLM_TXD, S3C_GPIO_PULL_NONE);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(s3c_setup_uart_cfg_gpio);

//**** recovery utility interface ****//  
#ifdef RECOVERY_SYSFS

struct class *recovery_class=NULL;
static atomic_t device_count;
static struct device *recovery_dev=NULL;
static char* recovery_name ="recovery";
static int recovery_index=0;

static ssize_t wipedata_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	char cmdline[COMMAND_LINE_SIZE];
	char* pCmd = NULL;	

    printk(KERN_INFO "%s\n", __func__);
    
    if(sec_get_param_value){
        sec_get_param_value(__CMDLINE, cmdline);

        printk("CMDLINE : %s\n",cmdline);
        //This is set from boot
        pCmd = strstr(cmdline, "wipedata");
        if( pCmd ){
            *pCmd = 0x00;
	         sec_set_param_value(__CMDLINE, cmdline);
	         return sprintf(buf, "%s", "wipedata");
        }
        
    }

	return sprintf(buf, "%s", "null");
}

static ssize_t wipedata_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char cmdline[COMMAND_LINE_SIZE];
	char* pCmd = NULL;
	int mode;

    printk(KERN_INFO "%s\n", __func__);
    
    if(strstr(buf, "null"))
	{
        if(sec_get_param_value){
            sec_get_param_value(__CMDLINE, cmdline);

            printk("CMDLINE : %s\n",cmdline);
            
            //This is set from boot
            pCmd = strstr(cmdline, "wipedata");
            if( pCmd ){
                *pCmd = 0x00;
  	            sec_set_param_value(__CMDLINE, cmdline);
            }
        }
	}
    else if(strstr(buf, "wipedata"))
	{
        if(sec_get_param_value){
            sec_get_param_value(__CMDLINE, cmdline);

            printk("CMDLINE : %s\n",cmdline);
            
            //to reboot as recovery mode to format partitions.
            strcat(cmdline, "wipedata");
  	        sec_set_param_value(__CMDLINE, cmdline);

  	        mode = REBOOT_MODE_RECOVERY;
            sec_set_param_value(__REBOOT_MODE, &mode);  	        

        }
	}
	

	return size;
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    printk(KERN_INFO "%s\n", __func__);
    
	return sprintf(buf, "%s\n", recovery_name);
}

static DEVICE_ATTR(wipedata, S_IRUGO | S_IWUSR, wipedata_read, wipedata_write);
static DEVICE_ATTR(name, S_IRUGO | S_IWUSR, name_show, NULL);

static int create_recovery_class(void)
{
    printk(KERN_INFO "%s\n", __func__);
    
	if (!recovery_class) {
		recovery_class = class_create(THIS_MODULE, "recovery");
		if (IS_ERR(recovery_class))
			return PTR_ERR(recovery_class);
		atomic_set(&device_count, 0);
	}

	return 0;
}

int recovery_dev_register(void)
{
	int ret;
	
    printk(KERN_INFO "%s\n", __func__);

	if (!recovery_class) {
		ret = create_recovery_class();
		if (ret < 0)
			return ret;
	}

	recovery_index = atomic_inc_return(&device_count);
	recovery_dev = device_create(recovery_class, NULL,
		MKDEV(0, recovery_index), NULL, recovery_name);
	if (IS_ERR(recovery_dev))
		return PTR_ERR(recovery_dev);

	ret = device_create_file(recovery_dev, &dev_attr_wipedata);
	if (ret < 0)
		goto err_create_file_1;
	ret = device_create_file(recovery_dev, &dev_attr_name);
	if (ret < 0)
		goto err_create_file_2;

	return 0;

err_create_file_2:
	device_remove_file(recovery_dev, &dev_attr_wipedata);
err_create_file_1:
	device_destroy(recovery_class, MKDEV(0, recovery_index));
	printk(KERN_ERR "recovery: Failed to register driver %s\n", recovery_name);

	return ret;
}
EXPORT_SYMBOL_GPL(recovery_dev_register);

void recovery_dev_unregister(struct device *dev)
{
    printk(KERN_INFO "%s\n", __func__);

	device_remove_file(dev, &dev_attr_name);
	device_remove_file(dev, &dev_attr_wipedata);
	device_destroy(recovery_class, MKDEV(0, recovery_index));
}
EXPORT_SYMBOL_GPL(recovery_dev_unregister);

static int __init recovery_class_init(void)
{
	return recovery_dev_register();
}

static void __exit recovery_class_exit(void)
{
    printk(KERN_INFO "%s\n", __func__);
    
	class_destroy(recovery_class);
}

#endif

