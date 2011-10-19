/* linux/arch/arm/mach-s5pv210/mach-aries.c
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
#include <linux/regulator/max8893.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/usb/ch9.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/usb/ch9.h>
#include <linux/input/cypress-touchkey.h>
#include <linux/i2c/qt602240_ts.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/skbuff.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <mach/map.h>
#include <mach/regs-clock.h>
#include <mach/gpio.h>
#include <mach/gpio-crespo.h>
#include <mach/gpio-crespo-settings.h>
#include <mach/adc.h>
#include <mach/param.h>
#include <mach/system.h>
#include <mach/irqs.h>

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

#include <media/s5ka3dfx_platform.h>
#include <media/s5k4ecgx.h>

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
#include <linux/input/k3g.h>
#include <../../../drivers/video/samsung/s3cfb.h>
#include <linux/sec_jack.h>
#include <linux/input/mxt224.h>
#include <linux/max17040_battery.h>
#include <linux/mfd/max8998.h>
#include <linux/switch.h>

#ifdef CONFIG_KERNEL_DEBUG_SEC
#include <linux/kernel_sec_common.h>
#endif

#include "crespo.h"


struct class *sec_class;
EXPORT_SYMBOL(sec_class);

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

void (*sec_set_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_set_param_value);

void (*sec_get_param_value)(int idx, void *value);
EXPORT_SYMBOL(sec_get_param_value);
#define IRQ_TOUCH_INT (IRQ_EINT_GROUP18_BASE+5)


#define KERNEL_REBOOT_MASK      0xFFFFFFFF
#define REBOOT_MODE_FAST_BOOT		7

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wifi_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static int aries_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	int mode = REBOOT_MODE_NONE;

	if ((code == SYS_RESTART) && _cmd) {
		if (!strcmp((char *)_cmd, "recovery"))
			mode = REBOOT_MODE_RECOVERY;
		else if (!strcmp((char *)_cmd, "bootloader"))
			mode = REBOOT_MODE_FAST_BOOT;
		else
			mode = REBOOT_MODE_NONE;
	}
	if(code != SYS_POWER_OFF ){
		if(sec_set_param_value){
			sec_set_param_value(__REBOOT_MODE,&mode);
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block aries_reboot_notifier = {
	.notifier_call = aries_notifier_call,
};

static void gps_gpio_init(void)
{
	struct device *gps_dev;

	gps_dev = device_create(sec_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("Failed to create device(gps)!\n");
		goto err;
	}

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

static void aries_switch_init(void)
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

static struct s3c2410_uartcfg aries_uartcfgs[] __initdata = {
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

static struct s3cfb_lcd tl2796 = {
        .width = 480,
        .height = 800,
        .p_width = 52,
        .p_height = 86,
        .bpp = 24,
        .freq = 60,

        .timing = {
                .h_fp = 16,
                .h_bp = 16,
                .h_sw = 2,
                .v_fp = 28,
                .v_fpe = 1,
                .v_bp = 1,
                .v_bpe = 1,
                .v_sw = 2,
        },
        .polarity = {
                .rise_vclk = 1,
                .inv_hsync = 1,
                .inv_vsync = 1,
                .inv_vden = 1,
        },
};

static struct s3cfb_lcd nt35580 = {
	.width = 480,
	.height = 800,
	.p_width = 52,
	.p_height = 86,
	.bpp = 24,
	.freq = 60,
	.timing = {
		.h_fp = 10,
		.h_bp = 20,
		.h_sw = 10,
		.v_fp = 6,
		.v_fpe = 1,
		.v_bp = 8,
		.v_bpe = 1,
		.v_sw = 2,
	},
	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 1,
	},
};

#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC0 (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC1 (9900 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMC2 (6144 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC0 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_MFC1 (36864 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_FIMD (3072 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_JPEG (8192 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_PMEM (8192 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_GPU1 (3072 * SZ_1K)
#define  S5PV210_VIDEO_SAMSUNG_MEMSIZE_ADSP (6144 * SZ_1K)
static struct s5p_media_device crespo_media_devs[] = {
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
};

static struct regulator_consumer_supply ldo3_consumer[] = {
	{	.supply	= "usb_io", },
};

static struct regulator_consumer_supply ldo7_consumer[] = {
	{	.supply	= "vlcd", },
};

static struct regulator_consumer_supply ldo8_consumer[] = {
	{	.supply = "usb_core", },
};

static struct regulator_consumer_supply ldo11_consumer[] = {
	{	.supply	= "cam_af", },
};

static struct regulator_consumer_supply ldo12_consumer[] = {
	{	.supply	= "cam_isp_host", },
};

static struct regulator_consumer_supply ldo13_consumer[] = {
	{	.supply	= "vga_vddio", },
};

static struct regulator_consumer_supply ldo14_consumer[] = {
	{	.supply	= "vga_dvdd", },
};

static struct regulator_consumer_supply ldo15_consumer[] = {
	{	.supply	= "vga_avdd", },
};

static struct regulator_consumer_supply ldo5_consumer[] = {
	{	.supply	= "cam_sensor", },
};

static struct regulator_consumer_supply ldo17_consumer[] = {
	{	.supply	= "vcc_lcd", },
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

static struct regulator_init_data aries_ldo2_data = {
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

static struct regulator_init_data aries_ldo3_data = {
	.constraints	= {
		.name		= "VUSB_1.1V",
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

static struct regulator_init_data aries_ldo4_data = {
	.constraints	= {
		.name		= "VADC_3.3V",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
};

static struct regulator_init_data aries_ldo7_data = {
	.constraints	= {
		.name		= "VLCD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo7_consumer),
	.consumer_supplies	= ldo7_consumer,
};

static struct regulator_init_data aries_ldo8_data = {
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

static struct regulator_init_data aries_ldo9_data = {
	.constraints	= {
		.name		= "VCC_2.8V_PDA",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
	},
};

static struct regulator_init_data aries_ldo11_data = {
	.constraints	= {
		.name		= "CAM_AF_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo11_consumer),
	.consumer_supplies	= ldo11_consumer,
};

static struct regulator_init_data aries_ldo5_data = {
	.constraints	= {
		.name		= "CAM_SENSOR_CORE_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo5_consumer),
	.consumer_supplies	= ldo5_consumer,
};

static struct regulator_init_data aries_ldo13_data = {
	.constraints	= {
		.name		= "VGA_VDDIO_2.8V",
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

static struct regulator_init_data aries_ldo14_data = {
	.constraints	= {
		.name		= "VGA_DVDD_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo14_consumer),
	.consumer_supplies	= ldo14_consumer,
};

static struct regulator_init_data aries_ldo12_data = {
	.constraints	= {
		.name		= "CAM_ISP_HOST_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo12_consumer),
	.consumer_supplies	= ldo12_consumer,
};

static struct regulator_init_data aries_ldo15_data = {
	.constraints	= {
		.name		= "VGA_AVDD_2.8V",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo15_consumer),
	.consumer_supplies	= ldo15_consumer,
};

static struct regulator_init_data aries_ldo17_data = {
	.constraints	= {
		.name		= "VCC_3.0V_LCD",
		.min_uV		= 3000000,
		.max_uV		= 3000000,
		.apply_uV	= 1,
		.always_on	= 0,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled = 1,
		},
	},
	.num_consumer_supplies	= ARRAY_SIZE(ldo17_consumer),
	.consumer_supplies	= ldo17_consumer,
};

static struct regulator_init_data aries_buck1_data = {
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


static struct regulator_init_data aries_buck2_data = {
	.constraints	= {
		.name		= "VDD_INT",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.apply_uV	= 1,
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

static struct regulator_init_data aries_buck3_data = {
	.constraints	= {
		.name		= "VCC_1.8V",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
	},
};

static struct regulator_init_data aries_buck4_data = {
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

static struct max8998_regulator_data aries_regulators[] = {
	{ MAX8998_LDO2,  &aries_ldo2_data },
	{ MAX8998_LDO3,  &aries_ldo3_data },
	{ MAX8998_LDO4,  &aries_ldo4_data },
	{ MAX8998_LDO7,  &aries_ldo7_data },
	{ MAX8998_LDO8,  &aries_ldo8_data },
	{ MAX8998_LDO9,  &aries_ldo9_data },
	{ MAX8998_LDO11, &aries_ldo11_data },
	{ MAX8998_LDO12, &aries_ldo12_data },
	{ MAX8998_LDO13, &aries_ldo13_data },
	{ MAX8998_LDO14, &aries_ldo14_data },
	{ MAX8998_LDO15, &aries_ldo15_data },
	{ MAX8998_LDO5, &aries_ldo5_data },
	{ MAX8998_LDO17, &aries_ldo17_data },
	{ MAX8998_BUCK1, &aries_buck1_data },
	{ MAX8998_BUCK2, &aries_buck2_data },
	{ MAX8998_BUCK3, &aries_buck3_data },
	{ MAX8998_BUCK4, &aries_buck4_data },
};

static struct max8998_adc_table_data temper_table[] =  {
	{  264,  650 },
	{  275,  640 },
	{  286,  630 },
	{  293,  620 },
	{  299,  610 },
	{  306,  600 },
#if !defined(CONFIG_ARIES_NTT)
	{  324,  590 },
	{  341,  580 },
	{  354,  570 },
	{  368,  560 },
#else
	{  310,  590 },
	{  315,  580 },
	{  320,  570 },
	{  324,  560 },
#endif
	{  381,  550 },
	{  396,  540 },
	{  411,  530 },
	{  427,  520 },
	{  442,  510 },
	{  457,  500 },
	{  472,  490 },
	{  487,  480 },
	{  503,  470 },
	{  518,  460 },
	{  533,  450 },
	{  554,  440 },
	{  574,  430 },
	{  595,  420 },
	{  615,  410 },
	{  636,  400 },
	{  656,  390 },
	{  677,  380 },
	{  697,  370 },
	{  718,  360 },
	{  738,  350 },
	{  761,  340 },
	{  784,  330 },
	{  806,  320 },
	{  829,  310 },
	{  852,  300 },
	{  875,  290 },
	{  898,  280 },
	{  920,  270 },
	{  943,  260 },
	{  966,  250 },
	{  990,  240 },
	{ 1013,  230 },
	{ 1037,  220 },
	{ 1060,  210 },
	{ 1084,  200 },
	{ 1108,  190 },
	{ 1131,  180 },
	{ 1155,  170 },
	{ 1178,  160 },
	{ 1202,  150 },
	{ 1226,  140 },
	{ 1251,  130 },
	{ 1275,  120 },
	{ 1299,  110 },
	{ 1324,  100 },
	{ 1348,   90 },
	{ 1372,   80 },
	{ 1396,   70 },
	{ 1421,   60 },
	{ 1445,   50 },
	{ 1468,   40 },
	{ 1491,   30 },
	{ 1513,   20 },
#if !defined(CONFIG_ARIES_NTT)
	{ 1536,   10 },
	{ 1559,    0 },
	{ 1577,  -10 },
	{ 1596,  -20 },
#else
	{ 1518,   10 },
	{ 1524,    0 },
	{ 1544,  -10 },
	{ 1570,  -20 },
#endif
	{ 1614,  -30 },
	{ 1619,  -40 },
	{ 1632,  -50 },
	{ 1658,  -60 },
	{ 1667,  -70 }, 
};
struct max8998_charger_callbacks *callbacks;
static enum cable_type_t set_cable_status;

static void max8998_charger_register_callbacks(
		struct max8998_charger_callbacks *ptr)
{
	callbacks = ptr;
	/* if there was a cable status change before the charger was
	ready, send this now */
	if ((set_cable_status != 0) && callbacks && callbacks->set_cable)
		callbacks->set_cable(callbacks, set_cable_status);
}

static struct max8998_charger_data aries_charger = {
	.register_callbacks = &max8998_charger_register_callbacks,
	.adc_table		= temper_table,
	.adc_array_size		= ARRAY_SIZE(temper_table),
};

static struct adc_channel_type s3c_adc_channel = {       
        .s3c_adc_temperature = 1,
        .s3c_adc_chg_current = 2,
	.s3c_adc_v_f = 4,
	.s3c_adc_hw_version = 7,
	.s3c_adc_voltage = 8,
};

static struct max8998_platform_data max8998_pdata = {
	.num_regulators = ARRAY_SIZE(aries_regulators),
	.regulators     = aries_regulators,
	.charger        = &aries_charger,
	.buck1_set1             = GPIO_BUCK_1_EN_A,
        .buck1_set2             = GPIO_BUCK_1_EN_B,
        .buck2_set3             = GPIO_BUCK_2_EN,
        .buck1_max_voltage1     = 1200000,
        .buck1_max_voltage2     = 1275000,
        .buck2_max_voltage      = 1100000,
	.s3c_adc_channel	= &s3c_adc_channel,
};


static struct regulator_init_data max8893_ldo1_data = {
        .constraints    = {
                .name           = "WIMAX_2.9V",
                .min_uV         = 2900000,
                .max_uV         = 2900000,
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct regulator_init_data max8893_ldo2_data = {
        .constraints    = {
                .name           = "TOUCH_KEY_3.0V",
                .min_uV         = 3000000,   //20100628_inchul(from HW) 2.8V -> 3.0V
                .max_uV         = 3000000,  //20100628_inchul(from HW) 2.8V -> 3.0V
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct regulator_init_data max8893_ldo3_data = {
        .constraints    = {
                .name           = "VCC_MOTOR_3.0V",
                .min_uV         = 3000000,
                .max_uV         = 3000000,
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct regulator_init_data max8893_ldo4_data = {
        .constraints    = {
                .name           = "WIMAX_SDIO_3.0V",
                .min_uV         = 3000000,
                .max_uV         = 3000000,
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct regulator_init_data max8893_ldo5_data = {
        .constraints    = {
                .name           = "VDD_RF & IO_1.8V",
                .min_uV         = 1800000,
                .max_uV         = 1800000,
                .always_on      = 0,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct regulator_init_data max8893_buck_data = {
        .constraints    = {
                .name           = "VDDA & SDRAM & WIMAX_USB & RFC1C4_1.8V",
                .min_uV         = 1800000,
                .max_uV         = 1800000,
                .always_on      = 1,
                .apply_uV       = 1,
                .valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
        },
};

static struct max8893_subdev_data universal_8893_regulators[] = {
        { MAX8893_LDO1, &max8893_ldo1_data },
        { MAX8893_LDO2, &max8893_ldo2_data },
  { MAX8893_LDO3, &max8893_ldo3_data },
        { MAX8893_LDO4, &max8893_ldo4_data },
  { MAX8893_LDO5, &max8893_ldo5_data },
  { MAX8893_BUCK, &max8893_buck_data },
};

static struct max8893_platform_data max8893_platform_data = {
        .num_regulators = ARRAY_SIZE(universal_8893_regulators),
        .regulators     = universal_8893_regulators,
};

static struct i2c_board_info i2c_devs12[] __initdata = {
        {
                I2C_BOARD_INFO("max8893", (0x3E)),
                .platform_data = &max8893_platform_data,
        },
};

struct platform_device s3c_device_8893consumer = {
        .name             = "max8893-consumer",
        .id               = 0,
        .dev = { .platform_data = &max8893_platform_data },
};



struct platform_device sec_device_dpram = {
	.name	= "dpram-device",
	.id	= -1,
};

static void panel_cfg_gpio(struct platform_device *pdev)
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
#ifdef CONFIG_FB_S3C_MDNIE
	writel(0x1, S5P_MDNIE_SEL);
#else
	writel(0x2, S5P_MDNIE_SEL);
#endif

	/* DISPLAY_CS */
	s3c_gpio_cfgpin(S5PV210_MP01(1), S3C_GPIO_SFN(1));
	/* DISPLAY_CLK */
	s3c_gpio_cfgpin(S5PV210_MP04(1), S3C_GPIO_SFN(1));
	/* DISPLAY_SO */
	s3c_gpio_cfgpin(S5PV210_MP05(0), S3C_GPIO_SFN(1));
	/* DISPLAY_SI */
	s3c_gpio_cfgpin(S5PV210_MP04(3), S3C_GPIO_SFN(1));

	/* DISPLAY_CS */
	s3c_gpio_setpull(S5PV210_MP01(1), S3C_GPIO_PULL_NONE);
	/* DISPLAY_CLK */
	s3c_gpio_setpull(S5PV210_MP04(1), S3C_GPIO_PULL_NONE);
	/* DISPLAY_SO */
	s3c_gpio_setpull(S5PV210_MP05(0), S3C_GPIO_PULL_NONE);
	/* DISPLAY_SI */
	s3c_gpio_setpull(S5PV210_MP04(3), S3C_GPIO_PULL_NONE);
}

void lcd_cfg_gpio_early_suspend(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF0(i), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S5PV210_GPF0(i), S3C_GPIO_PULL_NONE);
		gpio_set_value(S5PV210_GPF0(i), 0);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF1(i), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S5PV210_GPF1(i), S3C_GPIO_PULL_NONE);
		gpio_set_value(S5PV210_GPF1(i), 0);
	}

	for (i = 0; i < 8; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF2(i), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S5PV210_GPF2(i), S3C_GPIO_PULL_NONE);
		gpio_set_value(S5PV210_GPF2(i), 0);
	}

	for (i = 0; i < 4; i++) {
		s3c_gpio_cfgpin(S5PV210_GPF3(i), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(S5PV210_GPF3(i), S3C_GPIO_PULL_NONE);
		gpio_set_value(S5PV210_GPF3(i), 0);
	}
	/* drive strength to min */
	writel(0x00000000, S5P_VA_GPIO + 0x12c); /* GPF0DRV */
	writel(0x00000000, S5P_VA_GPIO + 0x14c); /* GPF1DRV */
	writel(0x00000000, S5P_VA_GPIO + 0x16c); /* GPF2DRV */
	writel(0x00000000, S5P_VA_GPIO + 0x18c); /* GPF3DRV */

	/* LCD_RST */
	s3c_gpio_cfgpin(GPIO_MLCD_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MLCD_RST, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_MLCD_RST, 0);

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
}
EXPORT_SYMBOL(lcd_cfg_gpio_late_resume);

static int panel_reset_lcd(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PV210_MP05(5), "MLCD_RST");
	if (err) {
		printk(KERN_ERR "failed to request MP0(5) for "
				"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PV210_MP05(5), 1);
	msleep(10);

	gpio_set_value(S5PV210_MP05(5), 0);
	msleep(10);

	gpio_set_value(S5PV210_MP05(5), 1);
	msleep(10);

	gpio_free(S5PV210_MP05(5));

	return 0;
}

static int panel_backlight_on(struct platform_device *pdev)
{
	return 0;
}

static struct s3c_platform_fb nt35580_data __initdata = {
	.hw_ver         = 0x62,
	.clk_name       = "sclk_fimd",
	.nr_wins        = 5,
	.default_win    = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap           = FB_SWAP_HWORD | FB_SWAP_WORD,

	.lcd            = &nt35580,
	.cfg_gpio       = panel_cfg_gpio,
	.backlight_on   = panel_backlight_on,
	.reset_lcd      = panel_reset_lcd,
};

static struct s3c_platform_fb tl2796_data __initdata = {
	.hw_ver         = 0x62,
	.clk_name       = "sclk_fimd",
	.nr_wins        = 5,
	.default_win    = CONFIG_FB_S3C_DEFAULT_WINDOW,
	.swap           = FB_SWAP_HWORD | FB_SWAP_WORD,

	.lcd 		= &tl2796,
	.cfg_gpio       = panel_cfg_gpio,
	.backlight_on   = panel_backlight_on,
	.reset_lcd      = panel_reset_lcd,
};

#define LCD_BUS_NUM     3
#define DISPLAY_CS      S5PV210_MP01(1)
#define SUB_DISPLAY_CS  S5PV210_MP01(2)
#define DISPLAY_CLK     S5PV210_MP04(1)
#define DISPLAY_SI      S5PV210_MP04(3)
#define DISPLAY_SO      S5PV210_MP05(0)

static struct spi_board_info spi_board_info[] __initdata = {
	{
		.modalias       = "tl2796",
		.platform_data  = &aries_panel_data,
		.max_speed_hz   = 1200000,
		.bus_num        = LCD_BUS_NUM,
		.chip_select    = 0,
		.mode           = SPI_MODE_3,
		.controller_data = (void *)DISPLAY_CS,
	},
};

static struct spi_gpio_platform_data tl2796_spi_gpio_data = {
	.sck    = DISPLAY_CLK,
	.mosi   = DISPLAY_SI,
	.miso   = -1,
	.num_chipselect = 2,
};

static struct spi_board_info spi_board_info_tft[] __initdata = {
	{
		.modalias       = "nt35580",
		.platform_data  = &nt35580_panel_data_tft,
		.max_speed_hz   = 1200000,
		.bus_num        = LCD_BUS_NUM,
		.chip_select    = 0,
		.mode           = SPI_MODE_3,
		.controller_data = (void *)DISPLAY_CS,
	},
};

static struct spi_gpio_platform_data nt35580_spi_gpio_data = {
	.sck	= DISPLAY_CLK,
	.mosi	= DISPLAY_SI,
	.miso	= DISPLAY_SO,
	.num_chipselect = 2,
};

static struct platform_device s3c_device_spi_gpio = {
	.name	= "spi_gpio",
	.id	= LCD_BUS_NUM,
	.dev	= {
		.parent		= &s3c_device_fb.dev,
		.platform_data	= &nt35580_spi_gpio_data,
	},
};

struct vibrator_platform_data vibrator_data = {

       .timer_id = 2,
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
        .sda_pin                = AP_SDA_30V,
        .scl_pin                = AP_SCL_30V,
        .udelay                 = 2,    /* 250KHz */
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c5 = {
        .name                           = "i2c-gpio",
        .id                                     = 5,
        .dev.platform_data      = &i2c5_platdata,
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


static  struct  i2c_gpio_platform_data  i2c11_platdata = {
	.sda_pin                = GPIO_FM_SDA_28V,
	.scl_pin                = GPIO_FM_SCL_28V,
	.udelay                 = 2,    /* 250KHz */
	.sda_is_open_drain      = 0,
	.scl_is_open_drain      = 0,
	.scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c11 = {
	.name			= "i2c-gpio",
	.id			= 11,
	.dev.platform_data	= &i2c11_platdata,
};


static  struct  i2c_gpio_platform_data  i2c12_platdata = {
        .sda_pin                = GPIO_WIMAX_PM_SDA,
        .scl_pin                = GPIO_WIMAX_PM_SCL,
        .udelay                 = 2,    /* 250KHz */
        .sda_is_open_drain      = 0,
        .scl_is_open_drain      = 0,
        .scl_is_output_only     = 0,
};

static struct platform_device s3c_device_i2c12 = {
        .name                   = "i2c-gpio",
        .id                     = 12,
        .dev.platform_data      = &i2c12_platdata,
};


static  struct  i2c_gpio_platform_data  i2c13_platdata = {
        .sda_pin                = GYRO_SDA_28V,
        .scl_pin                = GYRO_SCL_28V,
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




static void touch_keypad_gpio_init(void)
{
        s3c_gpio_cfgpin(S5PV210_GPJ2(3), S3C_GPIO_SFN(0x1));
        gpio_set_value(S5PV210_GPJ2(3), 1/*level= high*/);
        s3c_gpio_cfgpin(S5PV210_GPJ2(5), S3C_GPIO_SFN(0x1));
        gpio_set_value(S5PV210_GPJ2(5), 1/*level= high*/);
        s3c_gpio_cfgpin(S5PV210_GPJ2(6), S3C_GPIO_SFN(0x1));
        gpio_set_value(S5PV210_GPJ2(6), 1/*level= high*/);
        s3c_gpio_cfgpin(S5PV210_GPJ3(2), S3C_GPIO_SFN(0x1));
        gpio_set_value(S5PV210_GPJ3(2), 1/*level= high*/);
}

static void touch_keypad_onoff(int onoff,int key)
{
	   switch(key)
                      {
                         case KEY_BACK:
                                 gpio_set_value(S5PV210_GPJ2(3),onoff/*level= high*/);
                              break;
                         case KEY_MENU:
                                 gpio_set_value(S5PV210_GPJ2(5),onoff/*level= high*/);
                              break;
                         case KEY_SEARCH:
                                 gpio_set_value(S5PV210_GPJ2(6),onoff/*level= high*/);
                              break;
                         case KEY_HOME:
                                 gpio_set_value(S5PV210_GPJ3(2),onoff/*level= high*/);
                              break;
                         case TOUCH_KEY_LEDS:
                               gpio_set_value(S5PV210_GPJ2(3),onoff);
                               gpio_set_value(S5PV210_GPJ2(5),onoff);
                               gpio_set_value(S5PV210_GPJ2(6),onoff);
                               gpio_set_value(S5PV210_GPJ3(2),onoff);
                              break;
                         default:
                               printk("unsupported unknown touchkey LED\n");
                      }

}
static const int touch_keypad_code[] = {NULL, KEY_SEARCH, KEY_BACK, KEY_HOME, KEY_MENU};
struct qt602240_platform_data qt602240_data = {
        .x_size = 899,
        .y_size = 479,
        #ifdef CONFIG_QT602240_TOUCHKEY
        .is_touchkey_enabled = 1,
        #else
        .is_touchkey_enabled = 0,
        #endif
        .keycode_cnt = ARRAY_SIZE(touch_keypad_code),
        .keycode = touch_keypad_code,
        .touchkey_area = {
                          .y_start =800,
                          .y_end   =899,
        },
	.touchkey_onoff = touch_keypad_onoff,
	.irq = IRQ_TOUCH_INT,
	.touch_int = GPIO_TOUCH_INT,
	.touch_en  = GPIO_TOUCH_EN,
};

static struct gpio_event_direct_entry aries_keypad_key_map[] = {
	{
		.gpio	= S5PV210_GPH2(6),
		.code	= KEY_POWER,
	},
	{
		.gpio	= S5PV210_GPH3(1),
		.code	= KEY_VOLUMEDOWN,
	},
	{
		.gpio	= S5PV210_GPH3(0),
		.code	= KEY_VOLUMEUP,
	},
	{
		.gpio	= S5PV210_GPH3(5),
		.code	= KEY_HOME,
	}
};

static struct gpio_event_input_info aries_keypad_key_info = {
	.info.func = gpio_event_input_func,
	.info.no_suspend = true,
	.debounce_time.tv.nsec = 5 * NSEC_PER_MSEC,
	.type = EV_KEY,
	.keymap = aries_keypad_key_map,
	.keymap_size = ARRAY_SIZE(aries_keypad_key_map)
};

static struct gpio_event_info *aries_input_info[] = {
	&aries_keypad_key_info.info,
};


static struct gpio_event_platform_data aries_input_data = {
	.names = {
		"aries-keypad",
		NULL,
	},
	.info = aries_input_info,
	.info_count = ARRAY_SIZE(aries_input_info),
};

static struct platform_device aries_input_device = {
	.name = GPIO_EVENT_DEV_NAME,
	.id = 0,
	.dev = {
		.platform_data = &aries_input_data,
	},
};

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
	if (system_rev < 0x09) {
		unsigned long flags;
		spin_lock_irqsave(&mic_bias_lock, flags);
		jack_mic_bias = on;
		set_shared_mic_bias();
		spin_unlock_irqrestore(&mic_bias_lock, flags);
	} else
		gpio_set_value(GPIO_EAR_MICBIAS_EN, on);
}

static struct wm8994_platform_data wm8994_pdata = {
	.ldo = GPIO_CODEC_LDO_EN,
	.ear_sel = GPIO_EAR_SEL,
	.set_mic_bias = wm8994_set_mic_bias,
};

/*
 * Guide for Camera Configuration for Crespo board
 * ITU CAM CH A: LSI s5k4ecgx
 */
static DEFINE_MUTEX(s5k4ecgx_lock);
static struct regulator *cam_isp_core_regulator;
static struct regulator *cam_isp_host_regulator;
static struct regulator *cam_af_regulator;
static struct regulator *cam_sensor;
static bool s5k4ecgx_powered_on;
static int s5k4ecgx_regulator_init(void)
{
	pr_err("mach-crespo s5k4ecgx_regulator_init\n");

	if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
		cam_isp_core_regulator = regulator_get(NULL, "cam_isp_core");
		if (IS_ERR_OR_NULL(cam_isp_core_regulator)) {
			pr_err("failed to get cam_isp_core regulator");
			return -EINVAL;
		}
	}
	if (IS_ERR_OR_NULL(cam_isp_host_regulator)) {
		cam_isp_host_regulator = regulator_get(NULL, "cam_isp_host");
		if (IS_ERR_OR_NULL(cam_isp_host_regulator)) {
			pr_err("failed to get cam_isp_host regulator");
			return -EINVAL;
		}
	}
	if (IS_ERR_OR_NULL(cam_af_regulator)) {
		cam_af_regulator = regulator_get(NULL, "cam_af");
		if (IS_ERR_OR_NULL(cam_af_regulator)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
	if (IS_ERR_OR_NULL(cam_sensor)) {
		cam_sensor = regulator_get(NULL, "cam_sensor");
		if (IS_ERR_OR_NULL(cam_sensor)) {
			pr_err("failed to get cam_af regulator");
			return -EINVAL;
		}
	}
    
	pr_err("cam_isp_core_regulator = %p\n", cam_isp_core_regulator);
	pr_err("cam_isp_host_regulator = %p\n", cam_isp_host_regulator);
	pr_err("cam_af_regulator = %p\n", cam_af_regulator);
	pr_err("cam_sensor = %p\n", cam_sensor);
    
	return 0;
}

static void s5k4ecgx_init(void)
{
	pr_err("mach-crespo s5k4ecgx_init\n");

	/* CAM_IO_EN - GPB(7) */
//	if (gpio_request(GPIO_GPB7, "GPB7") < 0)
//		pr_err("failed gpio_request(GPB7) for camera control\n");
	/* CAM_MEGA_nRST - GPJ1(5) */
	if (gpio_request(GPIO_CAM_MEGA_nRST, "GPJ1") < 0)
		pr_err("failed gpio_request(GPJ1) for camera control\n");
	/* CAM_MEGA_EN - GPJ0(6) */
	if (gpio_request(GPIO_CAM_MEGA_EN, "GPJ0") < 0)
		pr_err("failed gpio_request(GPJ0) for camera control\n");
	/* FLASH_EN - GPJ1(2) */
	if (gpio_request(GPIO_FLASH_EN, "GPIO_FLASH_EN") < 0)
		pr_err("failed gpio_request(GPIO_FLASH_EN)\n");
	/* FLASH_EN_SET - GPJ1(0) */
	if (gpio_request(GPIO_CAM_FLASH_EN_SET, "GPIO_CAM_FLASH_EN_SET") < 0)
		pr_err("failed gpio_request(GPIO_CAM_FLASH_EN_SET)\n");
}

static int s5k4ecgx_ldo_en(bool en)
{
	int err = 0;
	int result;

	pr_err("mach-crespo s5k4ecgx_ldo_en\n");

	if (IS_ERR_OR_NULL(cam_isp_core_regulator) ||
		IS_ERR_OR_NULL(cam_isp_host_regulator) ||
		IS_ERR_OR_NULL(cam_af_regulator)||
		IS_ERR_OR_NULL(cam_sensor)) {
		pr_err("Camera regulators not initialized\n");
		return -EINVAL;
	}

	if (!en)
		goto off;

	/* Turn CAM_ISP_CORE_1.2V(VDD_REG) on */
	err = regulator_enable(cam_isp_core_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		goto off;
	}
	mdelay(1);

	/* Turn CAM_SENSOR_A_2.8V(VDDA) on */
//	gpio_set_value(GPIO_GPB7, 1);
	err = regulator_enable(cam_sensor);
	if (err) {
		pr_err("Failed to enable regulator cam_sensor\n");
		goto off;
	}
	mdelay(1);

	/* Turn CAM_ISP_HOST_2.8V(VDDIO) on */
	err = regulator_enable(cam_isp_host_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		goto off;
	}
	udelay(50);

	/* Turn CAM_AF_2.8V or 3.0V on */
	err = regulator_enable(cam_af_regulator);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_core\n");
		goto off;
	}
	udelay(50);
	return 0;

off:
	result = err;
	err = regulator_disable(cam_af_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}
	err = regulator_disable(cam_isp_host_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}
//	gpio_set_value(GPIO_GPB7, 0);
	err = regulator_disable(cam_sensor);
	if (err) {
		pr_err("Failed to disable regulator cam_sensor\n");
		result = err;
	}
	err = regulator_disable(cam_isp_core_regulator);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_core\n");
		result = err;
	}
	return result;
}

static int s5k4ecgx_power_on(void)
{
	/* LDO on */
	int err;

	pr_err("mach-crespo s5k4ecgx_power_on\n");

	/* can't do this earlier because regulators aren't available in
	 * early boot
	 */
	if (s5k4ecgx_regulator_init()) {
		pr_err("Failed to initialize camera regulators\n");
		return -EINVAL;
	}

	err = s5k4ecgx_ldo_en(true);
	if (err)
		return err;
	mdelay(66);

	/* MCLK on - default is input, to save power when camera not on */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(GPIO_CAM_MCLK_AF));
	mdelay(1);

	/* CAM_MEGA_EN - GPJ1(2) LOW */
	gpio_set_value(GPIO_CAM_MEGA_EN, 1);
	mdelay(1);

	/* CAM_MEGA_nRST - GPJ1(5) LOW */
	gpio_set_value(GPIO_CAM_MEGA_nRST, 1);
	mdelay(1);

	return 0;
}

static int s5k4ecgx_power_off(void)
{
	pr_err("mach-crespo s5k4ecgx_power_off\n");

	/* CAM_MEGA_nRST - GPJ1(5) LOW */
	gpio_set_value(GPIO_CAM_MEGA_nRST, 0);
	udelay(60);

	/*  Mclk disable - set to input function to save power */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	udelay(10);

	/* CAM_MEGA_EN - GPJ1(2) LOW */
	gpio_set_value(GPIO_CAM_MEGA_EN, 0);
	udelay(10);

	s5k4ecgx_ldo_en(false);
	mdelay(1);

	return 0;
}

static int s5k4ecgx_power_en(int onoff)
{
	int err = 0;
	pr_err("mach-crespo s5k4ecgx_power_en\n");
    
	mutex_lock(&s5k4ecgx_lock);
	/* we can be asked to turn off even if we never were turned
	 * on if something odd happens and we are closed
	 * by camera framework before we even completely opened.
	 */
	if (onoff != s5k4ecgx_powered_on) {
		if (onoff)
			err = s5k4ecgx_power_on();
		else
			err = s5k4ecgx_power_off();
		if (!err)
			s5k4ecgx_powered_on = onoff;
	}
	mutex_unlock(&s5k4ecgx_lock);
	return err;
}

#define FLASH_MOVIE_MODE_CURRENT_50_PERCENT	7

#define FLASH_TIME_LATCH_US			500
#define FLASH_TIME_EN_SET_US			1

/* The AAT1274 uses a single wire interface to write data to its
 * control registers. An incoming value is written by sending a number
 * of rising edges to EN_SET. Data is 4 bits, or 1-16 pulses, and
 * addresses are 17 pulses or more. Data written without an address
 * controls the current to the LED via the default address 17. */
static void aat1274_write(int value)
{
	while (value--) {
		gpio_set_value(GPIO_CAM_FLASH_EN_SET, 0);
		udelay(FLASH_TIME_EN_SET_US);
		gpio_set_value(GPIO_CAM_FLASH_EN_SET, 1);
		udelay(FLASH_TIME_EN_SET_US);
	}
	udelay(FLASH_TIME_LATCH_US);
	/* At this point, the LED will be on */
}

static int aat1274_flash(int enable)
{
	/* Turn main flash on or off by asserting a value on the EN line. */
	gpio_set_value(GPIO_FLASH_EN, !!enable);

	return 0;
}

static int aat1274_af_assist(int enable)
{
	/* Turn assist light on or off by asserting a value on the EN_SET
	 * line. The default illumination level of 1/7.3 at 100% is used */
	gpio_set_value(GPIO_CAM_FLASH_EN_SET, !!enable);
	if (!enable)
		gpio_set_value(GPIO_FLASH_EN, 0);

	return 0;
}

static int aat1274_torch(int enable)
{
	/* Turn torch mode on or off by writing to the EN_SET line. A level
	 * of 1/7.3 and 50% is used (half AF assist brightness). */
	if (enable) {
		aat1274_write(FLASH_MOVIE_MODE_CURRENT_50_PERCENT);
	} else {
		gpio_set_value(GPIO_CAM_FLASH_EN_SET, 0);
		gpio_set_value(GPIO_FLASH_EN, 0);
	}

	return 0;
}

static struct s5k4ecgx_platform_data s5k4ecgx_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.flash_onoff = &aat1274_flash,
	.af_assist_onoff = &aat1274_af_assist,
	.torch_onoff = &aat1274_torch,
};

static struct i2c_board_info  s5k4ecgx_i2c_info = {
	I2C_BOARD_INFO("S5K4ECGX", 0x5A>>1),
	.platform_data = &s5k4ecgx_plat,
};

static struct s3c_platform_camera s5k4ecgx = {
	.id = CAMERA_PAR_A,
	.type = CAM_TYPE_ITU,
	.fmt = ITU_601_YCBCR422_8BIT,
	.order422 = CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum = 0,
	.info = &s5k4ecgx_i2c_info,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.srclk_name = "xusbxti",
	.clk_name = "sclk_cam",
	.clk_rate = 24000000,
	.line_length = 1920,
	.width = 640,
	.height = 480,
	.window = {
		.left = 0,
		.top = 0,
		.width = 640,
		.height = 480,
	},

	/* Polarity */
	.inv_pclk = 0,
	.inv_vsync = 1,
	.inv_href = 0,
	.inv_hsync = 0,

	.initialized = 0,
	.cam_power = s5k4ecgx_power_en,
};


/* External camera module setting */
static DEFINE_MUTEX(s5ka3dfx_lock);
static struct regulator *s5ka3dfx_vga_avdd;
static struct regulator *s5ka3dfx_vga_vddio;
static struct regulator *s5ka3dfx_cam_isp_host;
static struct regulator *s5ka3dfx_vga_dvdd;
static bool s5ka3dfx_powered_on;

static int s5ka3dfx_request_gpio(void)
{
	int err;

	/* CAM_VGA_nSTBY - GPB(0) */
	err = gpio_request(GPIO_CAM_VGA_nSTBY, "GPB0");
	if (err) {
		pr_err("Failed to request GPB0 for camera control\n");
		return -EINVAL;
	}

	/* CAM_VGA_nRST - GPB(2) */
	err = gpio_request(GPIO_CAM_VGA_nRST, "GPB2");
	if (err) {
		pr_err("Failed to request GPB2 for camera control\n");
		gpio_free(GPIO_CAM_VGA_nSTBY);
		return -EINVAL;
	}

	return 0;
}

static int s5ka3dfx_power_init(void)
{
	if (IS_ERR_OR_NULL(s5ka3dfx_vga_avdd))
		s5ka3dfx_vga_avdd = regulator_get(NULL, "vga_avdd");

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_avdd)) {
		pr_err("Failed to get regulator vga_avdd\n");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_vddio))
		s5ka3dfx_vga_vddio = regulator_get(NULL, "vga_vddio");

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_vddio)) {
		pr_err("Failed to get regulator vga_vddio\n");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(s5ka3dfx_cam_isp_host))
		s5ka3dfx_cam_isp_host = regulator_get(NULL, "cam_isp_host");

	if (IS_ERR_OR_NULL(s5ka3dfx_cam_isp_host)) {
		pr_err("Failed to get regulator cam_isp_host\n");
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_dvdd))
		s5ka3dfx_vga_dvdd = regulator_get(NULL, "vga_dvdd");

	if (IS_ERR_OR_NULL(s5ka3dfx_vga_dvdd)) {
		pr_err("Failed to get regulator vga_dvdd\n");
		return -EINVAL;
	}

	return 0;
}

static int s5ka3dfx_power_on(void)
{
	int err = 0;
	int result;

	if (s5ka3dfx_power_init()) {
		pr_err("Failed to get all regulator\n");
		return -EINVAL;
	}

	/* Turn VGA_AVDD_2.8V on */
	err = regulator_enable(s5ka3dfx_vga_avdd);
	if (err) {
		pr_err("Failed to enable regulator vga_avdd\n");
		return -EINVAL;
	}
	msleep(3);

	/* Turn VGA_VDDIO_2.8V on */
	err = regulator_enable(s5ka3dfx_vga_vddio);
	if (err) {
		pr_err("Failed to enable regulator vga_vddio\n");
		goto off_vga_vddio;
	}
	udelay(20);

	/* Turn VGA_DVDD_1.8V on */
	err = regulator_enable(s5ka3dfx_vga_dvdd);
	if (err) {
		pr_err("Failed to enable regulator vga_dvdd\n");
		goto off_vga_dvdd;
	}
	udelay(100);

	/* CAM_VGA_nSTBY HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 0);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 1);

	udelay(10);

	/* Mclk enable */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, S3C_GPIO_SFN(0x02));
	udelay(430);

	/* Turn CAM_ISP_HOST_2.8V on */
	err = regulator_enable(s5ka3dfx_cam_isp_host);
	if (err) {
		pr_err("Failed to enable regulator cam_isp_host\n");
		goto off_cam_isp_host;
	}
	udelay(150);

	/* CAM_VGA_nRST HIGH */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 0);
	gpio_set_value(GPIO_CAM_VGA_nRST, 1);
	mdelay(5);

	return 0;
off_cam_isp_host:
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);
	udelay(1);
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);
	udelay(1);
	err = regulator_disable(s5ka3dfx_vga_dvdd);
	if (err) {
		pr_err("Failed to disable regulator vga_dvdd\n");
		result = err;
	}
off_vga_dvdd:
	err = regulator_disable(s5ka3dfx_vga_vddio);
	if (err) {
		pr_err("Failed to disable regulator vga_vddio\n");
		result = err;
	}
off_vga_vddio:
	err = regulator_disable(s5ka3dfx_vga_avdd);
	if (err) {
		pr_err("Failed to disable regulator vga_avdd\n");
		result = err;
	}

	return result;
}

static int s5ka3dfx_power_off(void)
{
	int err;

	if (!s5ka3dfx_vga_avdd || !s5ka3dfx_vga_vddio ||
		!s5ka3dfx_cam_isp_host || !s5ka3dfx_vga_dvdd) {
		pr_err("Faild to get all regulator\n");
		return -EINVAL;
	}

	/* Turn CAM_ISP_HOST_2.8V off */
	err = regulator_disable(s5ka3dfx_cam_isp_host);
	if (err) {
		pr_err("Failed to disable regulator cam_isp_host\n");
		return -EINVAL;
	}

	/* CAM_VGA_nRST LOW */
	gpio_direction_output(GPIO_CAM_VGA_nRST, 1);
	gpio_set_value(GPIO_CAM_VGA_nRST, 0);
	udelay(430);

	/* Mclk disable */
	s3c_gpio_cfgpin(GPIO_CAM_MCLK, 0);

	udelay(1);

	/* Turn VGA_VDDIO_2.8V off */
	err = regulator_disable(s5ka3dfx_vga_vddio);
	if (err) {
		pr_err("Failed to disable regulator vga_vddio\n");
		return -EINVAL;
	}

	/* Turn VGA_DVDD_1.8V off */
	err = regulator_disable(s5ka3dfx_vga_dvdd);
	if (err) {
		pr_err("Failed to disable regulator vga_dvdd\n");
		return -EINVAL;
	}

	/* CAM_VGA_nSTBY LOW */
	gpio_direction_output(GPIO_CAM_VGA_nSTBY, 1);
	gpio_set_value(GPIO_CAM_VGA_nSTBY, 0);

	udelay(1);

	/* Turn VGA_AVDD_2.8V off */
	err = regulator_disable(s5ka3dfx_vga_avdd);
	if (err) {
		pr_err("Failed to disable regulator vga_avdd\n");
		return -EINVAL;
	}

	return err;
}

static int s5ka3dfx_power_en(int onoff)
{
	int err = 0;
	mutex_lock(&s5ka3dfx_lock);
	/* we can be asked to turn off even if we never were turned
	 * on if something odd happens and we are closed
	 * by camera framework before we even completely opened.
	 */
	if (onoff != s5ka3dfx_powered_on) {
		if (onoff)
			err = s5ka3dfx_power_on();
		else {
			err = s5ka3dfx_power_off();
			s3c_i2c0_force_stop();
		}
		if (!err)
			s5ka3dfx_powered_on = onoff;
	}
	mutex_unlock(&s5ka3dfx_lock);

	return err;
}

static struct s5ka3dfx_platform_data s5ka3dfx_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,

	.cam_power = s5ka3dfx_power_en,
};

static struct i2c_board_info s5ka3dfx_i2c_info = {
	I2C_BOARD_INFO("S5KA3DFX", 0xc4>>1),
	.platform_data = &s5ka3dfx_plat,
};

static struct s3c_platform_camera s5ka3dfx = {
	.id		= CAMERA_PAR_A,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &s5ka3dfx_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_name	= "sclk_cam",
	.clk_rate	= 24000000,
	.line_length	= 480,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
	.cam_power	= s5ka3dfx_power_en,
};

/* Interface setting */
static struct s3c_platform_fimc fimc_plat_lsi = {
	.srclk_name	= "mout_mpll",
	.clk_name	= "sclk_fimc",
	.lclk_name	= "sclk_fimc_lclk",
	.clk_rate	= 166750000,
	.default_cam	= CAMERA_PAR_A,
	.camera		= {
		&s5k4ecgx,
		&s5ka3dfx,
	},
	.hw_ver		= 0x43,
};

#ifdef CONFIG_VIDEO_JPEG_V2
static struct s3c_platform_jpeg jpeg_plat __initdata = {
	.max_main_width	= 800,
	.max_main_height	= 480,
	.max_thumb_width	= 320,
	.max_thumb_height	= 240,
};
#endif

static void fsa9480_usb_cb(bool attached)
{
	struct usb_gadget *gadget = platform_get_drvdata(&s3c_device_usbgadget);

	if (gadget) {
		if (attached)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}

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

static struct switch_dev switch_dock = {
	.name = "dock",
};

static void fsa9480_deskdock_cb(bool attached)
{
	if (attached)
		switch_set_state(&switch_dock, 1);
	else
		switch_set_state(&switch_dock, 0);
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

static struct fsa9480_platform_data fsa9480_pdata = {
	.usb_cb = fsa9480_usb_cb,
	.charger_cb = fsa9480_charger_cb,
	.deskdock_cb = fsa9480_deskdock_cb,
	.cardock_cb = fsa9480_cardock_cb,
	.reset_cb = fsa9480_reset_cb,
};


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
static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("fsa9480", 0x4A >> 1),
		.platform_data = &fsa9480_pdata,
		.irq = IRQ_EINT(23),
	},{
		I2C_BOARD_INFO("bma023", 0x38),
	},{

		I2C_BOARD_INFO("yas529", 0x2e),
	},
};

/* I2C2 */
static struct i2c_board_info i2c_devs2[] __initdata = {
	{
		I2C_BOARD_INFO("qt602240_ts", 0x4a),
		.platform_data  = &qt602240_data,
	},
};


static struct i2c_board_info i2c_devs8[] __initdata = {
};
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

static int max17040_power_supply_register(struct device *parent,
	struct power_supply *psy)
{
	aries_charger.psy_fuelgauge = psy;
	return 0;
}

static void max17040_power_supply_unregister(struct power_supply *psy)
{
	aries_charger.psy_fuelgauge = NULL;
}

static struct max17040_platform_data max17040_pdata = {
	.power_supply_register = max17040_power_supply_register,
	.power_supply_unregister = max17040_power_supply_unregister,
	.rcomp_value = 0xD700,
};

static struct i2c_board_info i2c_devs9[] __initdata = {
	{
		I2C_BOARD_INFO("max17040", (0x6D >> 1)),
		.platform_data = &max17040_pdata,
	},
};

static void gp2a_gpio_init(void)
{
	int ret = gpio_request(GPIO_PS_ON, "gp2a_power_supply_on");
	if (ret)
		printk(KERN_ERR "Failed to request gpio gp2a power supply.\n");
}

static int gp2a_power(bool on)
{
	/* this controls the power supply rail to the gp2a IC */
	gpio_direction_output(GPIO_PS_ON, on);
	return 0;
}

static int gp2a_light_adc_value(void)
{
	return s3c_adc_get_adc_data(9);
}


static struct gp2a_platform_data gp2a_pdata = {
	.power = gp2a_power,
	.p_out = GPIO_PS_VOUT,
	.light_adc_value = gp2a_light_adc_value
};

static struct i2c_board_info i2c_devs11[] __initdata = {
	{
		I2C_BOARD_INFO("gp2a", (0x88 >> 1)),
		.platform_data = &gp2a_pdata,
	},
};


static struct k3g_platform_data k3g_pdata = {
        .axis_map_x = 1,
        .axis_map_y = 1,
        .axis_map_z = 1,
        .negate_x = 0,
        .negate_y = 0,
        .negate_z = 0,
};

static struct i2c_board_info i2c_devs13[] __initdata = {
        {
                I2C_BOARD_INFO("k3g", 0x69),
                .platform_data = &k3g_pdata,
                .irq = -1,
        },
};

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

static struct platform_device sec_device_rfkill = {
	.name	= "bt_rfkill",
	.id	= -1,
};

static struct platform_device sec_device_btsleep = {
	.name	= "bt_sleep",
	.id	= -1,
};

static struct sec_jack_zone sec_jack_zones[] = {
	{
		/* adc == 0, unstable zone, default to 3pole if it stays
		 * in this range for a half second (20ms delays, 25 samples)
		 */
		.adc_high = 0,
		.delay_ms = 20,
		.check_count = 25,
		.jack_type = SEC_HEADSET_3_POLE_DEVICE,
	},
	{
		/* 0 < adc <= 1000, unstable zone, default to 3pole if it stays
		 * in this range for a second (10ms delays, 100 samples)
		 */
		.adc_high = 1000,
		.delay_ms = 10,
		.check_count = 100,
		.jack_type = SEC_HEADSET_3_POLE_DEVICE,
	},
	{
		/* 1000 < adc <= 2000, unstable zone, default to 4pole if it
		 * stays in this range for a second (10ms delays, 100 samples)
		 */
		.adc_high = 2000,
		.delay_ms = 10,
		.check_count = 100,
		.jack_type = SEC_HEADSET_4_POLE_DEVICE,
	},
	{
		/* 2000 < adc <= 3700, 4 pole zone, default to 4pole if it
		 * stays in this range for 200ms (20ms delays, 10 samples)
		 */
		.adc_high = 3700,
		.delay_ms = 20,
		.check_count = 10,
		.jack_type = SEC_HEADSET_4_POLE_DEVICE,
	},
	{
		/* adc > 3700, unstable zone, default to 3pole if it stays
		 * in this range for a second (10ms delays, 100 samples)
		 */
		.adc_high = 0x7fffffff,
		.delay_ms = 10,
		.check_count = 100,
		.jack_type = SEC_HEADSET_3_POLE_DEVICE,
	},
};

static int sec_jack_get_adc_value(void)
{

	printk(" ****************** LNT DEBUG ***************** sec_jack_get_adc_value : %d \n",s3c_adc_get_adc_data(3));

	return s3c_adc_get_adc_data(3);
}

static void set_popup_sw_enable(bool val)
{
	if (val){
		gpio_set_value(GPIO_POPUP_SW_EN, 1); //suik_Fix
		s3c_gpio_slp_cfgpin(GPIO_POPUP_SW_EN, S3C_GPIO_SLP_OUT1);
	}else{
		gpio_set_value(GPIO_POPUP_SW_EN, 0); //suik_Fix
		s3c_gpio_slp_cfgpin(GPIO_POPUP_SW_EN, S3C_GPIO_SLP_OUT0);
	}
}

struct sec_jack_platform_data sec_jack_pdata = {
	.set_micbias_state = sec_jack_set_micbias_state,
	.set_popup_sw_state = set_popup_sw_enable,
	.get_adc_value = sec_jack_get_adc_value,
	.zones = sec_jack_zones,
	.num_zones = ARRAY_SIZE(sec_jack_zones),
	.det_gpio = GPIO_DET_35,
	.short_send_end_gpio = GPIO_EAR_SEND_END_SHORT,
	.open_send_end_gpio = GPIO_3P_SEND_END,
	.open_send_end_eintr = IRQ_EINT2,
	.short_send_end_eintr = IRQ_EINT(30),
	.det_active_high = 1,
        .det_active_low = 0,
};

static struct platform_device sec_device_jack = {
	.name			= "sec_jack",
	.id			= 1, /* will be used also for gpio_event id */
	.dev.platform_data	= &sec_jack_pdata,
};


#define S3C_GPIO_SETPIN_ZERO         0
#define S3C_GPIO_SETPIN_ONE          1
#define S3C_GPIO_SETPIN_NONE	     2

#define S5PV210_PS_HOLD_CONTROL_REG (S3C_VA_SYS+0xE81C)
static void aries_power_off(void)
{
	int i = 0;
	int err;
	char reset_mode = 'r';
	int phone_wait_cnt = 0;
	int mode=REBOOT_MODE_CHARGING;
	
	/* Change this API call just before power-off to take the dump. */
	/* kernel_sec_clear_upload_magic_number(); */

	err = gpio_request(GPIO_PHONE_ACTIVE, "GPIO_PHONE_ACTIVE");
	/* will never be freed */
	WARN(err, "failed to request GPIO_PHONE_ACTIVE");

	gpio_direction_input(GPIO_nPOWER);
	gpio_direction_input(GPIO_PHONE_ACTIVE);

	/* prevent phone reset when AP off */
	gpio_set_value(GPIO_CAM_FLASH_EN_SET, 0);

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

       for (i=1; i<=6; i++) { //from MAX8893_LDO1(1) to MAX8893_BUCK(6)
	      if(max8893_ldo_is_enabled_direct(i)){
        	  max8893_ldo_disable_direct(i);
        	}
        }

	while (1) {
		/* Check reboot charging */
		if (set_cable_status) {
			/* watchdog reset */
			pr_info("%s: charger connected, rebooting\n", __func__);
			mode=REBOOT_MODE_CHARGING;
			if(sec_set_param_value){
				sec_set_param_value(__REBOOT_MODE,&mode);
			}
			arch_reset('r', NULL);
			kernel_sec_clear_upload_magic_number();
			kernel_sec_hw_reset(1);
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

static void config_gpio_table(int array_size, unsigned int (*gpio_table)[5])
{
        u32 i, gpio;

        for (i = 0; i < array_size; i++) {
                gpio = gpio_table[i][0];
                s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
                if (gpio_table[i][2] != S3C_GPIO_SETPIN_NONE)
                        gpio_set_value(gpio, gpio_table[i][2]);
                s3c_gpio_setpull(gpio, gpio_table[i][3]);
                s3c_gpio_set_drvstrength(gpio, initial_gpio_table[i][4]);
        }
}

static void config_alive_gpio_table(int array_size, unsigned int (*gpio_table)[4])
{
        u32 i, gpio;

        for (i = 0; i < array_size; i++) {
                gpio = gpio_table[i][0];
                s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
                if (gpio_table[i][2] != S3C_GPIO_SETPIN_NONE)
                        gpio_set_value(gpio, gpio_table[i][2]);
                s3c_gpio_setpull(gpio, gpio_table[i][3]);
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
        config_alive_gpio_table(ARRAY_SIZE(sleep_alive_gpio_table), sleep_alive_gpio_table);
        config_sleep_gpio_table(ARRAY_SIZE(sleep_gpio_table), sleep_gpio_table);
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

		s3c_gpio_cfgpin(GPIO_WLAN_WAKE,
				S3C_GPIO_SFN(GPIO_WLAN_WAKE_AF));
		s3c_gpio_setpull(GPIO_WLAN_WAKE, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_WLAN_WAKE, GPIO_LEVEL_LOW);

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
		.start	= IRQ_EINT(3),  //ijihyun.jung - SEC Crespo
		.end	= IRQ_EINT(3),
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

int __init aries_init_wifi_mem(void)
{
	int i;
	int j;

	for (i = 0 ; i < WLAN_SKB_BUF_NUM ; i++) {
		wlan_static_skb[i] = dev_alloc_skb(
				((i < (WLAN_SKB_BUF_NUM / 2)) ? 4096 : 8192));

		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

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

static struct platform_device *aries_devices[] __initdata = {
	&watchdog_device,
#ifdef CONFIG_FIQ_DEBUGGER
	&s5pv210_device_fiqdbg_uart2,
#endif
	&s5pc110_device_onenand,
#ifdef CONFIG_RTC_DRV_S3C
	&s5p_device_rtc,
#endif
	&aries_input_device,

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
	&sec_device_dpram,
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
	/* Platform device for LCD spi bit bang */
	&s3c_device_spi_gpio,
	&sec_device_jack,
	&s3c_device_vibrator,

	&s3c_device_i2c0,
#if defined(CONFIG_S3C_DEV_I2C1)
	&s3c_device_i2c1,
#endif

#if defined(CONFIG_S3C_DEV_I2C2)
	&s3c_device_i2c2,
#endif
	&s3c_device_i2c4,
	&s3c_device_i2c5,
	&s3c_device_i2c6,
	&s3c_device_i2c9,  /* max1704x:fuel_guage */
	&s3c_device_i2c11, /* optical sensor */
	&s3c_device_i2c12, /* MAX8893 PMIC */
	&s3c_device_i2c13, /* k3g gyro sensor */

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

	&sec_device_battery,

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
};

unsigned int HWREV;
EXPORT_SYMBOL(HWREV);

static void __init aries_map_io(void)
{
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s5pv210_gpiolib_init();
	s3c24xx_init_uarts(aries_uartcfgs, ARRAY_SIZE(aries_uartcfgs));
	s5p_reserve_bootmem(crespo_media_devs, ARRAY_SIZE(crespo_media_devs));
#ifdef CONFIG_MTD_ONENAND
	s5pc110_device_onenand.name = "s5pc110-onenand";
#endif
}

unsigned int pm_debug_scratchpad;

static unsigned int ram_console_start;
static unsigned int ram_console_size;

static void __init aries_fixup(struct machine_desc *desc,
		struct tag *tags, char **cmdline,
		struct meminfo *mi)
{
	mi->bank[0].start = 0x30000000;
	mi->bank[0].size = 80 * SZ_1M;
	mi->bank[0].node = 0;

#ifdef CONFIG_DDR_RAM_3G
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
#else
       	mi->bank[1].start = 0x40000000;
	mi->bank[1].size = 255 * SZ_1M;
	mi->bank[1].node = 1;

       	mi->nr_banks = 2;
       	ram_console_start = mi->bank[1].start + mi->bank[1].size;
	ram_console_size = SZ_1M - SZ_4K;
#endif
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
	s3c_gpio_cfgpin(GPIO_UART_SEL, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_UART_SEL, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_JACK_nINT, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(GPIO_JACK_nINT, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_USB_HS_SEL, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_USB_HS_SEL, 1/*level= high*/);

	s3c_gpio_cfgpin(GPIO_USB_HS_SW_EN_N, S3C_GPIO_OUTPUT);
	gpio_set_value(GPIO_USB_HS_SW_EN_N, 0/*level= low*/);
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

static void k3g_irq_init(void)
{
        i2c_devs0[0].irq = (system_rev >= 0x0A) ? IRQ_EINT(29) : -1;
}


static struct i2c_board_info i2c_devs1[] __initdata = { };

static void __init aries_machine_init(void)
{
	setup_ram_console_mem();
	s3c_usb_set_serial();
	platform_add_devices(aries_devices, ARRAY_SIZE(aries_devices));

	/* Find out S5PC110 chip version */
	_hw_version_check();

	pm_power_off = aries_power_off ;

	/*initialise the gpio's*/
        config_init_gpio();
	qt_touch_init();
    touch_keypad_gpio_init();
#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif

	/* headset/earjack detection */
	if (system_rev >= 0x09)
		gpio_request(GPIO_EAR_MICBIAS_EN, "ear_micbias_enable");

	/* i2c */
	s3c_i2c0_set_platdata(NULL);
#ifdef CONFIG_S3C_DEV_I2C1
	s3c_i2c1_set_platdata(NULL);
#endif

#ifdef CONFIG_S3C_DEV_I2C2
	s3c_i2c2_set_platdata(NULL);
#endif
	/* camera */
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));
	fsa9480_gpio_init();

	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));
	/* Touch Screen qt602240 */
	i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));

	/* wm8994 codec */
	sound_init();
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
        fsa9480_gpio_init();
        i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));
	/* Touch Key */
	/* FSA9480 */

	i2c_register_board_info(9, i2c_devs9, ARRAY_SIZE(i2c_devs9));
	/* optical sensor */
	gp2a_gpio_init();
	i2c_register_board_info(11, i2c_devs11, ARRAY_SIZE(i2c_devs11));

	/* MAX8893 PMIC */
	i2c_register_board_info(12, i2c_devs12, ARRAY_SIZE(i2c_devs12));

	/* K3g Gyro sensor */
	k3g_irq_init();
	i2c_register_board_info(13, i2c_devs13, ARRAY_SIZE(i2c_devs13));

#ifdef CONFIG_FB_S3C_TL2796
	spi_register_board_info(spi_board_info, ARRAY_SIZE(spi_board_info));
	s3cfb_set_platdata(&tl2796_data);
#elif CONFIG_FB_S3C_NT35580
	spi_register_board_info(spi_board_info_tft, ARRAY_SIZE(spi_board_info_tft));
	s3cfb_set_platdata(&nt35580_data);
#endif

#if defined(CONFIG_S5P_ADC)
	s3c_adc_set_platdata(&s3c_adc_platform);
#endif

#if defined(CONFIG_PM)
	s3c_pm_init();
#endif

	s5ka3dfx_request_gpio();

	s5k4ecgx_init();

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

//	regulator_has_full_constraints();

	register_reboot_notifier(&aries_reboot_notifier);

	aries_switch_init();

	gps_gpio_init();

	uart_switch_init();

	aries_init_wifi_mem();

	onenand_init();
	#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
	/* soonyong.cho : This is for setting unique serial number */
        s3c_usb_set_serial();
        #endif


	if (gpio_is_valid(GPIO_MSENSE_nRST)) {
		if (gpio_request(GPIO_MSENSE_nRST, "GPD0"))
			printk(KERN_ERR "Failed to request GPIO_MSENSE_nRST!\n");
		gpio_direction_output(GPIO_MSENSE_nRST, 1);
	}
	gpio_free(GPIO_MSENSE_nRST);
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
		s3c_gpio_slp_cfgpin(GPIO_BT_RXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_TXD, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_TXD, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_CTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_CTS, S3C_GPIO_PULL_NONE);
		s3c_gpio_slp_cfgpin(GPIO_BT_RTS, S3C_GPIO_SLP_PREV);
		s3c_gpio_slp_setpull_updown(GPIO_BT_RTS, S3C_GPIO_PULL_NONE);
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


MACHINE_START(CRESPO, "Crespo")
	.phys_io	= S3C_PA_UART & 0xfff00000,
	.io_pg_offst	= (((u32)S3C_VA_UART) >> 18) & 0xfffc,
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.fixup		= aries_fixup,
	.init_irq	= s5pv210_init_irq,
	.map_io		= aries_map_io,
	.init_machine	= aries_machine_init,
	.timer		= &s5p_systimer,
MACHINE_END

