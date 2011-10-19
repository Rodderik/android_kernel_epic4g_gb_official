/*
 * wm8994_crespo.c  --  WM8994 ALSA Soc Audio driver related Aries
 *
 *  Copyright (C) 2010 Samsung Electronics.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <plat/gpio-cfg.h>
#include <plat/map-base.h>
#include <mach/regs-clock.h>
#include "wm8994_samsung.h"
#if defined(CONFIG_MACH_FORTE)
#include <mach/gpio-forte.h>
#else
#include <mach/gpio-atlas.h>
#endif
/*
 * Debug Feature
 */
#define SUBJECT "wm8994_crespo.c"

#ifdef FEATURE_SS_AUDIO_CAL
#define STEREO_SPEAKER_SUPPORT

extern unsigned int tty_mode;
extern unsigned int loopback_mode;
extern unsigned int system_rev;
extern void FSA9480_Enable_SPK(u8 enable);
extern unsigned int get_headset_status(void);
extern void set_recording_status(int value);
#endif

/*
 * Definitions of tunning volumes for wm8994
 */
struct gain_info_t playback_gain_table[PLAYBACK_GAIN_NUM] = {
	{ /* COMMON */
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC1_LEFT_VOLUME,	/* 610h */
		.mask = WM8994_DAC1L_VOL_MASK,
		.gain = WM8994_DAC1_VU | 0xC0
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC1_RIGHT_VOLUME,	/* 611h */
		.mask = WM8994_DAC1R_VOL_MASK,
		.gain = WM8994_DAC1_VU | 0xC0
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_AIF1_DAC1_LEFT_VOLUME,	/* 402h */
		.mask = WM8994_AIF1DAC1L_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xC0
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_AIF1_DAC1_RIGHT_VOLUME,	/* 403h */
		.mask = WM8994_AIF1DAC1R_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xC0
	}, { /* RCV */
		.mode = PLAYBACK_RCV,
		.reg  = WM8994_OUTPUT_MIXER_5,		/* 31h */
		.mask = WM8994_DACL_MIXOUTL_VOL_MASK,
		.gain = 0x0 << WM8994_DACL_MIXOUTL_VOL_SHIFT
	}, {
		.mode = PLAYBACK_RCV,
		.reg  = WM8994_OUTPUT_MIXER_6,		/* 32h */
		.mask = WM8994_DACR_MIXOUTR_VOL_MASK,
		.gain = 0x0 << WM8994_DACR_MIXOUTR_VOL_SHIFT
	}, {
		.mode = PLAYBACK_RCV,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x3D
	}, {
		.mode = PLAYBACK_RCV,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x3D
	}, {
		.mode = PLAYBACK_RCV,
		.reg  = WM8994_HPOUT2_VOLUME,		/* 1Fh */
		.mask = WM8994_HPOUT2_VOL_MASK,
		.gain = 0x0 << WM8994_HPOUT2_VOL_SHIFT
	}, { /* SPK */
		.mode = PLAYBACK_SPK,
		.reg  = WM8994_SPKMIXL_ATTENUATION,	/* 22h */
		.mask = WM8994_SPKMIXL_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = PLAYBACK_SPK,
		.reg  = WM8994_SPKMIXR_ATTENUATION,	/* 23h */
		.mask = WM8994_SPKMIXR_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = PLAYBACK_SPK,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E     /* +5dB */
	}, {
		.mode = PLAYBACK_SPK,
		.reg  = WM8994_SPEAKER_VOLUME_RIGHT,	/* 27h */
		.mask = WM8994_SPKOUTR_VOL_MASK,
		.gain = 0
	}, {
		.mode = PLAYBACK_SPK,
		.reg  = WM8994_CLASSD,			/* 25h */
		.mask = WM8994_SPKOUTL_BOOST_MASK,
		.gain = 0x05 << WM8994_SPKOUTL_BOOST_SHIFT  /* +7.5dB */
	}, {
		.mode = PLAYBACK_SPK,
		.reg  = WM8994_AIF1_DAC1_LEFT_VOLUME,	/* 402h */
		.mask = WM8994_AIF1DAC1L_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xB8           /* -2.625dB */
	}, {
		.mode = PLAYBACK_SPK,
		.reg  = WM8994_AIF1_DAC1_RIGHT_VOLUME,	/* 403h */
		.mask = WM8994_AIF1DAC1R_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xB8           /* -2.625dB */
	}, { /* HP */
		.mode = PLAYBACK_HP,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x31 /* -8dB */
	}, {
		.mode = PLAYBACK_HP,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x31 /* -8dB */
	}, {
		.mode = PLAYBACK_HP,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = PLAYBACK_HP,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, { /* SPK_HP */
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_SPKMIXL_ATTENUATION,	/* 22h */
		.mask = WM8994_SPKMIXL_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_SPKMIXR_ATTENUATION,	/* 23h */
		.mask = WM8994_SPKMIXR_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_SPEAKER_VOLUME_RIGHT,	/* 27h */
		.mask = WM8994_SPKOUTR_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_CLASSD,			/* 25h */
		.mask = WM8994_SPKOUTL_BOOST_MASK,
		.gain = 0x5 << WM8994_SPKOUTL_BOOST_SHIFT
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x1E
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x1E
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = PLAYBACK_SPK_HP,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, { /* RING_SPK */
		.mode = PLAYBACK_RING_SPK,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E
	}, {
		.mode = PLAYBACK_RING_SPK,
		.reg  = WM8994_CLASSD,			/* 25h */
		.mask = WM8994_SPKOUTL_BOOST_MASK,
		.gain = 0x5 << WM8994_SPKOUTL_BOOST_SHIFT
	}, { /* RING_HP */
		.mode = PLAYBACK_RING_HP,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x34
	}, {
		.mode = PLAYBACK_RING_HP,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x34
	}, {
		.mode = PLAYBACK_RING_HP,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = PLAYBACK_RING_HP,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, { /* RING_SPK_HP */
		.mode = PLAYBACK_RING_SPK_HP,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E
	}, {
		.mode = PLAYBACK_RING_SPK_HP,
		.reg  = WM8994_CLASSD,			/* 25h */
		.mask = WM8994_SPKOUTL_BOOST_MASK,
		.gain = 0x5 << WM8994_SPKOUTL_BOOST_SHIFT
	}, {
		.mode = PLAYBACK_RING_SPK_HP,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x1E
	}, {
		.mode = PLAYBACK_RING_SPK_HP,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x1E
	}, { /* HP_NO_MIC */
		.mode = PLAYBACK_HP_NO_MIC,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,  /* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x36   /* -3dB */
	}, {
		.mode = PLAYBACK_HP_NO_MIC,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME, /* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x36   /* -3dB */
	}, {
		.mode = PLAYBACK_HP_NO_MIC,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = PLAYBACK_HP_NO_MIC,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,   /* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
    },
};

struct gain_info_t voicecall_gain_table[VOICECALL_GAIN_NUM] = {
	{ /* COMMON */
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC1_LEFT_VOLUME,	/* 610h */
		.mask = WM8994_DAC1L_VOL_MASK,
		.gain = WM8994_DAC1_VU | 0xC0     /* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC1_RIGHT_VOLUME,	/* 611h */
		.mask = WM8994_DAC1R_VOL_MASK,
		.gain = WM8994_DAC1_VU | 0xC0     /* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_AIF1_DAC1_LEFT_VOLUME,	/* 402h */
		.mask = WM8994_AIF1DAC1L_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xC0   /* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_AIF1_DAC1_RIGHT_VOLUME,	/* 403h */
		.mask = WM8994_AIF1DAC1R_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xC0     /* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC2_LEFT_VOLUME,	/* 612h */
		.mask = WM8994_DAC2L_VOL_MASK,
		.gain = WM8994_DAC2_VU | 0xC0         /* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC2_RIGHT_VOLUME,	/* 613h */
		.mask = WM8994_DAC2R_VOL_MASK,
		.gain = WM8994_DAC2_VU | 0xC0         /* 0dB */
	}, { /* RCV */
		.mode = VOICECALL_RCV,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x15   /* +15dB */
	}, {
		.mode = VOICECALL_RCV,
		.reg  = WM8994_INPUT_MIXER_3,		/* 29h */
		.mask = WM8994_IN1L_MIXINL_VOL_MASK | WM8994_MIXOUTL_MIXINL_VOL_MASK,
		.gain = 0x10                    /* +30dB */
	}, {
		.mode = VOICECALL_RCV,
		.reg  = WM8994_OUTPUT_MIXER_5,		/* 31h */
		.mask = WM8994_DACL_MIXOUTL_VOL_MASK,
		.gain = 0x0 << WM8994_DACL_MIXOUTL_VOL_SHIFT
	}, {
		.mode = VOICECALL_RCV,
		.reg  = WM8994_OUTPUT_MIXER_6,		/* 32h */
		.mask = WM8994_DACR_MIXOUTR_VOL_MASK,
		.gain = 0x0 << WM8994_DACR_MIXOUTR_VOL_SHIFT
	}, {
		.mode = VOICECALL_RCV,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x3F
	}, {
		.mode = VOICECALL_RCV,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x3F
	}, {
		.mode = VOICECALL_RCV,
		.reg  = WM8994_HPOUT2_VOLUME,		/* 1Fh */
		.mask = WM8994_HPOUT2_VOL_MASK,
		.gain = 0x0 << WM8994_HPOUT2_VOL_SHIFT
	}, { /* SPK */
		.mode = VOICECALL_SPK,
		.reg  = WM8994_INPUT_MIXER_3,		/* 29h */
		.mask = WM8994_IN1L_MIXINL_VOL_MASK | WM8994_MIXOUTL_MIXINL_VOL_MASK,
		.gain = 0x10     /* Mic +7.5dB */
	}, {
		.mode = VOICECALL_SPK,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x12   /* Mic +30dB */
	}, {
		.mode = VOICECALL_SPK,
		.reg  = WM8994_SPKMIXL_ATTENUATION,	/* 22h */
		.mask = WM8994_SPKMIXL_VOL_MASK,
		.gain = 0x0     /* Speaker +0dB */
	}, {
		.mode = VOICECALL_SPK,
		.reg  = WM8994_SPKMIXR_ATTENUATION,	/* 23h */
		.mask = WM8994_SPKMIXR_VOL_MASK,
		.gain = 0x0     /* Speaker +0dB */
	}, {
		.mode = VOICECALL_SPK,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3C    /* Left Speaker +3dB */
	}, {
		.mode = VOICECALL_SPK,
		.reg  = WM8994_SPEAKER_VOLUME_RIGHT,	/* 27h */
		.mask = WM8994_SPKOUTR_VOL_MASK,   /* Right Speaker -57dB */
		.gain = 0x0
	}, {
		.mode = VOICECALL_SPK,
		.reg  = WM8994_CLASSD,			/* 25h */
		.mask = WM8994_SPKOUTL_BOOST_MASK,
		.gain = 0x7 << WM8994_SPKOUTL_BOOST_SHIFT /* Left spaker +12dB */
	}, { /* HP */
		.mode = VOICECALL_HP,
		.reg  = WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,	/* 1Ah */
		.mask = WM8994_IN1R_VOL_MASK,
		.gain = WM8994_IN1R_VU | 0x1D
	}, {
		.mode = VOICECALL_HP,
		.reg  = WM8994_INPUT_MIXER_4,		/* 2Ah */
		.mask = WM8994_IN1R_MIXINR_VOL_MASK | WM8994_MIXOUTR_MIXINR_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = VOICECALL_HP,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = VOICECALL_HP,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = VOICECALL_HP,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x30
	}, {
		.mode = VOICECALL_HP,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x30
	}, { /* HP_NO_MIC */
		.mode = VOICECALL_HP_NO_MIC,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x12	/* +10.5dB */
	}, {
		.mode = VOICECALL_HP_NO_MIC,
		.reg  = WM8994_INPUT_MIXER_3,		/* 29h */
		.mask = WM8994_IN1L_MIXINL_VOL_MASK | WM8994_MIXOUTL_MIXINL_VOL_MASK,
		.gain = 0x10
	}, {
		.mode = VOICECALL_HP_NO_MIC,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = VOICECALL_HP_NO_MIC,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = VOICECALL_HP_NO_MIC,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x30
	}, {
		.mode = VOICECALL_HP_NO_MIC,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME, /* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x30
	}

};

struct gain_info_t recording_gain_table[RECORDING_GAIN_NUM] = {
	{ /* MAIN */
		.mode = RECORDING_MAIN,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x12    /* +10.5dB */
	}, {
		.mode = RECORDING_MAIN,
		.reg  = WM8994_INPUT_MIXER_3,		/* 29h */
		.mask = WM8994_IN1L_MIXINL_VOL_MASK | WM8994_MIXOUTL_MIXINL_VOL_MASK,
		.gain = 0x10                /* +30dB */
	}, {
		.mode = RECORDING_MAIN,
		.reg  = WM8994_AIF1_ADC1_LEFT_VOLUME,	/* 400h */
		.mask = WM8994_AIF1ADC1L_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0       /* 0dB */
	}, {
		.mode = RECORDING_MAIN,
		.reg  = WM8994_AIF1_ADC1_RIGHT_VOLUME,	/* 401h */
		.mask = WM8994_AIF1ADC1R_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0       /* 0dB */
	}, { /* HP */
		.mode = RECORDING_HP,
		.reg  = WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,	/* 1Ah */
		.mask = WM8994_IN1R_VOL_MASK,
		.gain = WM8994_IN1R_VU | 0x15
	}, {
		.mode = RECORDING_HP,
		.reg  = WM8994_INPUT_MIXER_4,		/* 2Ah */
		.mask = WM8994_IN1R_MIXINR_VOL_MASK | WM8994_MIXOUTR_MIXINR_VOL_MASK,
		.gain = 0x10
	}, {
		.mode = RECORDING_HP,
		.reg  = WM8994_AIF1_ADC1_LEFT_VOLUME,	/* 400h */
		.mask = WM8994_AIF1ADC1L_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0
	}, {
		.mode = RECORDING_HP,
		.reg  = WM8994_AIF1_ADC1_RIGHT_VOLUME,	/* 401h */
		.mask = WM8994_AIF1ADC1R_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0
	}, { /* RECOGNITION_MAIN */
		.mode = RECORDING_REC_MAIN,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x0D /* +3dB */
	}, {
		.mode = RECORDING_REC_MAIN,
		.reg  = WM8994_INPUT_MIXER_3,		/* 29h */
		.mask = WM8994_IN1L_MIXINL_VOL_MASK | WM8994_MIXOUTL_MIXINL_VOL_MASK,
		.gain = 0x10  /* 30dB */
	}, {
		.mode = RECORDING_REC_MAIN,
		.reg  = WM8994_AIF1_ADC1_LEFT_VOLUME,	/* 400h */
		.mask = WM8994_AIF1ADC1L_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xc0   /* +0dB */
	}, {
		.mode = RECORDING_REC_MAIN,
		.reg  = WM8994_AIF1_ADC1_RIGHT_VOLUME,	/* 401h */
		.mask = WM8994_AIF1ADC1R_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xc0   /* +0dB */
	}, { /* RECOGNITION_HP */
		.mode = RECORDING_REC_HP,
		.reg  = WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,	/* 1Ah */
		.mask = WM8994_IN1R_VOL_MASK,
		.gain = WM8994_IN1R_VU | 0x12    /* +10.5dB */
	}, {
		.mode = RECORDING_REC_HP,
		.reg  = WM8994_INPUT_MIXER_4,		/* 2Ah */
		.mask = WM8994_IN1R_MIXINR_VOL_MASK | WM8994_MIXOUTR_MIXINR_VOL_MASK,
		.gain = 0x10   /* +30dB */
	}, {
		.mode = RECORDING_REC_HP,
		.reg  = WM8994_AIF1_ADC1_LEFT_VOLUME,	/* 400h */
		.mask = WM8994_AIF1ADC1L_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0
	}, {
		.mode = RECORDING_REC_HP,
		.reg  = WM8994_AIF1_ADC1_RIGHT_VOLUME,	/* 401h */
		.mask = WM8994_AIF1ADC1R_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0
	}, { /* CAMCORDER_MAIN */
		.mode = RECORDING_CAM_MAIN,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x17    /* +18dB */
	}, {
		.mode = RECORDING_CAM_MAIN,
		.reg  = WM8994_INPUT_MIXER_3,		/* 29h */
		.mask = WM8994_IN1L_MIXINL_VOL_MASK | WM8994_MIXOUTL_MIXINL_VOL_MASK,
		.gain = 0x10  /* 30dB */
	}, {
		.mode = RECORDING_CAM_MAIN,
		.reg  = WM8994_AIF1_ADC1_LEFT_VOLUME,	/* 400h */
		.mask = WM8994_AIF1ADC1L_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0   /* +0dB */
	}, {
		.mode = RECORDING_CAM_MAIN,
		.reg  = WM8994_AIF1_ADC1_RIGHT_VOLUME,	/* 401h */
		.mask = WM8994_AIF1ADC1R_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0   /* +0dB */
	}, { /* CAMCORDER_HP */
		.mode = RECORDING_CAM_HP,
		.reg  = WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,	/* 1Ah */
		.mask = WM8994_IN1R_VOL_MASK,
		.gain = WM8994_IN1R_VU | 0x15    /* +15dB */
	}, {
		.mode = RECORDING_CAM_HP,
		.reg  = WM8994_INPUT_MIXER_4,		/* 2Ah */
		.mask = WM8994_IN1R_MIXINR_VOL_MASK | WM8994_MIXOUTR_MIXINR_VOL_MASK,
		.gain = 0x10   /* +30dB */
	}, {
		.mode = RECORDING_CAM_HP,
		.reg  = WM8994_AIF1_ADC1_LEFT_VOLUME,	/* 400h */
		.mask = WM8994_AIF1ADC1L_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0
	}, {
		.mode = RECORDING_CAM_HP,
		.reg  = WM8994_AIF1_ADC1_RIGHT_VOLUME,	/* 401h */
		.mask = WM8994_AIF1ADC1R_VOL_MASK,
		.gain = WM8994_AIF1ADC1_VU | 0xC0
	},
};

struct gain_info_t fmradio_gain_table[FMRADIO_GAIN_NUM] = {
	{ /* COMMON */
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC1_LEFT_VOLUME,	/* 610h */
		.mask = WM8994_DAC1L_VOL_MASK,
		.gain = WM8994_DAC1_VU | 0xC0		/* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC1_RIGHT_VOLUME,	/* 611h */
		.mask = WM8994_DAC1R_VOL_MASK,
		.gain = WM8994_DAC1_VU | 0xC0		/* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_AIF1_DAC1_LEFT_VOLUME,	/* 402h */
		.mask = WM8994_AIF1DAC1L_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xC0	/* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_AIF1_DAC1_RIGHT_VOLUME,	/* 403h */
		.mask = WM8994_AIF1DAC1R_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xC0	/* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC2_LEFT_VOLUME,	/* 612h */
		.mask = WM8994_DAC2L_VOL_MASK,
		.gain = WM8994_DAC2_VU | 0xC0		/* 0dB */
	}, {
		.mode = COMMON_SET_BIT,
		.reg  = WM8994_DAC2_RIGHT_VOLUME,	/* 613h */
		.mask = WM8994_DAC2R_VOL_MASK,
		.gain = WM8994_DAC2_VU | 0xC0		/* 0dB */
	}, { /* Headset */
		.mode = FMRADIO_HP,
		.reg  = WM8994_LEFT_LINE_INPUT_3_4_VOLUME,	/* 19h */
		.mask = WM8994_IN2L_VOL_MASK,
		.gain = WM8994_IN2L_VU | 0x0B
	}, {
		.mode = FMRADIO_HP,
		.reg  = WM8994_RIGHT_LINE_INPUT_3_4_VOLUME,	/* 1Bh */
		.mask = WM8994_IN2R_VOL_MASK,
		.gain = WM8994_IN2R_VU | 0x0B
	}, {
		.mode = FMRADIO_HP,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3C
	}, {
		.mode = FMRADIO_HP,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3C
	}, {
		.mode = FMRADIO_HP,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = FMRADIO_HP,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = FMRADIO_HP,
		.reg  = WM8994_AIF1_DAC1_LEFT_VOLUME,	/* 402h */
		.mask = WM8994_AIF1DAC1L_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xA8
	}, {
		.mode = FMRADIO_HP,
		.reg  = WM8994_AIF1_DAC1_RIGHT_VOLUME,	/* 403h */
		.mask = WM8994_AIF1DAC1R_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0xA8
	}, { /* Speaker */
		.mode = FMRADIO_SPK,
		.reg  = WM8994_LEFT_LINE_INPUT_3_4_VOLUME,	/* 19h */
		.mask = WM8994_IN2L_VOL_MASK,
		.gain = WM8994_IN2L_VU | 0x0F
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_RIGHT_LINE_INPUT_3_4_VOLUME,	/* 1Bh */
		.mask = WM8994_IN2R_VOL_MASK,
		.gain = WM8994_IN2R_VU | 0x0F
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_SPKMIXL_ATTENUATION,	/* 22h */
		.mask = WM8994_SPKMIXL_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_SPKMIXR_ATTENUATION,	/* 23h */
		.mask = WM8994_SPKMIXR_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_SPEAKER_VOLUME_RIGHT,	/* 27h */
		.mask = WM8994_SPKOUTR_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_CLASSD,			/* 25h */
		.mask = WM8994_SPKOUTL_BOOST_MASK,
		.gain = 0x6 << WM8994_SPKOUTL_BOOST_SHIFT
	}, { /* SPK_HP */
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_LEFT_LINE_INPUT_3_4_VOLUME,	/* 19h */
		.mask = WM8994_IN2L_VOL_MASK,
		.gain = WM8994_IN2L_VU | 0x0F
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_RIGHT_LINE_INPUT_3_4_VOLUME,	/* 1Bh */
		.mask = WM8994_IN2R_VOL_MASK,
		.gain = WM8994_IN2R_VU | 0x0F
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_SPKMIXL_ATTENUATION,	/* 22h */
		.mask = WM8994_SPKMIXL_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = FMRADIO_SPK,
		.reg  = WM8994_SPKMIXR_ATTENUATION,	/* 23h */
		.mask = WM8994_SPKMIXR_VOL_MASK,
		.gain = 0x0
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_LEFT_OPGA_VOLUME,	/* 20h */
		.mask = WM8994_MIXOUTL_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_RIGHT_OPGA_VOLUME,	/* 21h */
		.mask = WM8994_MIXOUTR_VOL_MASK,
		.gain = WM8994_MIXOUT_VU | 0x39
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3C
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3C
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_AIF1_DAC1_LEFT_VOLUME,	/* 402h */
		.mask = WM8994_AIF1DAC1L_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0x70
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_AIF1_DAC1_RIGHT_VOLUME,	/* 403h */
		.mask = WM8994_AIF1DAC1R_VOL_MASK,
		.gain = WM8994_AIF1DAC1_VU | 0x70
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_CLASSD,			/* 25h */
		.mask = WM8994_SPKOUTL_BOOST_MASK,
		.gain = 0x6 << WM8994_SPKOUTL_BOOST_SHIFT
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E
	}, {
		.mode = FMRADIO_SPK_HP,
		.reg  = WM8994_SPEAKER_VOLUME_RIGHT,	/* 27h */
		.mask = WM8994_SPKOUTR_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3E
	},

};

static void wait_for_dc_servo(struct snd_soc_codec *codec, unsigned int op)
{
        unsigned int reg;
        int count = 0;
        unsigned int val, start;

        val = op | WM8994_DCS_ENA_CHAN_0 | WM8994_DCS_ENA_CHAN_1;

        /* Trigger the command */
        snd_soc_write(codec, WM8994_DC_SERVO_1, val);

	start = jiffies;
        pr_debug("Waiting for DC servo...\n");

        do {
                count++;
                msleep(1);
                reg = snd_soc_read(codec, WM8994_DC_SERVO_1);
                pr_debug("DC servo: %x\n", reg);
        } while (reg & op && count < 400);

	pr_info("DC servo took %dms\n", jiffies_to_msecs(jiffies - start));

        if (reg & op)
                pr_err("Timed out waiting for DC Servo\n");
}

/* S5P_SLEEP_CONFIG must be controlled by codec if codec use XUSBTI */
int wm8994_configure_clock(struct snd_soc_codec *codec, int en)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	if (en) {
		clk_enable(wm8994->codec_clk);
		DEBUG_LOG("USBOSC Enabled in Sleep Mode\n");
	} else {
		clk_disable(wm8994->codec_clk);
		DEBUG_LOG("USBOSC disable in Sleep Mode\n");
	}

	return 0;
}

void audio_ctrl_mic_bias_gpio(struct wm8994_platform_data *pdata, int enable)
{
    unsigned int headset_status;
	
	DEBUG_LOG("enable = [%d], HWREV = %d.", enable, system_rev);

    if(system_rev < 05)
    {
        if(enable)
        {
	        gpio_set_value(GPIO_MICBIAS_EN, 1);
			set_recording_status(1);
        }
		else
		{
		    set_recording_status(0);
			headset_status = get_headset_status();

    		if(headset_status == 0 || headset_status == 0x1<<5)
    		{
    		    gpio_set_value(GPIO_MICBIAS_EN, 0);
    		}
		}
    }
	else
	{
	    if(enable)
        {
	        gpio_set_value(GPIO_EAR_BIAS_EN, 1);
			set_recording_status(1);
        }
		else
		{
		    set_recording_status(0);
			headset_status = get_headset_status();

    		if(headset_status == 0 || headset_status == 0x1<<5)
    		{
    		    gpio_set_value(GPIO_EAR_BIAS_EN, 0);
    		}
		}
	}
}

static int wm8994_earsel_control(struct wm8994_platform_data *pdata, int en)
{

	if (!pdata) {
		pr_err("failed to control wm8994 ear selection\n");
		return -EINVAL;
	}

	gpio_set_value(pdata->ear_sel, en);

	return 0;

}

/* Audio Routing routines for the universal board..wm8994 codec*/
void wm8994_disable_path(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;
	enum audio_path path = wm8994->cur_path;

	DEBUG_LOG("Path = [%d]", path);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);

	switch (path) {
	case RCV:
		/* Disbale the HPOUT2 */
		val &= ~(WM8994_HPOUT2_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

		/* Disable left MIXOUT */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
		val &= ~(WM8994_DAC1L_TO_MIXOUTL_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

		/* Disable right MIXOUT */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
		val &= ~(WM8994_DAC1R_TO_MIXOUTR_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

		/* Disable HPOUT Mixer */
		val = wm8994_read(codec, WM8994_HPOUT2_MIXER);
		val &= ~(WM8994_MIXOUTLVOL_TO_HPOUT2_MASK |
			WM8994_MIXOUTRVOL_TO_HPOUT2_MASK);
		wm8994_write(codec, WM8994_HPOUT2_MIXER, val);

		/* Disable mixout volume control */
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
		val &= ~(WM8994_MIXOUTLVOL_ENA_MASK |
			WM8994_MIXOUTRVOL_ENA_MASK |
			WM8994_MIXOUTL_ENA_MASK |
			WM8994_MIXOUTR_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);
		break;

	case SPK:
		/* Disbale the SPKOUTL */
		val &= ~(WM8994_SPKOUTL_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

		/* Disable SPKLVOL */
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
		val &= ~(WM8994_SPKLVOL_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

		/* Disable SPKOUT mixer */
		val = wm8994_read(codec, WM8994_SPKOUT_MIXERS);
		val &= ~(WM8994_SPKMIXL_TO_SPKOUTL_MASK |
			 WM8994_SPKMIXR_TO_SPKOUTL_MASK |
			 WM8994_SPKMIXR_TO_SPKOUTR_MASK);
		wm8994_write(codec, WM8994_SPKOUT_MIXERS, val);

		/* Mute Speaker mixer */
		val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
		val &= ~(WM8994_DAC1L_TO_SPKMIXL_MASK);
		wm8994_write(codec, WM8994_SPEAKER_MIXER, val);
		break;

	case HP:
	case HP_NO_MIC:
		val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
		val &= ~(0x02C0);
		val |= 0x02C0;
		wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, 0x02C0);

		val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
		val &= ~(0x02C0);
		val |= 0x02C0;
		wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, 0x02C0);

		val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
		val &= ~(0x0022);
		val |= 0x0022;
		wm8994_write(codec, WM8994_ANALOGUE_HP_1, 0x0022);

		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
		val &= ~(0x0);
		val |= 0x0;
		wm8994_write(codec, WM8994_OUTPUT_MIXER_1, 0x0);

		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
		val &= ~(0x0);
		val |= 0x0;
		wm8994_write(codec, WM8994_OUTPUT_MIXER_2, 0x0);

		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
		val &= ~(0x0300);
		val |= 0x0300;
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, 0x0300);

		val = wm8994_read(codec, WM8994_CHARGE_PUMP_1);
		val &= ~(0x1F25);
		val |= 0x1F25;
		wm8994_write(codec, WM8994_CHARGE_PUMP_1, 0x1F25);
		break;

	case BT:
		val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1);
		val &= ~(WM8994_AIF1DAC1_MUTE_MASK | WM8994_AIF1DAC1_MONO_MASK);
		val |= (WM8994_AIF1DAC1_MUTE);
		wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val);
		break;

	case SPK_HP:
		val &= ~(WM8994_HPOUT1L_ENA_MASK | WM8994_HPOUT1R_ENA_MASK |
				WM8994_SPKOUTL_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

		/* Disable DAC1L to HPOUT1L path */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
		val &= ~(WM8994_DAC1L_TO_HPOUT1L_MASK |
				WM8994_DAC1L_TO_MIXOUTL_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

		/* Disable DAC1R to HPOUT1R path */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
		val &= ~(WM8994_DAC1R_TO_HPOUT1R_MASK |
				WM8994_DAC1R_TO_MIXOUTR_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

		/* Disable Charge Pump */
		val = wm8994_read(codec, WM8994_CHARGE_PUMP_1);
		val &= ~WM8994_CP_ENA_MASK;
		val |= WM8994_CP_ENA_DEFAULT;
		wm8994_write(codec, WM8994_CHARGE_PUMP_1, val);

		/* Intermediate HP settings */
		val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
		val &= ~(WM8994_HPOUT1R_DLY_MASK | WM8994_HPOUT1R_OUTP_MASK |
		      WM8994_HPOUT1R_RMV_SHORT_MASK | WM8994_HPOUT1L_DLY_MASK |
		      WM8994_HPOUT1L_OUTP_MASK | WM8994_HPOUT1L_RMV_SHORT_MASK);
		wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

		/* Disable SPKLVOL */
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
		val &= ~(WM8994_SPKLVOL_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

		/* Disable SPKOUT mixer */
		val = wm8994_read(codec, WM8994_SPKOUT_MIXERS);
		val &= ~(WM8994_SPKMIXL_TO_SPKOUTL_MASK |
			 WM8994_SPKMIXR_TO_SPKOUTL_MASK |
			 WM8994_SPKMIXR_TO_SPKOUTR_MASK);
		wm8994_write(codec, WM8994_SPKOUT_MIXERS, val);

		/* Mute Speaker mixer */
		val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
		val &= ~(WM8994_DAC1L_TO_SPKMIXL_MASK);
		wm8994_write(codec, WM8994_SPEAKER_MIXER, val);
		break;

	default:
		DEBUG_LOG_ERR("Path[%d] is not invaild!\n", path);
		return;
		break;
	}
}

void wm8994_disable_rec_path(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;
	enum mic_path mic = wm8994->rec_path;

	wm8994->rec_path = MIC_OFF;

	if (!(wm8994->codec_state & CALL_ACTIVE))
		audio_ctrl_mic_bias_gpio(wm8994->pdata, 0);

	switch (mic) {
	case MAIN:
		DEBUG_LOG("Disabling MAIN Mic Path..\n");

		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_2);
		val &= ~(WM8994_IN1L_ENA_MASK | WM8994_MIXINL_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_2, val);

		/* Mute IN1L PGA, update volume */
		val = wm8994_read(codec,
				WM8994_LEFT_LINE_INPUT_1_2_VOLUME);
		val &= ~(WM8994_IN1L_MUTE_MASK | WM8994_IN1L_VOL_MASK);
		val |= (WM8994_IN1L_VU | WM8994_IN1L_MUTE);
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME,
				val);

		/*Mute the PGA */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_3);
		val &= ~(WM8994_IN1L_TO_MIXINL_MASK |
			WM8994_IN1L_MIXINL_VOL_MASK |
			WM8994_MIXOUTL_MIXINL_VOL_MASK);
		wm8994_write(codec, WM8994_INPUT_MIXER_3, val);

		/* Disconnect IN1LN ans IN1LP to the inputs */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_2);
		val &= (WM8994_IN1LN_TO_IN1L_MASK | WM8994_IN1LP_TO_IN1L_MASK);
		wm8994_write(codec, WM8994_INPUT_MIXER_2, val);

		/* Digital Paths */
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
		val &= ~(WM8994_ADCL_ENA_MASK | WM8994_AIF1ADC1L_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);

		/* Disable timeslots */
		val = wm8994_read(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING);
		val &= ~(WM8994_ADC1L_TO_AIF1ADC1L);
		wm8994_write(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING, val);
		break;

	case SUB:
		DEBUG_LOG("Disbaling SUB Mic path..\n");
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_2);
		val &= ~(WM8994_IN1R_ENA_MASK | WM8994_MIXINR_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_2, val);

		/* Disable volume,unmute Right Line */
		val = wm8994_read(codec,
				WM8994_RIGHT_LINE_INPUT_1_2_VOLUME);
		val &= ~WM8994_IN1R_MUTE_MASK;	/* Unmute IN1R */
		val |= (WM8994_IN1R_VU | WM8994_IN1R_MUTE);
		wm8994_write(codec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,
			     val);

		/* Mute right pga, set volume */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_4);
		val &= ~(WM8994_IN1R_TO_MIXINR_MASK |
		      WM8994_IN1R_MIXINR_VOL_MASK |
		      WM8994_MIXOUTR_MIXINR_VOL_MASK);
		wm8994_write(codec, WM8994_INPUT_MIXER_4, val);

		/* Disconnect in1rn to inr1 and in1rp to inrp */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_2);
		val &= ~(WM8994_IN1RN_TO_IN1R_MASK | WM8994_IN1RP_TO_IN1R_MASK);
		wm8994_write(codec, WM8994_INPUT_MIXER_2, val);

		/* Digital Paths */
		/* Disable right ADC and time slot */
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
		val &= ~(WM8994_ADCR_ENA_MASK | WM8994_AIF1ADC1R_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);

		/* ADC Right mixer routing */
		val = wm8994_read(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING);
		val &= ~(WM8994_ADC1R_TO_AIF1ADC1R_MASK);
		wm8994_write(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING, val);
		break;

	case BT_REC:
		DEBUG_LOG("Disbaling BT Mic path..\n");
		val = wm8994_read(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING);
		val &= ~(WM8994_AIF2DACL_TO_AIF1ADC1L_MASK |
			WM8994_ADC1L_TO_AIF1ADC1L_MASK);
		wm8994_write(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING, val);

		val = wm8994_read(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING);
		val &= ~(WM8994_AIF2DACR_TO_AIF1ADC1R_MASK |
			WM8994_ADC1R_TO_AIF1ADC1R_MASK);
		wm8994_write(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING, val);

		val = wm8994_read(codec, WM8994_AIF2_DAC_FILTERS_1);
		val &= ~(WM8994_AIF2DAC_MUTE_MASK);
		val |= (WM8994_AIF2DAC_MUTE);
		wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val);
		break;

	case MIC_OFF:
		DEBUG_LOG("Mic is already OFF!\n");
		break;

	default:
		DEBUG_LOG_ERR("Path[%d] is not invaild!\n", mic);
		break;
	}
}

void wm8994_set_bluetooth_common_setting(struct snd_soc_codec *codec)
{
	u32 val;

	wm8994_write(codec, WM8994_GPIO_1, 0xA101);
	wm8994_write(codec, WM8994_GPIO_2, 0x8100);
	wm8994_write(codec, WM8994_GPIO_3, 0x0100);
	wm8994_write(codec, WM8994_GPIO_4, 0x0100);
	wm8994_write(codec, WM8994_GPIO_5, 0x8100);
	wm8994_write(codec, WM8994_GPIO_6, 0xA101);
	wm8994_write(codec, WM8994_GPIO_7, 0x0100);
	wm8994_write(codec, WM8994_GPIO_8, 0xA101);
	wm8994_write(codec, WM8994_GPIO_9, 0xA101);
	wm8994_write(codec, WM8994_GPIO_10, 0xA101);
	wm8994_write(codec, WM8994_GPIO_11, 0xA101);

	wm8994_write(codec, WM8994_FLL2_CONTROL_2, 0x0700);
	wm8994_write(codec, WM8994_FLL2_CONTROL_3, 0x3126);
	wm8994_write(codec, WM8994_FLL2_CONTROL_4, 0x0100);
	wm8994_write(codec, WM8994_FLL2_CONTROL_5, 0x0C88);
	wm8994_write(codec, WM8994_FLL2_CONTROL_1,
		WM8994_FLL2_FRACN_ENA | WM8994_FLL2_ENA);

	val = wm8994_read(codec, WM8994_AIF2_CLOCKING_1);
	if (!(val & WM8994_AIF2CLK_ENA))
		wm8994_write(codec, WM8994_AIF2_CLOCKING_1, 0x0018);

	wm8994_write(codec, WM8994_AIF2_RATE, 0x9 << WM8994_AIF2CLK_RATE_SHIFT);

	/* AIF2 Interface - PCM Stereo mode */
	/* Left Justified, BCLK invert, LRCLK Invert */
	wm8994_write(codec, WM8994_AIF2_CONTROL_1,
		WM8994_AIF2ADCR_SRC | WM8994_AIF2_BCLK_INV | 0x18);

	wm8994_write(codec, WM8994_AIF2_BCLK, 0x70);
	wm8994_write(codec, WM8994_AIF2_CONTROL_2, 0x0000);
	wm8994_write(codec, WM8994_AIF2_MASTER_SLAVE, WM8994_AIF2_MSTR |
		WM8994_AIF2_CLK_FRC | WM8994_AIF2_LRCLK_FRC);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_AIF2DACL_ENA_MASK | WM8994_AIF2DACR_ENA_MASK |
		WM8994_AIF1DAC1L_ENA_MASK | WM8994_AIF1DAC1R_ENA_MASK |
		WM8994_DAC1L_ENA_MASK | WM8994_DAC1R_ENA_MASK);
	val |= (WM8994_AIF2DACL_ENA | WM8994_AIF2DACR_ENA |
		WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* Clocking */
	val = wm8994_read(codec, WM8994_CLOCKING_1);
	val |= (WM8994_DSP_FS2CLK_ENA | WM8994_SYSCLK_SRC);
	wm8994_write(codec, WM8994_CLOCKING_1, val);

	/* AIF1 & AIF2 Output is connected to DAC1 */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK |
		WM8994_AIF2DACL_TO_DAC1L_MASK);
	val |= (WM8994_AIF1DAC1L_TO_DAC1L | WM8994_AIF2DACL_TO_DAC1L);
	wm8994_write(codec, WM8994_DAC1_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1R_TO_DAC1R_MASK |
		WM8994_AIF2DACR_TO_DAC1R_MASK);
	val |= (WM8994_AIF1DAC1R_TO_DAC1R | WM8994_AIF2DACR_TO_DAC1R);
	wm8994_write(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING, val);
}

void wm8994_record_headset_mic(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	u16 val;

	wm8994_earsel_control(wm8994->pdata, 1);
	
	wm8994_write(codec, 0x0700, 0xA101);	// GPIO 1

	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL);
	wm8994_write(codec, 0x0001, val);	 // Power Management 1
	msleep(50);
	
	wm8994_write(codec, 0x0300, 0xC010);	// AIF1 Control 1. Right ADC data is only. I2S Format.
	wm8994_write(codec, 0x0002, 0x0110);	// Power Management 2

	if(wm8994->input_source == RECOGNITION)
	{
		wm8994_write(codec, 0x001A, 0x011F);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x002A, 0x0020);	// Input Mixer 4
		wm8994_write(codec, 0x0400, 0x01D5);	// AIF1 ADC1 Left Volume
		wm8994_write(codec, 0x0401, 0x01D5);	// AIF1 ADC1 Right Volume

		wm8994_write(codec, 0x0410, 0x1800);	// AIF1 DRC 1(5)
		
		DEBUG_LOG("===================================================> Voice Search, Bing Search, Voice Command");
	}
	else
	{
		wm8994_write(codec, 0x001A, 0x0112);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x002A, 0x0020);	// Input Mixer 4
		wm8994_write(codec, 0x0400, 0x01FF);	// AIF1 ADC1 Left Volume
		wm8994_write(codec, 0x0401, 0x01FF);	// AIF1 ADC1 Right Volume

		/* DRC & Noise Gate */
		wm8994_write(codec, 0x0444, 0x0004);	// AIF1 DRC 1(5)
		wm8994_write(codec, 0x0442, 0x0800);	// AIF1 DRC 1(3)
		wm8994_write(codec, 0x0440, 0x0100);	// AIF1 DRC 1(1)
		
		DEBUG_LOG("===================================================> Camcorder Recording, Voice Recording");
	}
	wm8994_write(codec, 0x0028, 0x0001);	// Input Mixer 2. Single Ended Input. Negative is connected only.
	
	val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4);
	val &= ~(WM8994_ADCR_ENA_MASK |WM8994_AIF1ADC1R_ENA_MASK );
	val |= (WM8994_AIF1ADC1R_ENA | WM8994_ADCR_ENA	);
	wm8994_write(codec, 0x0004, val);	 // Power Management 4

	wm8994_write(codec, 0x0606, 0x0002);	// AIF1 ADC1 Left Mixer Routing. Enable.
	wm8994_write(codec, 0x0607, 0x0002);	// AIF1 ADC1 Right Mixer Routing. Enable.
	
	msleep(60);    // To remove the pop noise front of the recorded pcm data.
}


void wm8994_record_main_mic(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	u16 val;

	audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);

	if(wm8994->input_source == RECOGNITION)
	{
		// Disable FM radio path
		val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
		val &= ~WM8994_MIXINL_TO_SPKMIXL_MASK;
		wm8994_write(codec, WM8994_SPEAKER_MIXER, val);
	
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
		val &= ~WM8994_MIXINL_TO_MIXOUTL_MASK;
		wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val); 
		
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
		val &= ~WM8994_MIXINR_TO_MIXOUTR_MASK;
		wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val); 
		
		val = wm8994_read(codec, WM8994_DAC2_LEFT_MIXER_ROUTING);	//604H : 0x0010
		val &= ~(WM8994_ADC1_TO_DAC2L_MASK);
		wm8994_write(codec, WM8994_DAC2_LEFT_MIXER_ROUTING, val);
	
		val = wm8994_read(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING);	//605H : 0x0010
		val &= ~(WM8994_ADC1_TO_DAC2R_MASK);
		wm8994_write(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING, val);
	
		wm8994_write(codec, 0x0300, 0x0010);	// AIF1 Control 1. Left ADC data is only. I2S Format.
		wm8994_write(codec, WM8994_ANTIPOP_2, 0x68);	//Main mic volume issue fix: requested H/W
		wm8994_write(codec, 0x0002, 0x0240);	// Power Management 2
		wm8994_write(codec, 0x002A, 0x0000);	// Input Mixer 4.
	
		val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
		val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK);
		val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL);
		wm8994_write(codec, 0x0001, val);	 // Power Management 1

		/* Input Gain */
		wm8994_write(codec, 0x0018, 0x011E);	// Left Line Input 1&2 volume
		wm8994_write(codec, 0x0029, 0x0020);	// Input Mixer 3
		wm8994_write(codec, 0x0400, 0x01CA);	// AIF1 ADC1 Left Volume
		wm8994_write(codec, 0x0401, 0x01CA);	// AIF1 ADC1 Right Volume
		
		wm8994_write(codec, 0x0410, 0x1800);	// AIF1 DRC 1(5)
		wm8994_write(codec, 0x0028, 0x0010);	// Input Mixer 2. Single Ended Input. Negative is connected only.
		wm8994_write(codec, 0x0004, 0x0202);	// Power Management 4
		wm8994_write(codec, 0x0606, 0x0002);	// AIF1 ADC1 Left Mixer Routing. Enable.
		wm8994_write(codec, 0x0607, 0x0002);	// AIF1 ADC1 Right Mixer Routing. Enable.
		
		val = wm8994_read(codec, WM8994_AIF1_MASTER_SLAVE);
		val |= (WM8994_AIF1_MSTR | WM8994_AIF1_CLK_FRC | WM8994_AIF1_LRCLK_FRC);	// Master mode
		wm8994_write(codec, WM8994_AIF1_MASTER_SLAVE, val);
	
		wm8994_write( codec, WM8994_GPIO_1, 0xA101 );	// GPIO1 is Input Enable
		
		/* DRC & Noise Gate */
		wm8994_write(codec, 0x0444, 0x0004);	// AIF1 DRC 1(5)
		wm8994_write(codec, 0x0442, 0x0800);	// AIF1 DRC 1(3)
		wm8994_write(codec, 0x0440, 0x0100);	// AIF1 DRC 1(1)

		msleep(120);	// To remove the pop noise front of the recorded pcm data.

		DEBUG_LOG("===================================================> Voice Search, Bing Search, Voice Command"); 		   
	}
	else
	{
		wm8994_write(codec, 0x0700, 0xA101);   // GPIO1
	
		val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_1 );
		val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK);
		val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL);	
		wm8994_write(codec, 0x0001, val);	 // Power Management 1
		msleep(50);
		
		val = wm8994_read(codec, WM8994_AIF1_CONTROL_1);
		val &= ~(WM8994_AIF1ADCL_SRC_MASK | WM8994_AIF1ADCR_SRC_MASK);
		wm8994_write(codec, 0x0300, val);	 // AIF1 Control 1
		wm8994_write(codec, 0x0039, 0x68);	// Anti Pop
		wm8994_write(codec, 0x0002, 0x6240);	// Power Management 2

		/* Input Gain */
		wm8994_write(codec, 0x0018, 0x0112);	// Left Line Input 1&2 volume
		wm8994_write(codec, 0x0029, 0x0020);	// Input Mixer 3
		wm8994_write(codec, 0x0400, 0x01FF);	// AIF1 ADC1 Left Volume
		wm8994_write(codec, 0x0401, 0x01FF);	// AIF1 ADC1 Right Volume
	
		wm8994_write(codec, 0x0028, 0x0010);	// Input Mixer 2
		
		val = wm8994_read(codec,WM8994_POWER_MANAGEMENT_4 );
		val &= ~(WM8994_ADCL_ENA_MASK |WM8994_AIF1ADC1L_ENA_MASK  );
		val |= ( WM8994_AIF1ADC1L_ENA | WM8994_ADCL_ENA);
		wm8994_write(codec, 0x0004, val);	 // Power Management 4
	
		val = wm8994_read(codec,WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING );
		val |= WM8994_ADC1L_TO_AIF1ADC1L ;	
		wm8994_write(codec, 0x0606, val);		 // AIF1 ADC1 Left Mixer Routing. Enable.
		wm8994_write(codec, 0x0607, 0x0002);	// AIF1 ADC1 Right Mixer Routing. Enable.

#if 0    /* Block to resolve the issue that  the shutter sound is cracked. */
		msleep(60);    // To remove the pop noise front of the recorded pcm data.
#endif
		DEBUG_LOG("===================================================> Camcorder Recording, Voice Recording");
	}
}


void wm8994_record_bluetooth(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	wm8994_write(codec, 0x0001, 0x0003);	// Power Management 1
	msleep(50);
	
	wm8994_write(codec, 0x0700, 0xA101);	// GPIO 1
	wm8994_write(codec, 0x0702, 0x0100);	// GPIO 3
	wm8994_write(codec, 0x0704, 0x8100);	// GPIO 5
	wm8994_write(codec, 0x0706, 0x0100);	// GPIO 7
	wm8994_write(codec, 0x0707, 0x8100);	// GPIO 8
	wm8994_write(codec, 0x0708, 0x0100);	// GPIO 9
	wm8994_write(codec, 0x0709, 0x0100);	// GPIO 10
	wm8994_write(codec, 0x070A, 0x0100);	// GPIO 11

	wm8994_write(codec, 0x0240, 0x0005);	// FLL2 Control 1
	wm8994_write(codec, 0x0241, 0x2F00);	// FLL2 Control 2
	wm8994_write(codec, 0x0242, 0x3126);	// FLL2 Control 3
	wm8994_write(codec, 0x0244, 0x0C88);	// FLL2 Control 5
	wm8994_write(codec, 0x0243, 0x0100);	// FLL2 Control 4
	msleep(3);
	
	/* Audio Interface & Clock Setting */
	wm8994_write(codec, 0x0211, 0x0003);	// AIF2 Rate
	wm8994_write(codec, 0x0310, 0x4118);	// AIF2 Control 1
	wm8994_write(codec, 0x0312, 0x4000);	// AIF2 Master/Slave
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1
	wm8994_write(codec, 0x0208, 0x000E);	// Clocking 1
	wm8994_write(codec, 0x0210, 0x0073);	// AIF1 Rate
	wm8994_write(codec, 0x0200, 0x0011);	// AIF1 Clocking 1
	
	/* Input Path Routing */
	wm8994_write(codec, 0x0606, 0x0001);	// AIF1 ADC1 Left Mixer Routing
	wm8994_write(codec, 0x0607, 0x0001);	// AIF1 ADC1 Right Mixer Routing
	wm8994_write(codec, 0x0604, 0x0001);	// DAC2 Left Mixer Routing
	wm8994_write(codec, 0x0605, 0x0001);	// DAC2 Right Mixer Routing
	wm8994_write(codec, 0x0520, 0x0000);	// AIF2 DAC Filter
	wm8994_write(codec, 0x0422, 0x0000);	// AIF1 DAC2 Filter 1
	wm8994_write(codec, 0x0423, 0x0010);	// AIF1 DAC2 FIlter 2
	wm8994_write(codec, 0x0004, 0x3300);	// Power Management 4
	wm8994_write(codec, 0x0005, 0x3300);	// Power Management 5

	/* Output Path Routing */
	wm8994_write(codec, 0x0601, 0x0004);	// DAC1 Left Mixer Routing
	wm8994_write(codec, 0x0602, 0x0004);	// DAC1 Right Mixer Routing
	wm8994_write(codec, 0x0420, 0x0000);	// AIF1 DAC1 Filter
	
	/* Input Volume */
	wm8994_write(codec, 0x0400, 0x01DA);	// AIF1 ADC1 Left Volume
	wm8994_write(codec, 0x0401, 0x01DA);	// AIF1 ADC1 Right Volume
	wm8994_write(codec, 0x0502, 0x01C0);	// AIF2 DAC Left Volume
	wm8994_write(codec, 0x0503, 0x01C0);	// AIF2 DAC Right Volume

	/* Output Volume */
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
	
	wm8994_write(codec, 0x0620, 0x0000);	// Oversampling
	wm8994_write(codec, 0x0621, 0x01C0);	// Sidetone

	wm8994_write(codec, 0x0006, 0x000A);	// Power Management 6
	
	DEBUG_LOG("");
}

void wm8994_set_playback_receiver(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	u16 val;

	DEBUG_LOG("");
	
	wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
	wm8994_write(codec, 0x0032, 0x0000);	// Output Mixer 6
	wm8994_write(codec, 0x0020, 0x017D);	// OPGA Left Volume
	wm8994_write(codec, 0x0021, 0x017D);	// OPGA Right Volume
	wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	wm8994_write(codec, 0x0402, 0x01C0);	// AIF1 DAC1 Left Volume
	wm8994_write(codec, 0x0403, 0x01C0);	// AIF1 DAC1 Right Volume
	
	wm8994_write(codec, 0x002D, 0x0001);	// Output Mixer 1
	wm8994_write(codec, 0x002E, 0x0001);	// Output Mixer 2
	wm8994_write(codec, 0x0033, 0x0018);	// HPOUT2 Mixer
	wm8994_write(codec, 0x0005, 0x0303);	// Power Management 5
	wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 Filter
	wm8994_write(codec, 0x0601, 0x0001);	// DAC1 Left Mixer
	wm8994_write(codec, 0x0602, 0x0001);	// DAC1 Right Mixer
	wm8994_write(codec, 0x0208, 0x000A);	// Clocking 1
	wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3
	wm8994_write(codec, 0x0001, 0x0803);	// Power Management 1
}


void wm8994_set_playback_headset(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	u16 val;
	u16 TestReturn1=0;
	u16 TestReturn2=0;
	u16 TestLow1=0;
	u16 TestHigh1=0;
	u8 TestLow=0;
	u8 TestHigh=0;
	
	DEBUG_LOG("");

	wm8994_write(codec, 0x0601, 0x0001);	// DAC1 Left Mixer
	wm8994_write(codec, 0x0602, 0x0001);	// DAC1 Right Mixer
	wm8994_write(codec, 0x0102, 0x0003);
	wm8994_write(codec, 0x0056, 0x0003);
	wm8994_write(codec, 0x0102, 0x0000);
	wm8994_write(codec, 0x0051, 0x0005);	// Class W

	if(wm8994->output_source == RING_TONE)
	{
		wm8994_write(codec, 0x001C, 0x016D);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x016D);	// Right Output Volume
		wm8994_write(codec, 0x0020, 0x0179 );	 // Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume
	}
	else
	{
		wm8994_write(codec, 0x001C, 0x0174);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0174);	// Right Output Volume
		wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume
	}
	wm8994_write(codec, 0x0402, 0x01C0);	// AIF1 DAC1 Left Volume
	wm8994_write(codec, 0x0403, 0x01C0);	// AIF1 DAC1 Right Volume

	wm8994_write(codec, 0x0055, 0x03E0);	// DC Servo 2
	wm8994_write(codec, 0x0001, 0x0303);	// Power Management 1
	wm8994_write(codec, 0x0060, 0x0022);	// Analogue HP 1
	wm8994_write(codec, 0x004C, 0x9F25);	// Charge Pump 1

	msleep(5);	// 20ms delay

	wm8994_write(codec, 0x0005, 0x0303);	// Power Management 5
	wm8994_write(codec, 0x002D, 0x0001);	// Output Mixer 1
	wm8994_write(codec, 0x002E, 0x0001);	// Output Mixer 2
	wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3
	wm8994_write(codec, 0x0054, 0x0303);	// DC Servo 1
	
	msleep(160);	// 160ms delay

	TestReturn1=wm8994_read(codec,WM8994_DC_SERVO_4);
	TestLow=(signed char)(TestReturn1 & 0xff);
	TestHigh=(signed char)((TestReturn1>>8) & 0xff);
	TestLow1=((signed short)(TestLow-5))&0x00ff;
	TestHigh1=(((signed short)(TestHigh-5)<<8)&0xff00);
	TestReturn2=TestLow1|TestHigh1;
	
	wm8994_write(codec, 0x0057, TestReturn2);	 // DC Servo 4
	wm8994_write(codec, 0x0054, 0x000F);	// DC Servo 1

	msleep(20);
	
	wm8994_write(codec, 0x0060, 0x00EE);	// Analogue HP 1
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	wm8994_write(codec, 0x0420, 0x0000);	// AIF1 DAC1 Filter
}


void wm8994_set_playback_speaker(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	DEBUG_LOG("");

	wm8994_write(codec, 0x0003, 0x0300);	// Power Management 3

	if(wm8994->output_source == RING_TONE)
	{
		wm8994_write(codec, 0x0026, 0x017D);	// Speaker Left Volume
		wm8994_write(codec, 0x0027, 0x017D);	// Speaker Right Volume
		wm8994_write(codec, 0x0025, (0x0005 << WM8994_SPKOUTL_BOOST_SHIFT) | 0x0005);	 // Speaker Booster
	}
	else
	{
		wm8994_write(codec, 0x0026, 0x01FD);	// Speaker Left Volume
		wm8994_write(codec, 0x0027, 0x01FD);	// Speaker Right Volume
		wm8994_write(codec, 0x0025, (0x0005 << WM8994_SPKOUTL_BOOST_SHIFT) | 0x0005);	 // Speaker Booster
	}
	wm8994_write(codec, 0x0022, 0x0000);	// SPKMIXL Attenuation
	wm8994_write(codec, 0x0023, 0x0000);	// SPKMIXR Attenuation
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	wm8994_write(codec, 0x0402, 0x01C0);	// AIF1 DAC1 Left Volume
	wm8994_write(codec, 0x0403, 0x01C0);	// AIF1 DAC1 Right Volume

	wm8994_write(codec, 0x0024, 0x0011);	// Speaker Output Mixer
	wm8994_write(codec, 0x0036, 0x0003);	// Speaker Mixer
	wm8994_write(codec, 0x0005, 0x0303);	// Power Management 5
	wm8994_write(codec, 0x0420, 0x0000);	// AIF1 DAC1 Filter
	wm8994_write(codec, 0x0601, 0x0001);	// DAC1 Left Mixer
	wm8994_write(codec, 0x0602, 0x0001);	// DAC1 Right Mixer
	wm8994_write(codec, 0x0001, 0x3003);	// Power Management 1

	/* EQ 350Hz Cutoff Frequency */
	wm8994_write(codec, 0x0482, 0x0F39);
	wm8994_write(codec, 0x0483, 0x0402);
	wm8994_write(codec, 0x0484, 0x031C);
	wm8994_write(codec, 0x0485, 0x1EA9);
	wm8994_write(codec, 0x0486, 0xF150);
	wm8994_write(codec, 0x0487, 0x040A);
	wm8994_write(codec, 0x0488, 0x0533);
	wm8994_write(codec, 0x0480, 0x0319);
}


void wm8994_set_playback_speaker_headset(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	u16 val;
	u16 nReadServo4Val = 0;
	u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	//------------------  Common Settings ------------------
	wm8994_write(codec, 0x0601, 0x0001);	// DAC1 Left Mixer
	wm8994_write(codec, 0x0602, 0x0001);	// DAC1 Right Mixer
	wm8994_write(codec, 0x0402, 0x01C0);	// AIF1 DAC1 Left Volume
	wm8994_write(codec, 0x0403, 0x01C0);	// AIF1 DAC1 Right Volume

	//------------------  Speaker Path Settings ------------------

	/* Speaker Volume Control */
	if(wm8994->output_source == RING_TONE)
	{
		wm8994_write(codec, 0x0026, 0x01FC);	// Speaker Left Volume
		wm8994_write(codec, 0x0027, 0x01FC);	// Speaker Right Volume
		wm8994_write(codec, 0x0025, (0x0005 << WM8994_SPKOUTL_BOOST_SHIFT) | 0x0006);	 // Speaker Booster
	}
	else
	{
		wm8994_write(codec, 0x0026, 0x01FD);	// Speaker Left Volume
		wm8994_write(codec, 0x0027, 0x01FD);	// Spekaer Right Volume
		wm8994_write(codec, 0x0025, (0x0006 << WM8994_SPKOUTL_BOOST_SHIFT) | 0x0006);	 // Speaker Booster
	}

	wm8994_write(codec, 0x0024, 0x0011);	// Speaker Output Mixer
	wm8994_write(codec, 0x0036, 0x0003);	// Speaker Mixer

	/* EQ 350Hz Cutoff Frequency */
	wm8994_write(codec, 0x0482, 0x0F39);
	wm8994_write(codec, 0x0483, 0x0402);
	wm8994_write(codec, 0x0484, 0x031C);
	wm8994_write(codec, 0x0485, 0x1EA9);
	wm8994_write(codec, 0x0486, 0xF150);
	wm8994_write(codec, 0x0487, 0x040A);
	wm8994_write(codec, 0x0488, 0x0533);
	wm8994_write(codec, 0x0480, 0x0319);

	//------------------  Ear Path Settings ------------------
	//Configuring the Digital Paths
	wm8994_write(codec, 0x0102, 0x0003);
	wm8994_write(codec, 0x0056, 0x0003);
	wm8994_write(codec, 0x0102, 0x0000);
	wm8994_write(codec, 0x0051, 0x0005);	// Class W

	if(wm8994->output_source == RING_TONE)
	{
		wm8994_write(codec, 0x001C, 0x016D);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x016D);	// Right Output Volume
	}
	else
	{
		wm8994_write(codec, 0x001C, 0x0159);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0159);	// Right Output Volume
	}

	wm8994_write(codec, 0x0055, 0x03E0);	// DC Servo 2
	wm8994_write(codec, 0x0001, 0x3303);
	wm8994_write(codec, 0x0060, 0x0022);	// Analogue HP 1
	wm8994_write(codec, 0x004C, 0x9F25);	// Charge Pump

	msleep(5);

	wm8994_write(codec, 0x0005, 0x0303);	// Power Management 5
	wm8994_write(codec, 0x002D, 0x0001);	// Output Mixer 1
	wm8994_write(codec, 0x002E, 0x0001);	// Output Mixer 2
	wm8994_write(codec, 0x0003, 0x0330);	// Power Management 3
	wm8994_write(codec, 0x0054, 0x0303);	// DC Servo 1

	msleep(160);

	nReadServo4Val=wm8994_read(codec,WM8994_DC_SERVO_4);
	nServo4Low=(signed char)(nReadServo4Val & 0xff);
	nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
	nCompensationResultLow=((signed short)nServo4Low -5)&0x00ff;
	nCompensationResultHigh=((signed short)(nServo4High -5)<<8)&0xff00;
	ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
	wm8994_write(codec, 0x0057, ncompensationResult);	 // DC Servo 4
	wm8994_write(codec, 0x0054, 0x000F);	// DC Servo 1

	msleep(15); 

	wm8994_write(codec, 0x0060, 0x00EE);	// Analogue HP 1
	
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume

	
	//------------------  Common Settings ------------------
	wm8994_write(codec, 0x0420, 0x0000);	// AIF1 DAC1 Filter
	
	wm8994_write(codec, 0x0022, 0x0000);	// SPKMIXL Attenuation
	wm8994_write(codec, 0x0023, 0x0000);	// SPKMIXR Attenuation

}


void wm8994_set_playback_bluetooth(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	wm8994_write(codec, 0x0001, 0x0003);	// Power Management 1
	msleep(50);
	
	wm8994_write(codec, 0x0700, 0xA101);	// GPIO 1
	wm8994_write(codec, 0x0702, 0x0100);	// GPIO 3
	wm8994_write(codec, 0x0704, 0x8100);	// GPIO 5
	wm8994_write(codec, 0x0706, 0x0100);	// GPIO 7
	wm8994_write(codec, 0x0707, 0x8100);	// GPIO 8
	wm8994_write(codec, 0x0708, 0x0100);	// GPIO 9
	wm8994_write(codec, 0x0709, 0x0100);	// GPIO 10
	wm8994_write(codec, 0x070A, 0x0100);	// GPIO 11

	wm8994_write(codec, 0x0240, 0x0005);	// FLL2 Control 1
	wm8994_write(codec, 0x0241, 0x2F00);	// FLL2 Control 2
	wm8994_write(codec, 0x0242, 0x3126);	// FLL2 Control 3
	wm8994_write(codec, 0x0244, 0x0C88);	// FLL2 Control 5
	wm8994_write(codec, 0x0243, 0x0100);	// FLL2 Control 4
	msleep(3);
	
	/* Audio Interface & Clock Setting */
	wm8994_write(codec, 0x0211, 0x0003);	// AIF2 Rate
	wm8994_write(codec, 0x0310, 0x4118);	// AIF2 Control 1
	wm8994_write(codec, 0x0312, 0x4000);	// AIF2 Master/Slave
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1
	wm8994_write(codec, 0x0208, 0x000E);	// Clocking 1
	wm8994_write(codec, 0x0210, 0x0073);	// AIF1 Rate
	wm8994_write(codec, 0x0200, 0x0011);	// AIF1 Clocking 1
	
	/* Input Path Routing */
	wm8994_write(codec, 0x0606, 0x0001);	// AIF1 ADC1 Left Mixer Routing
	wm8994_write(codec, 0x0607, 0x0001);	// AIF1 ADC1 Right Mixer Routing
	wm8994_write(codec, 0x0604, 0x0001);	// DAC2 Left Mixer Routing
	wm8994_write(codec, 0x0605, 0x0001);	// DAC2 Right Mixer Routing
	wm8994_write(codec, 0x0520, 0x0000);	// AIF2 DAC Filter
	wm8994_write(codec, 0x0422, 0x0000);	// AIF1 DAC2 Filter 1
	wm8994_write(codec, 0x0423, 0x0010);	// AIF1 DAC2 FIlter 2
	wm8994_write(codec, 0x0004, 0x3300);	// Power Management 4
	wm8994_write(codec, 0x0005, 0x3300);	// Power Management 5

	/* Output Path Routing */
	wm8994_write(codec, 0x0601, 0x0004);	// DAC1 Left Mixer Routing
	wm8994_write(codec, 0x0602, 0x0004);	// DAC1 Right Mixer Routing
	wm8994_write(codec, 0x0420, 0x0000);	// AIF1 DAC1 Filter
	
	/* Input Volume */
	wm8994_write(codec, 0x0400, 0x01DA);	// AIF1 ADC1 Left Volume
	wm8994_write(codec, 0x0401, 0x01DA);	// AIF1 ADC1 Right Volume
	wm8994_write(codec, 0x0502, 0x01C0);	// AIF2 DAC Left Volume
	wm8994_write(codec, 0x0503, 0x01C0);	// AIF2 DAC Right Volume

	/* Output Volume */
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
	
	wm8994_write(codec, 0x0620, 0x0000);	// Oversampling
	wm8994_write(codec, 0x0621, 0x01C0);	// Sidetone

	wm8994_write(codec, 0x0006, 0x000A);	// Power Management 6
	
	DEBUG_LOG("");
}

void wm8994_set_playback_extra_dock_speaker(struct snd_soc_codec *codec)
{
    /* Volume Setting */
    wm8994_write(codec, 0x0031, 0x0000);    // Output Mixer 5
	wm8994_write(codec, 0x0032, 0x0000);    // Output Mixer 6
	wm8994_write(codec, 0x0020, 0x0177);    // Left OPGA Volume
	wm8994_write(codec, 0x0021, 0x0177);    // Right OPGA Volume
    wm8994_write(codec, 0x001E, 0x0000);    // Lineout Volume
	wm8994_write(codec, 0x0610, 0x01C0);    // DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);    // DAC 1 Right Volume
	wm8994_write(codec, 0x0402, 0x01C0);    // AIF1 DAC 1 Left Volume
	wm8994_write(codec, 0x0403, 0x01C0);    // AIF1 DAC 1 Rigt Volume

    /* Output Path Routing */
    wm8994_write(codec, 0x002D, 0x0001);    // Output Mixer1
    wm8994_write(codec, 0x002E, 0x0001);    // Output Mixer2
    wm8994_write(codec, 0x0035, 0x0031);    // Line Mixer 2
    wm8994_write(codec, 0x0005, 0x0303);    // Power Management5
    wm8994_write(codec, 0x0601, 0x0001);    // DAC1 Left Mixer Routing
    wm8994_write(codec, 0x0602, 0x0001);    // DAC1 Right Mixer Routing
    wm8994_write(codec, 0x0208, 0x000A);    // Clocking 1
    wm8994_write(codec, 0x0003, 0x0CF0);    // Power Management 3
    wm8994_write(codec, 0x0420, 0x0000);    // DAC1 Filter

    /* Channel Seperation */
    wm8994_write(codec, 0x0037, 0x0000);    // Lineout Control
    wm8994_write(codec, 0x0038, 0x0080);    // AntiPop1
    
    FSA9480_Enable_SPK(1);

    DEBUG_LOG("");
}

void wm8994_set_voicecall_common_setting(struct snd_soc_codec *codec)
{
	int val;

	/* GPIO Configuration */
	wm8994_write(codec, WM8994_GPIO_1, 0xA101);
	wm8994_write(codec, WM8994_GPIO_2, 0x8100);
	wm8994_write(codec, WM8994_GPIO_3, 0x0100);
	wm8994_write(codec, WM8994_GPIO_4, 0x0100);
	wm8994_write(codec, WM8994_GPIO_5, 0x8100);
	wm8994_write(codec, WM8994_GPIO_6, 0xA101);
	wm8994_write(codec, WM8994_GPIO_7, 0x0100);
	wm8994_write(codec, WM8994_GPIO_8, 0xA101);
	wm8994_write(codec, WM8994_GPIO_9, 0xA101);
	wm8994_write(codec, WM8994_GPIO_10, 0xA101);
	wm8994_write(codec, WM8994_GPIO_11, 0xA101);

	wm8994_write(codec, WM8994_FLL2_CONTROL_2, 0x2F00);
	wm8994_write(codec, WM8994_FLL2_CONTROL_3, 0x3126);
	wm8994_write(codec, WM8994_FLL2_CONTROL_4, 0x0100);
	wm8994_write(codec, WM8994_FLL2_CONTROL_5, 0x0C88);
	wm8994_write(codec, WM8994_FLL2_CONTROL_1,
		WM8994_FLL2_FRACN_ENA | WM8994_FLL2_ENA);

	val = wm8994_read(codec, WM8994_AIF2_CLOCKING_1);
	if (!(val & WM8994_AIF2CLK_ENA))
		wm8994_write(codec, WM8994_AIF2_CLOCKING_1, 0x0018);

	wm8994_write(codec, WM8994_AIF2_RATE, 0x3 << WM8994_AIF2CLK_RATE_SHIFT);

	/* AIF2 Interface - PCM Stereo mode */
	/* Left Justified, BCLK invert, LRCLK Invert */
	wm8994_write(codec, WM8994_AIF2_CONTROL_1,
		WM8994_AIF2ADCR_SRC | WM8994_AIF2_BCLK_INV | 0x18);

	wm8994_write(codec, WM8994_AIF2_BCLK, 0x70);
	wm8994_write(codec, WM8994_AIF2_CONTROL_2, 0x0000);
	wm8994_write(codec, WM8994_AIF2_MASTER_SLAVE, WM8994_AIF2_MSTR |
		WM8994_AIF2_CLK_FRC | WM8994_AIF2_LRCLK_FRC);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_AIF2DACL_ENA_MASK | WM8994_AIF2DACR_ENA_MASK |
		WM8994_AIF1DAC1L_ENA_MASK | WM8994_AIF1DAC1R_ENA_MASK |
		WM8994_DAC1L_ENA_MASK | WM8994_DAC1R_ENA_MASK);
	val |= (WM8994_AIF2DACL_ENA | WM8994_AIF2DACR_ENA |
		WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* Clocking */
	val = wm8994_read(codec, WM8994_CLOCKING_1);
	val |= (WM8994_DSP_FS2CLK_ENA);
	wm8994_write(codec, WM8994_CLOCKING_1, val);

	wm8994_write(codec, WM8994_POWER_MANAGEMENT_6, 0x0);

	/* AIF1 & AIF2 Output is connected to DAC1 */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK |
		WM8994_AIF2DACL_TO_DAC1L_MASK);
	val |= (WM8994_AIF1DAC1L_TO_DAC1L | WM8994_AIF2DACL_TO_DAC1L);
	wm8994_write(codec, WM8994_DAC1_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1R_TO_DAC1R_MASK |
		WM8994_AIF2DACR_TO_DAC1R_MASK);
	val |= (WM8994_AIF1DAC1R_TO_DAC1R | WM8994_AIF2DACR_TO_DAC1R);
	wm8994_write(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING, val);

	wm8994_write(codec, 0x6, 0x0);
}

#ifdef FEATURE_SS_AUDIO_CAL
void wm8994_set_voicecall_receiver(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	int val;

	wm8994_write(codec, 0x0039, 0x0068);	// Anti Pop2
	wm8994_write(codec, 0x0001, 0x0003);	// Power Management 1
	msleep(50);
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x0817, 0x0000);	// To remove the robotic sound
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x0015, 0x0040);
	wm8994_write(codec, 0x0702, 0x8100);	// GPIO 3. Speech PCM Clock
	wm8994_write(codec, 0x0703, 0x8100);	// GPIO 4. Speech PCM Sync
	wm8994_write(codec, 0x0704, 0x8100);	// GPIO 5. Speech PCM Data Out
	wm8994_write(codec, 0x0706, 0x0100);	// GPIO 7. Speech PCM Data Input
#ifdef CONFIG_ARIES_VER_B2
	wm8994_write(codec, 0x0244, 0x0C83);	// FLL2 Control 5
#else
	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
#endif
	wm8994_write(codec, 0x0241, 0x0700);	// FLL2 Control 2
	wm8994_write(codec, 0x0242, 0x0000);	// FLL2 Control 3
	wm8994_write(codec, 0x0243, 0x0600);	// FLL2 Control 4
	wm8994_write(codec, 0x0240, 0x0001);	// FLL2 Control 1
	msleep(3);

	/* Audio Interface & Clock Setting */
	wm8994_write(codec, 0x0204, 0x0018);	// AIF2 Clocking 1. Clock Source Select
	wm8994_write(codec, 0x0208, 0x000F);	// Clocking 1. '0x000A' is added for a playback. (original = 0x0007)
	wm8994_write(codec, 0x0620, 0x0000);	// Oversampling
	wm8994_write(codec, 0x0211, 0x0009);	// AIF2 Rate
	wm8994_write(codec, 0x0302, 0x4000);	// AIF1 Master Slave Setting. To prevent that the music is played slowly.
	wm8994_write(codec, 0x0312, 0x0000);	// AIF2 Master Slave Setting
	wm8994_write(codec, 0x0310, 0x4118);	// AIF2 Control 1
	wm8994_write(codec, 0x0311, 0x0000);	// AIF2 Control 2
	wm8994_write(codec, 0x0520, 0x0080);	// AIF2 DAC Filter 1
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1. AIF2 Clock Enable

	/* Input Path Routing */
	wm8994_write(codec, 0x0028, 0x0030);	// Input Mixer 2
	wm8994_write(codec, 0x0002, 0x6240);	// Power Management 2
	wm8994_write(codec, 0x0029, 0x0030);	// Input Mixer 3
	wm8994_write(codec, 0x0004, 0x2002);	// Power Management 4
	wm8994_write(codec, 0x0604, 0x0010);	// DAC2 Left Mixer Routing

	audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);
	
	/* Output Path Routing */
	wm8994_write(codec, 0x0005, 0x2303);	// Power Management 5. '0x0303' is added for a playback. (Original = 0x2002)
	wm8994_write(codec, 0x0601, 0x0015);	// DAC1 Left Mixer Routing. '0x0001' is added for a playback. (Original = 0x0004)
	wm8994_write(codec, 0x0602, 0x0001);	// DAC1 Right Mixer Routing(Playback)
	wm8994_write(codec, 0x002D, 0x0001);	// Output Mixer 1
	wm8994_write(codec, 0x002E, 0x0001);	// Output Mixer 2(Playback)
	wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3. '0x00F0' is added for a playback. (Original = 0x00A0)
	wm8994_write(codec, 0x0033, 0x0018);	// HPOUT2 Mixer. '0x0008' is added for a playback. (Original = 0x0010)
	wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 FIlter(Playback)

	/* Input Path Volume */
	if(loopback_mode == PBA_LOOPBACK_MODE_ON || loopback_mode == SIMPLETEST_LOOPBACK_MODE_ON)
	{
		wm8994_write(codec, 0x0018, 0x0110);	// Left Line Input 1&2 Volume
	}
	else
	{
		wm8994_write(codec, 0x0018, 0x0116);	// Left Line Input 1&2 Volume
	}
	wm8994_write(codec, 0x0500, 0x01C0);	// AIF2 ADC Left Volume
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0603, 0x000C);	// DAC2 Mixer Volumes

	wm8994_write(codec, 0x0621, 0x01C0);	// Sidetone

	/* Output Path Volume */
	wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
	wm8994_write(codec, 0x0032, 0x0000);	// Output Mixer 6

	if(loopback_mode == PBA_LOOPBACK_MODE_ON || loopback_mode == SIMPLETEST_LOOPBACK_MODE_ON)
	{
		wm8994_write(codec, 0x0020, 0x01F9);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x01F9);	// Right OPGA Volume
	}
	else
	{
		wm8994_write(codec, 0x0020, 0x01FD);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x01FD);	// Right OPGA Volume
	}
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume

#if 1
	wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
#else
	if(wm8994->codec_state & CALL_ACTIVE)
	{
		wm8994_write(codec, 0x001F, (TUNING_HPOUT2_VOL << 0x0004));    // HPOUT2 Volume
	}
	else
	{
		wm8994_write(codec, 0x001F, 0x0020);	// HPOUT2 Volume
	}
#endif

	wm8994_write(codec, 0x0015, 0x0000);	
	wm8994_write(codec, 0x0038, 0x0040);	// Anti Pop 1
	wm8994_write(codec, 0x0006, 0x0000);	// Power Management 6. Prevent the mute when the audio transfer is executed from the bluetooth.

	/* Sidetone */
	wm8994_write(codec, 0x0600, 0x0002);	// DAC1 Mixer Volume

	if(!(wm8994->codec_state & CALL_ACTIVE))
	{
		msleep(300);
	}
	wm8994_write(codec, 0x0001, 0x0803);	// Power Management 1
//	  msleep(50);
	wm8994_write(codec, 0x0224, 0x0C98);	// FLL1 Control(5). To set again the sampling rate for a AP sound.
	
	DEBUG_LOG("");
}


void wm8994_set_voicecall_headset(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	int val;
	
	wm8994_write(codec, 0x0039, 0x006C);	// Anti Pop 2
	wm8994_write(codec, 0x0001, 0x0003);	// Power Management 1
	msleep(50);
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x0817, 0x0000);	// To remove the robotic sound
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x004C, 0x9F25);	// Charge Pump 1

	wm8994_write(codec, 0x0702, 0x8100);	// GPIO 3. Speech PCM Clock
	wm8994_write(codec, 0x0703, 0x8100);	// GPIO 4. Speech PCM Sync
	wm8994_write(codec, 0x0704, 0x8100);	// GPIO 5. Speech PCM Data Out
	wm8994_write(codec, 0x0706, 0x0100);	// GPIO 7. Speech PCM Data Input

#ifdef CONFIG_ARIES_VER_B2
	wm8994_write(codec, 0x0244, 0x0C83);	// FLL2 Control 5
#else
	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
#endif
	wm8994_write(codec, 0x0241, 0x0700);	// FLL2 Control 2
	wm8994_write(codec, 0x0242, 0x0000);	// FLL2 Control 3
	wm8994_write(codec, 0x0243, 0x0600);	// FLL2 Control 4
	wm8994_write(codec, 0x0240, 0x0001);	// FLL2 Control 1
	msleep(3);

	/* Audio Interface & Clock Setting */
	wm8994_write(codec, 0x0204, 0x0018);	// AIF2 Clocking 1
	wm8994_write(codec, 0x0208, 0x000F);	// Clocking 1. '0x000A' is added for a playback. (original = 0x0007)
	wm8994_write(codec, 0x0620, 0x0000);	// Oversampling
	wm8994_write(codec, 0x0211, 0x0009);	// AIF2 Rate
	wm8994_write(codec, 0x0302, 0x4000);	// AIF1 Master Slave Setting. To prevent that the music is played slowly.
	wm8994_write(codec, 0x0312, 0x0000);	// AIF2 Master Slave Setting

	if(tty_mode == TTY_MODE_VCO)
	{
		wm8994_write(codec, 0x0310, 0x4118);	// AIF2 Control 1
	}
	else
	{
		wm8994_write(codec, 0x0310, 0xC118);	// AIF2 Control 1
	}
	wm8994_write(codec, 0x0311, 0x0000);	// AIF2 Control 2
	wm8994_write(codec, 0x0520, 0x0080);	// AIF2 DAC Filter1
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1

	/* Input Path Routing */
	if(tty_mode == TTY_MODE_VCO)
	{
		wm8994_write(codec, 0x0028, 0x0030);	// Input Mixer 2
		wm8994_write(codec, 0x0002, 0x6240);	// Power Management 2
		wm8994_write(codec, 0x0029, 0x0030);	// Input Mixer 
		wm8994_write(codec, 0x0004, 0x2002);	// Power Management 4

		audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);
	}
	else if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0028, 0x0001);	// Input Mixer 2
		wm8994_write(codec, 0x0002, 0x6130);	// Power Management 2
		wm8994_write(codec, 0x002A, 0x0020);	// Input Mixer 4
		wm8994_write(codec, 0x0004, 0x1001);	// Power Management 4
	}
	else
	{
		wm8994_write(codec, 0x0028, 0x0001);	// Input Mixer 2
		wm8994_write(codec, 0x0002, 0x6130);	// Power Management 2
		wm8994_write(codec, 0x002A, 0x0020);	// Input Mixer 4
		wm8994_write(codec, 0x0004, 0x1001);	// Power Management 4
	}
	wm8994_write(codec, 0x0604, 0x0030);	// DAC2 Left Mixer Routing
	wm8994_write(codec, 0x0605, 0x0030);	// DAC2 Right Mixer Routing

	if(tty_mode == TTY_MODE_VCO)
	{
		audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);
	}
	else
	{
		wm8994_earsel_control(wm8994->pdata, 1);
	}

	/* Output Path Routing */
	wm8994_write(codec, 0x0005, 0x3303);	// Power Management 5

	if(tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0601, 0x0005);	// DAC1 Left Mixer Routing
		wm8994_write(codec, 0x0602, 0x0005);	// DAC1 Right Mixer Routing
		wm8994_write(codec, 0x002D, 0x0001);	// Output Mixer 1
		wm8994_write(codec, 0x002E, 0x0001);	// Output Mixer 2
		wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3
		wm8994_write(codec, 0x0033, 0x0018);	// HPOUT2 Mixer
		wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 Filter1
	}
	else
	{
		wm8994_write(codec, 0x0601, 0x0005);	// DAC1 Left Mixer Routing
		wm8994_write(codec, 0x0602, 0x0005);	// DAC1 Right Mixer Routing
		wm8994_write(codec, 0x002D, 0x0100);	// Output Mixer 1
		wm8994_write(codec, 0x002E, 0x0100);	// Output Mixer 2
		wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3(Playback)
		wm8994_write(codec, 0x0060, 0x00EE);	// Analogue HP 1
		wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 Filter1
	}

	/* Input Path Volume */
	if(tty_mode == TTY_MODE_VCO)
	{
		wm8994_write(codec, 0x0018, 0x0116);	// Left Line Input 1&2 Volume
		wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	}
	else if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x001A, 0x0105);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
	}
	else if(loopback_mode == PBA_LOOPBACK_MODE_ON)
	{
		wm8994_write(codec, 0x001A, 0x0114);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
		wm8994_write(codec, 0x0501, 0x01EF);	// AIF2 Right ADC Volume
	}
	else if(loopback_mode == SIMPLETEST_LOOPBACK_MODE_ON)
	{
		wm8994_write(codec, 0x001A, 0x0114);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
		wm8994_write(codec, 0x0501, 0x01EF);	// AIF2 Right ADC Volume
	}
	else
	{
		wm8994_write(codec, 0x001A, 0x0118);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
		wm8994_write(codec, 0x0501, 0x01EF);	// AIF2 Right ADC Volume
	}
	wm8994_write(codec, 0x0603, 0x018C);	// DAC2 Mixer Volumes

	/* Output Path Volume */
	if(tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Output Mixer 6
		wm8994_write(codec, 0x0020, 0x017D);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x017D);	// Right OPGA Volume
		wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume

#if 1
		wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
#else
		if(wm8994->codec_state == CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001F, (TUNING_HPOUT2_VOL << 0x0004));    // HPOUT2 Volume
		}
		else
		{
			wm8994_write(codec, 0x001F, 0x0020);	// HPOUT2 Volume
		}
#endif
	}
	else if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Outupt Mixer 6
		wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume

#if 1
		wm8994_write(codec, 0x001C, 0x0179);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0179);	// Right Output Volume
#else
		if(wm8994->codec_state & CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
		}
		else
		{
			wm8994_write(codec, 0x001C, 0x0100);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0100);	// Right Output Volume
		}
#endif
		wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	}		 
	else
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Outupt Mixer 6
		wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume

#if 1
		wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
#else
		if(wm8994->codec_state & CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001C, 0x0176);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0176);	// Right Output Volume
		}
		else
		{
			wm8994_write(codec, 0x001C, 0x0100);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0100);	// Right Output Volume
		}
#endif
		wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	}

	wm8994_write(codec, 0x0006, 0x0000);	// Power Management 6. Prevent the mute when the audio transfer is executed from the bluetooth.
	
	if(tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0038, 0x0040);	// Anti Pop 1
	}			 
	wm8994_write(codec, 0x0621, 0x01C0);	// Sidetone

	if(!(wm8994->codec_state & CALL_ACTIVE))
	{
		msleep(300);
	}
	
	if(tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0001, 0x0803);	// Power Management 1
	}
	else
	{
		wm8994_write(codec, 0x0001, 0x0303);	// Power Management 1
	}
	wm8994_write(codec, 0x0224, 0x0C98);	// FLL1 Control(5). To set again the sampling rate for a AP sound.

	DEBUG_LOG("");
	DEBUG_LOG("============================> TTY Mode = %d.", tty_mode);
}


void wm8994_set_voicecall_headphone(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	int val;
	
	wm8994_write(codec, 0x0039, 0x006C);	// Anti Pop 2
	wm8994_write(codec, 0x0001, 0x0003);	// Power Management 1
	msleep(50);
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x0817, 0x0000);	// To remove the robotic sound
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x004C, 0x9F25);	// Charge Pump 1

	wm8994_write(codec, 0x0702, 0x8100);	// GPIO 3. Speech PCM Clock
	wm8994_write(codec, 0x0703, 0x8100);	// GPIO 4. Speech PCM Sync
	wm8994_write(codec, 0x0704, 0x8100);	// GPIO 5. Speech PCM Data Out
	wm8994_write(codec, 0x0706, 0x0100);	// GPIO 7. Speech PCM Data Input

#ifdef CONFIG_ARIES_VER_B2
	wm8994_write(codec, 0x0244, 0x0C83);	// FLL2 Control 5
#else
	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
#endif
	wm8994_write(codec, 0x0241, 0x0700);	// FLL2 Control 2
	wm8994_write(codec, 0x0242, 0x0000);	// FLL2 Control 3
	wm8994_write(codec, 0x0243, 0x0600);	// FLL2 Control 4
	wm8994_write(codec, 0x0240, 0x0001);	// FLL2 Control 1
	msleep(3);

	/* Audio Interface & Clock Setting */
	wm8994_write(codec, 0x0204, 0x0018);	// AIF2 Clocking 1
	wm8994_write(codec, 0x0208, 0x000F);	// Clocking 1. '0x000A' is added for a playback. (original = 0x0007)
	wm8994_write(codec, 0x0620, 0x0000);	// Oversampling
	wm8994_write(codec, 0x0211, 0x0009);	// AIF2 Rate
	wm8994_write(codec, 0x0302, 0x4000);	// AIF1 Master Slave Setting. To prevent that the music is played slowly.
	wm8994_write(codec, 0x0312, 0x0000);	// AIF2 Master Slave Setting

	wm8994_write(codec, 0x0310, 0x4118);	// AIF2 Control 1
	wm8994_write(codec, 0x0311, 0x0000);	// AIF2 Control 2
	wm8994_write(codec, 0x0520, 0x0080);	// AIF2 DAC Filter1
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1

	/* Input Path Routing */
	wm8994_write(codec, 0x0028, 0x0030);	// Input Mixer 2
	wm8994_write(codec, 0x0002, 0x6240);	// Power Management 2
	wm8994_write(codec, 0x0029, 0x0030);	// Input Mixer 3
	wm8994_write(codec, 0x0004, 0x2002);	// Power Management 4
	wm8994_write(codec, 0x0604, 0x0010);	// DAC2 Left Mixer Routing

	audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);

	/* Output Path Routing */
	wm8994_write(codec, 0x0005, 0x3303);	// Power Management 5

	if(tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0601, 0x0005);	// DAC1 Left Mixer Routing
		wm8994_write(codec, 0x0602, 0x0005);	// DAC1 Right Mixer Routing
		wm8994_write(codec, 0x002D, 0x0001);	// Output Mixer 1
		wm8994_write(codec, 0x002E, 0x0001);	// Output Mixer 2
		wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3
		wm8994_write(codec, 0x0033, 0x0018);	// HPOUT2 Mixer
		wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 Filter1
	}
	else
	{
		wm8994_write(codec, 0x0601, 0x0005);	// DAC1 Left Mixer Routing
		wm8994_write(codec, 0x0602, 0x0005);	// DAC1 Right Mixer Routing
		wm8994_write(codec, 0x002D, 0x0100);	// Output Mixer 1
		wm8994_write(codec, 0x002E, 0x0100);	// Output Mixer 2
		wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3(Playback)
		wm8994_write(codec, 0x0060, 0x00EE);	// Analogue HP 1
		wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 Filter1
	}

	/* Input Path Volume */
	wm8994_write(codec, 0x0018, 0x0116);	// Left Line Input 1&2 Volume
	wm8994_write(codec, 0x0500, 0x01C0);	// AIF2 ADC Left Volume
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0603, 0x000C);	// DAC2 Mixer Volumes

	/* Output Path Volume */
	if(tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Output Mixer 6
		wm8994_write(codec, 0x0020, 0x017D);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x017D);	// Right OPGA Volume
		wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume

#if 1
		wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
#else
		if(wm8994->codec_state == CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001F, (TUNING_HPOUT2_VOL << 0x0004));    // HPOUT2 Volume
		}
		else
		{
			wm8994_write(codec, 0x001F, 0x0020);	// HPOUT2 Volume
		}
#endif
	}
	else if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Outupt Mixer 6
		wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume

#if 1
		wm8994_write(codec, 0x001C, 0x0179);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0179);	// Right Output Volume
#else
		if(wm8994->codec_state & CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
		}
		else
		{
			wm8994_write(codec, 0x001C, 0x0100);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0100);	// Right Output Volume
		}
#endif
		wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	}		 
	else
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Outupt Mixer 6
		wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume

#if 1
		wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
#else
		if(wm8994->codec_state & CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001C, 0x0176);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0176);	// Right Output Volume
		}
		else
		{
			wm8994_write(codec, 0x001C, 0x0100);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0100);	// Right Output Volume
		}
#endif
		wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	}

	wm8994_write(codec, 0x0006, 0x0000);	// Power Management 6. Prevent the mute when the audio transfer is executed from the bluetooth.
	
	if(tty_mode == TTY_MODE_HCO)
	{
		wm8994_write(codec, 0x0038, 0x0040);	// Anti Pop 1
	}			 
	wm8994_write(codec, 0x0621, 0x01C0);	// Sidetone

	if(!(wm8994->codec_state & CALL_ACTIVE))
	{
		msleep(300);
	}
	
	wm8994_write(codec, 0x0001, 0x0303);	// Power Management 1
	wm8994_write(codec, 0x0224, 0x0C98);	// FLL1 Control(5). To set again the sampling rate for a AP sound.

	DEBUG_LOG("");
	DEBUG_LOG("============================> TTY Mode = %d.", tty_mode);
}




void wm8994_set_voicecall_speaker(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	int val;

	wm8994_write(codec, 0x0039, 0x006C);	// Anti Pop 2
	wm8994_write(codec, 0x0001, 0x0003);	// Power Management 1
	msleep(50);
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x0817, 0x0000);	// To remove the robotic sound
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound

	wm8994_write(codec, 0x0702, 0x8100);	// GPIO 3. Speech PCM Clock
	wm8994_write(codec, 0x0703, 0x8100);	// GPIO 4. Speech PCM Sync
	wm8994_write(codec, 0x0704, 0x8100);	// GPIO 5. Speech PCM Data Out
	wm8994_write(codec, 0x0706, 0x0100);	// GPIO 7. Speech PCM Data Input

#ifdef CONFIG_ARIES_VER_B2
	wm8994_write(codec, 0x0244, 0x0C83);	// FLL2 Control 5
#else
	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
#endif
	wm8994_write(codec, 0x0241, 0x0700);	// FLL2 Control 2
	wm8994_write(codec, 0x0242, 0x0000);	// FLL2 Control 3
	wm8994_write(codec, 0x0243, 0x0600);	// FLL2 Control 4
	wm8994_write(codec, 0x0240, 0x0001);	// FLL2 Control 1
	msleep(3);

	/* Audio Interface & Clock Setting */
	wm8994_write(codec, 0x0204, 0x0018);	// AIF2 Clocking 1
	wm8994_write(codec, 0x0208, 0x000F);	// Clocking 1. '0x000A' is added for a playback. (original = 0x0007)
	wm8994_write(codec, 0x0620, 0x0000);	// Oversampling
	wm8994_write(codec, 0x0211, 0x0009);	// AIF2 Rate
	wm8994_write(codec, 0x0300, 0x0010);	// AIF1 Control 1
	wm8994_write(codec, 0x0302, 0x4000);	// AIF1 Master Slave Setting. To prevent that the music is played slowly.
	wm8994_write(codec, 0x0312, 0x0000);	// AIF2 Master Slave Setting
	wm8994_write(codec, 0x0310, 0x4118);	// AIF2 Control 1
#ifdef STEREO_SPEAKER_SUPPORT
	wm8994_write(codec, 0x0311, 0x4000);	// AIF2 Control 2
#else
	wm8994_write(codec, 0x0311, 0x0000);	// AIF2 Control 2
#endif
	wm8994_write(codec, 0x0520, 0x0080);	// AIF2 DAC Filter1
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1

	/* Input Path Routing */
	wm8994_write(codec, 0x0028, 0x0030);	// Input Mixer 2.
	wm8994_write(codec, 0x0002, 0x6240);	// Power Management 2

	wm8994_write(codec, 0x0029, 0x0030);	// Input Mixer 3.
	wm8994_write(codec, 0x0004, 0x2002);	// Power Management 4
	wm8994_write(codec, 0x0604, 0x0010);	// DAC2 Left Mixer Routing
	wm8994_write(codec, 0x0605, 0x0010);	// DAC2 Right Mixer Routing

	audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);

	/* Output Path Routing */
	wm8994_write(codec, 0x0005, 0x3303);	// Power Management 5
	wm8994_write(codec, 0x0003, 0x0300);	// Power Management 3
	wm8994_write(codec, 0x0601, 0x0005);	// DAC1 Left Mixer Routing. '0x0001' is added for a playback. (Original = 0x0004)
	wm8994_write(codec, 0x0602, 0x0005);	// DAC1 Right Mixer Routing(Playback)
#ifdef STEREO_SPEAKER_SUPPORT
	wm8994_write(codec, 0x0024, 0x0011);	// SPKOUT Mixers
#else
	wm8994_write(codec, 0x0024, 0x0010);	// SPKOUT Mixers
#endif
	wm8994_write(codec, 0x0420, 0x0080);	// AIF2 DAC Filter1(Playback)

	/* Input Path Volume */
	wm8994_write(codec, 0x0018, 0x010C);	// Left Line Input 1&2 Volume
	wm8994_write(codec, 0x0603, 0x000C);	// DAC2 Mixer Volumes
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume

	/* Output Path Volume */
#ifdef STEREO_SPEAKER_SUPPORT
	wm8994_write(codec, 0x0025, 0x002D);	// SPKOUT Boost
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	wm8994_write(codec, 0x0022, 0x0000);	// SPKMIXL Attenuation
	wm8994_write(codec, 0x0023, 0x0000);	// SPKMIXR Attenuation
#if 1
	wm8994_write(codec, 0x0026, 0x017A);	// Speaker Volume Left
	wm8994_write(codec, 0x0027, 0x017A);	// Speaker Volume Right 
#else
	if(wm8994->codec_state == CALL_ACTIVE)
	{
		wm8994_write(codec, 0x0026, (0x0140 | TUNING_CALL_SPKL_VOL));	 // Speaker Volume Left
		wm8994_write(codec, 0x0027, (0x0100 | 0x0040 | 0x003E));	  // Speaker Volume Right 
	}
	else
	{
		wm8994_write(codec, 0x0026, 0x0100);	// Speaker Volume Left
		wm8994_write(codec, 0x0027, 0x0100);	// Speaker Volume Right 
	}
#endif
#else
	wm8994_write(codec, 0x0025, (0x0007 << 0x0003));	// SPKOUT Boost
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0022, 0x0000);	// SPKMIXL Attenuation
#if 1
	wm8994_write(codec, 0x0026, 0x017A);	// Speaker Volume Left
#else
	if(wm8994->codec_state == CALL_ACTIVE)
	{
		wm8994_write(codec, 0x0026, (0x0140 | 0x003E));	 // Speaker Volume Left
	}
	else
	{
		wm8994_write(codec, 0x0026, 0x0100);	// Speaker Volume Left
	}
#endif
#endif /* STEREO_SPEAKER_SUPPORT */

	wm8994_write(codec, 0x0006, 0x0000);	// Power Management 6. Prevent the mute when the audio transfer is executed from the bluetooth.
	wm8994_write(codec, 0x0621, 0x01C0);	// Sidetone
	wm8994_write(codec, 0x0036, 0x0003);

	if(!(wm8994->codec_state & CALL_ACTIVE))
	{
		msleep(300);
	}
	
#ifdef STEREO_SPEAKER_SUPPORT
	wm8994_write(codec, 0x0001, 0x3003);	// Power Management 1
#else
	wm8994_write(codec, 0x0001, 0x1003);	// Power Management 1
#endif
//	  msleep(50);
	wm8994_write(codec, 0x0224, 0x0C98);	// FLL1 Control(5). To set again the sampling rate for a AP sound.

	DEBUG_LOG("");
}



void wm8994_set_voicecall_bluetooth(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	wm8994_write(codec, 0x0039, 0x0068);	// Anti Pop2
	wm8994_write(codec, 0x0001, 0x0003);	// Power Management 1
	msleep(50);
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound
	wm8994_write(codec, 0x0817, 0x0000);	// To remove the robotic sound
	wm8994_write(codec, 0x0102, 0x0003);	// To remove the robotic sound

	wm8994_write(codec, 0x0704, 0x8100);	// GPIO 5. Speech PCM OUT
	wm8994_write(codec, 0x0706, 0x0100);	// GPIO 7. Speech PCM IN
	wm8994_write(codec, 0x0702, 0x8100);	// GPIO 3. Speech PCM CLK
	wm8994_write(codec, 0x0703, 0x8100);	// GPIO 4. Speech PCM SYNC
	wm8994_write(codec, 0x0707, 0x8100);	// GPIO 8. BT PCM DOUT
	wm8994_write(codec, 0x0708, 0x0100);	// GPIO 9. BT PCM DIN
	wm8994_write(codec, 0x0709, 0x0100);	// GPIO 10. BT PCM SYNC
	wm8994_write(codec, 0x070A, 0x0100);	// GPIO 11. BT PCM CLK

#ifdef CONFIG_ARIES_VER_B2
	wm8994_write(codec, 0x0244, 0x0C83);	// FLL2 Control 5
#else
	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
#endif
	wm8994_write(codec, 0x0241, 0x0700);	// FLL2 Control 2
	wm8994_write(codec, 0x0242, 0x0000);	// FLL2 Control 3
	wm8994_write(codec, 0x0243, 0x0600);	// FLL2 Control 4
	wm8994_write(codec, 0x0240, 0x0001);	// FLL2 Cotnrol 1
	msleep(3);
	
	/* Audio Interface & Clock Setting */
	wm8994_write(codec, 0x0204, 0x0018);	// AIF2 Clocking 1. Clock Source Select
	wm8994_write(codec, 0x0208, 0x000F);	// Clocking 1. '0x000A' is added for a playback. (original = 0x0007)
	wm8994_write(codec, 0x0620, 0x0000);	// Oversampling
	wm8994_write(codec, 0x0211, 0x0009);	// AIF2 Rate
	wm8994_write(codec, 0x0302, 0x4000);	// AIF1 Master Slave Setting. To prevent that the music is played slowly.
	wm8994_write(codec, 0x0312, 0x0000);	// AIF2 Master Slave Setting
	wm8994_write(codec, 0x0310, 0x4118);	// AIF2 Control 1
	wm8994_write(codec, 0x0311, 0x0000);	// AIF2 Control 2
	wm8994_write(codec, 0x0520, 0x0080);	// AIF2 DAC Filter 1
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1. AIF2 Clock Enable

	/* Input Path Routing */
	wm8994_write(codec, 0x0002, 0x4000);	// Power Management 2
	wm8994_write(codec, 0x0004, 0x3000);	// Power Management 4
	wm8994_write(codec, 0x0604, 0x0007);	// DAC2 Left Mixer Routing(Playback)
	wm8994_write(codec, 0x0605, 0x0007);	// DAC2 Right Mixer(Playback)
	wm8994_write(codec, 0x0015, 0x0040);	
	
	/* Output Path Routing */
	wm8994_write(codec, 0x004C, 0x1F25);	// Charge Pump 1
	wm8994_write(codec, 0x0006, 0x000C);	// Power Management 6. Input = GPIO8, Output = AIF2
	wm8994_write(codec, 0x0003, 0x0000);	// Power Management 3
	wm8994_write(codec, 0x0005, 0x3303);	// Power Management 5. '0x3300' is added for a playback. (Original = 0x0003)
	wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 FIlter(Playback)

	/* Output Path Volume */
	wm8994_write(codec, 0x0402, 0x01C0);	// AIF1 DAC1 Left Volume(Playback)
	wm8994_write(codec, 0x0500, 0x01C8);	// AIF2 ADC Left Volume
	wm8994_write(codec, 0x0501, 0x01C8);	// AIF2 ADC Right Volume

#if 1
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
#else
	if(wm8994->codec_state & CALL_ACTIVE)
	{
		wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
	}
	else
	{
		wm8994_write(codec, 0x0612, 0x0100);	// DAC2 Left Volume(Playback)
		wm8994_write(codec, 0x0613, 0x0100);	// DAC2 Right Volume(Playback)
	}
#endif
	wm8994_write(codec, 0x0015, 0x0000);
	wm8994_write(codec, 0x0224, 0x0C98);	// FLL1 Control(5). To set again the sampling rate for a AP sound.
	
	DEBUG_LOG("");
}


void wm8994_mute_voicecall_path(struct snd_soc_codec *codec, int path)
{
	if(path == RCV)
	{
		wm8994_write(codec, 0x0702, 0x100);    // GPIO 3. Speech PCM Clock
	wm8994_write(codec, 0x0703, 0x100);    // GPIO 4. Speech PCM Sync
	wm8994_write(codec, 0x0704, 0x100);    // GPIO 5. Speech PCM Data Out
	wm8994_write(codec, 0x0706, 0x8100);	// GPIO 7. Speech PCM Data Input
	
		/* Output Path Volume */
		wm8994_write(codec, 0x0610, 0x0100);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x0100);	// DAC1 Right Volume
		wm8994_write(codec, 0x0020, 0x0040);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0040);	// Right OPGA Volume
		wm8994_write(codec, 0x001F, 0x0020);	// HPOUT2 Volume

		/* Output Path Routing */
		wm8994_write(codec, 0x0033, 0x0000);	// HPOUT2 Mixer. '0x0008' is added for a playback. (Original = 0x0010)
		wm8994_write(codec, 0x0420, 0x0200);	// AIF1 DAC1 FIlter(Playback)

		/* Input Path Volume */
		wm8994_write(codec, 0x0018, 0x008B);	// Left Line Input 1&2 Volume
		wm8994_write(codec, 0x0612, 0x0100);	// DAC2 Left Volume
		wm8994_write(codec, 0x0603, 0x0000);	// DAC2 Mixer Volumes
	
		DEBUG_LOG("===========================> The receiver voice path is muted.");
	}
	else if(path == HP)
	{
		/* Output Path Volume */
		if(tty_mode == TTY_MODE_HCO)
		{
			wm8994_write(codec, 0x0020, 0x0000);	// Left OPGA Volume
			wm8994_write(codec, 0x0021, 0x0000);	// Right OPGA Volume
			wm8994_write(codec, 0x001F, 0x0020);	// HPOUT2 Volume
			wm8994_write(codec, 0x0610, 0x0100);	// DAC1 Left Volume
			wm8994_write(codec, 0x0611, 0x0100);	// DAC1 Right Volume
			wm8994_write(codec, 0x0033, 0x0000);	// HPOUT2 Mixer
		}
		else
		{
			wm8994_write(codec, 0x001C, 0x0100);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0100);	// Right Output Volume
			wm8994_write(codec, 0x0020, 0x0000);	// Left OPGA Volume
			wm8994_write(codec, 0x0021, 0x0000);	// Right OPGA Volume
			wm8994_write(codec, 0x0610, 0x0100);	// DAC1 Left Volume
			wm8994_write(codec, 0x0611, 0x0100);	// DAC1 Right Volume
			wm8994_write(codec, 0x0060, 0x0000);	// Analogue HP 1
		}

		/* Output Path Routing */
		if(tty_mode == TTY_MODE_HCO)
		{
			wm8994_write(codec, 0x0033, 0x0000);	// HPOUT2 Mixer
			wm8994_write(codec, 0x0420, 0x0200);	// AIF1 DAC1 Filter1
		}
		else
		{
			wm8994_write(codec, 0x0060, 0x0000);	// Analogue HP 1
			wm8994_write(codec, 0x0420, 0x0200);	// AIF1 DAC1 Filter1
		}

		/* Input Path Volume */
		if(tty_mode == TTY_MODE_VCO)
		{
			wm8994_write(codec, 0x0018, 0x008B);	// Left Line Input 1&2 Volume
		}
		else
		{
			wm8994_write(codec, 0x001A, 0x008B);	// Right Line Input 1&2 Volume
		}
		wm8994_write(codec, 0x0612, 0x0100);	// DAC2 Left Volume
		wm8994_write(codec, 0x0613, 0x0100);	// DAC2 Right Volume
		wm8994_write(codec, 0x0603, 0x0000);	// DAC2 Mixer Volumes
		DEBUG_LOG("===========================> The headset voice path is muted.");
	}
	else if(path == SPK)
	{
		/* Output Path Volume */
		wm8994_write(codec, 0x0025, 0x0000);	// SPKOUT Boost
		wm8994_write(codec, 0x0026, 0x0100);	// Speaker Volume Left
		wm8994_write(codec, 0x0027, 0x0100);	// Speaker Volume Right 
		wm8994_write(codec, 0x0613, 0x0100);	// DAC2 Right Volume

		/* Output Path Routing */
		wm8994_write(codec, 0x0024, 0x0000);	// SPKOUT Mixers
		wm8994_write(codec, 0x0420, 0x0200);	// AIF2 DAC Filter1(Playback)

		/* Input Path Volume */
		wm8994_write(codec, 0x0019, 0x008B);	// Left Line Input 3&4 Volume. SPK Mic using the IN2NL | DM1CDAT1
		wm8994_write(codec, 0x0604, 0x0000);	// DAC2 Left Mixer Routing
		wm8994_write(codec, 0x0605, 0x0000);	// DAC2 Right Mixer Routing

		DEBUG_LOG("===========================> The speaker voice path is muted.");
	}
	else if(path == BT)
	{
		/* Output Path Volume */
		wm8994_write(codec, 0x0420, 0x0200);	// AIF1 DAC1 FIlter(Playback)
		
		/* Input Path Routing */
		wm8994_write(codec, 0x0604, 0x0007);	// DAC2 Left Mixer Routing(Playback)
		wm8994_write(codec, 0x0605, 0x0007);	// DAC2 Right Mixer(Playback)
	
		wm8994_write(codec, 0x0707, 0x8000);	// GPIO 8. BT PCM DOUT
		wm8994_write(codec, 0x0708, 0x0000);	// GPIO 9. BT PCM DIN
		wm8994_write(codec, 0x0709, 0x0000);	// GPIO 10. BT PCM SYNC
		wm8994_write(codec, 0x070A, 0x0000);	// GPIO 11. BT PCM CLK

		/* Input Path Volume */
		wm8994_write(codec, 0x0612, 0x0100);	// DAC2 Left Volume(Playback)
		wm8994_write(codec, 0x0613, 0x0100);	// DAC2 Right Volume(Playback)
		wm8994_write(codec, 0x0402, 0x0100);	// AIF1 DAC1 Left Volume(Playback)

		DEBUG_LOG("===========================> The bluetooth voice path is muted.");
	}
}


void wm8994_set_end_point_volume(struct snd_soc_codec *codec, int path)
{
	switch(path)
	{
		case RCV :
		{
			wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume

			DEBUG_LOG("===========================> The end point volume for a receiver is set.");
			
			break;
		}
		case HP :
		{
			if(tty_mode == TTY_MODE_HCO)
			{
				wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
			}
			else if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
			{
				wm8994_write(codec, 0x001C, 0x01ED);	// Left Output Volume
				wm8994_write(codec, 0x001D, 0x01ED);	// Right Output Volume
			}
			else
			{
				wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
				wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
			}
			
			DEBUG_LOG("===========================> The end point volume for a headset is set.");
			
			break;
		}
		case SPK :
		{
			wm8994_write(codec, 0x0026, 0x017A);	// Speaker Volume Left
#ifdef STEREO_SPEAKER_SUPPORT
			wm8994_write(codec, 0x0027, 0x017A);	// Speaker Volume Right 
#endif

			DEBUG_LOG("===========================> The end point volume for a speaker is set.");

			break;
		}
		case BT :
		{
			wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume(Playback)
			wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume(Playback)

			DEBUG_LOG("===========================> The end point volume for a bluetooth is set.");
			
			break;
		}
		default :
		{
			break;
		}
	}
}

#endif

void wm8994_set_headset_common(struct snd_soc_codec *codec,
	enum wm8994_dc_servo_slots slots)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	u16 testreturn1 = 0;
	u16 testreturn2 = 0;
	u16 testlow1 = 0;
	u16 testhigh1 = 0;
	u8 testlow = 0;
	u8 testhigh = 0;

	/* Headset */
	wm8994_write(codec, 0x102, 0x0003);
	wm8994_write(codec, 0x56, 0x0003);
	wm8994_write(codec, 0x102, 0x0000);

	/* Charge pump control : Class W
	 * Using dynamic charge pump control mode whitch allows the power
	 * consumption to be optimised in real time, but can only be used
	 * if a single AIF source is the only signal source
	 */
	val = wm8994_read(codec, WM8994_CLASS_W_1);
	val &= ~(0x0005);
	val |= 0x0005;
	wm8994_write(codec, WM8994_CLASS_W_1, val);

	val = wm8994_read(codec, WM8994_DC_SERVO_2);
	val &= ~(0x03E0);
	val = 0x03E0;
	wm8994_write(codec, WM8994_DC_SERVO_2, val);

	/* Enable HPOUT1 intermediate stage */
	val = (WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_DLY);
	wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

	/* Enable Charge Pump, this is from wolfson */
	val = wm8994_read(codec, WM8994_CHARGE_PUMP_1);
	val &= ~WM8994_CP_ENA_MASK;
	val |= WM8994_CP_ENA | WM8994_CP_ENA_DEFAULT;
	wm8994_write(codec, WM8994_CHARGE_PUMP_1, val);

	msleep(5);

	if (!wm8994->dc_servo[slots]) {
		wait_for_dc_servo(codec,
				  WM8994_DCS_TRIG_SERIES_0 |
				  WM8994_DCS_TRIG_SERIES_1);

		testreturn1 = wm8994_read(codec, WM8994_DC_SERVO_4);

		testlow = (signed char)(testreturn1 & 0xff);
		testhigh = (signed char)((testreturn1>>8) & 0xff);

		testlow1 = ((signed short)(testlow-5)) & 0x00ff;
		testhigh1 = (((signed short)(testhigh-5)<<8) & 0xff00);
		testreturn2 = testlow1|testhigh1;
	} else {
		testreturn2 = wm8994->dc_servo[slots];
	}

	wm8994_write(codec, WM8994_DC_SERVO_4, testreturn2);
	wm8994->dc_servo[slots] = testreturn2;

	wait_for_dc_servo(codec,
			  WM8994_DCS_TRIG_DAC_WR_0 | WM8994_DCS_TRIG_DAC_WR_1);

	val = wm8994_read(codec, WM8994_LEFT_OUTPUT_VOLUME);
	val &= ~(WM8994_HPOUT1L_MUTE_N_MASK);
	val |= (WM8994_HPOUT1L_MUTE_N);
	wm8994_write(codec, WM8994_LEFT_OUTPUT_VOLUME, val);

	val = wm8994_read(codec, WM8994_RIGHT_OUTPUT_VOLUME);
	val &= ~(WM8994_HPOUT1R_MUTE_N_MASK);
	val |= (WM8994_HPOUT1R_MUTE_N);
	wm8994_write(codec, WM8994_RIGHT_OUTPUT_VOLUME, val);

}

void wm8994_disable_fmradio_path(struct snd_soc_codec *codec,
	enum fmradio_path path)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	DEBUG_LOG("Turn off fmradio_path = [%d]", path);

	switch (path) {
	case FMR_SPK:
		/* Disable end point for preventing pop up noise */
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
		val &= ~(WM8994_SPKOUTL_ENA_MASK | WM8994_HPOUT1L_ENA_MASK |
			WM8994_HPOUT1R_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_1 , val);

		/* Mute SPKOUTL */
		val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_LEFT);
		val &= ~WM8994_SPKOUTL_MUTE_N_MASK;
		val |= WM8994_SPKOUT_VU;
		wm8994_write(codec, WM8994_SPEAKER_VOLUME_LEFT, val);

		/* Mute SPKOUTR */
		val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_RIGHT);
		val &= ~WM8994_SPKOUTR_MUTE_N_MASK;
		val |= WM8994_SPKOUT_VU;
		wm8994_write(codec, WM8994_SPEAKER_VOLUME_RIGHT, val);

		/* Disable SPKMIX to SPKOUT */
		val = wm8994_read(codec, WM8994_SPKOUT_MIXERS);
		val &= ~(WM8994_SPKMIXL_TO_SPKOUTL_MASK |
			WM8994_SPKMIXR_TO_SPKOUTL_MASK |
			WM8994_SPKMIXR_TO_SPKOUTR_MASK);
		wm8994_write(codec, WM8994_SPKOUT_MIXERS, val);

		/* Disable MIXIN to SPKMIX */
		val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
		val &= ~(WM8994_MIXINL_TO_SPKMIXL_MASK |
			WM8994_MIXINR_TO_SPKMIXR_MASK);
		wm8994_write(codec, WM8994_SPEAKER_MIXER, val);
		break;

	case FMR_HP:

		/* cut off the power supply in analog domain */
		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
		val &= ~(WM8994_SPKOUTL_ENA_MASK | WM8994_HPOUT1L_ENA_MASK |
			WM8994_HPOUT1R_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_1 , val);

		val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
		val &= ~(WM8994_MIXOUTLVOL_ENA_MASK |
			WM8994_MIXOUTRVOL_ENA_MASK | WM8994_MIXOUTL_ENA_MASK |
			WM8994_MIXOUTR_ENA_MASK);
		wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

		/* Mute HPOUT1L_VOL */
		val = wm8994_read(codec, WM8994_LEFT_OUTPUT_VOLUME);
		val &= ~WM8994_HPOUT1L_MUTE_N_MASK;
		val |= WM8994_HPOUT1_VU;
		wm8994_write(codec, WM8994_LEFT_OUTPUT_VOLUME, val);

		/* Mute HPOUT1R_VOL */
		val = wm8994_read(codec, WM8994_RIGHT_OUTPUT_VOLUME);
		val &= ~WM8994_HPOUT1R_MUTE_N_MASK;
		val |= WM8994_HPOUT1_VU;
		wm8994_write(codec, WM8994_RIGHT_OUTPUT_VOLUME, val);

		/* Disable Charge Pump, this is from wolfson */
		val = wm8994_read(codec, WM8994_CHARGE_PUMP_1);
		val &= ~WM8994_CP_ENA_MASK;
		val |= WM8994_CP_ENA_DEFAULT;
		wm8994_write(codec, WM8994_CHARGE_PUMP_1, val);

		/* Intermediate HP settings */
		val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
		val &= ~(WM8994_HPOUT1R_DLY_MASK | WM8994_HPOUT1R_OUTP_MASK |
			WM8994_HPOUT1R_RMV_SHORT_MASK |
			WM8994_HPOUT1L_DLY_MASK | WM8994_HPOUT1L_OUTP_MASK |
			WM8994_HPOUT1L_RMV_SHORT_MASK);
		wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

		/* Disable Output mixer setting */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
		val &= ~(WM8994_DAC1L_TO_HPOUT1L_MASK |
			WM8994_MIXINL_TO_MIXOUTL_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
		val &= ~(WM8994_DAC1R_TO_HPOUT1R_MASK |
			WM8994_MIXINR_TO_MIXOUTR_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

		/* disble DAC1 to MIXOUT */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
		val &= ~WM8994_DAC1L_TO_MIXOUTL;
		wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
		val &= ~WM8994_DAC1R_TO_MIXOUTR;
		wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

		break;

	case FMR_DUAL_MIX:
		/* Disable DAC1L to SPKMIXL */
		val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
		val &= ~(WM8994_DAC1L_TO_SPKMIXL_MASK);
		wm8994_write(codec, WM8994_SPEAKER_MIXER, val);

		/* Disable DAC1L to HPOUT1L, DAC1L to MIXOUTL */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
		val &= ~(WM8994_DAC1L_TO_HPOUT1L_MASK |
			WM8994_DAC1L_TO_MIXOUTL_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

		/* Disable DAC1R to HPOUT1R, DAC1R to MIXOUTR */
		val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
		val &= ~(WM8994_DAC1R_TO_HPOUT1R_MASK |
			WM8994_DAC1R_TO_MIXOUTR_MASK);
		wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);
		break;

	default:
		DEBUG_LOG_ERR("fmradio path[%d] is not invaild!\n", path);
		return;
		break;
	}
}

void wm8994_set_fmradio_input_active(struct snd_soc_codec *codec, int on)
{
	u16 val;

	DEBUG_LOG("on = %d", on);

	if (on) {
		/* Unmute IN2 */
		val = wm8994_read(codec, WM8994_LEFT_LINE_INPUT_3_4_VOLUME);
		val &= ~WM8994_IN2L_MUTE_MASK;
		val |= WM8994_IN2L_VU;
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_3_4_VOLUME, val);

		val = wm8994_read(codec, WM8994_RIGHT_LINE_INPUT_3_4_VOLUME);
		val &= ~WM8994_IN2R_MUTE_MASK;
		val |= WM8994_IN2R_VU;
		wm8994_write(codec, WM8994_RIGHT_LINE_INPUT_3_4_VOLUME, val);

		/* Enable IN2LN to IN2L, IN2RN to IN2R */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_2);
		val = (WM8994_IN2LN_TO_IN2L | WM8994_IN2RN_TO_IN2R);
		wm8994_write(codec, WM8994_INPUT_MIXER_2, val);

		/* Enable IN2L to MIXINL */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_3);
		val |= WM8994_IN2L_TO_MIXINL;
		wm8994_write(codec, WM8994_INPUT_MIXER_3, val);

		/* Enable IN2R to MIXINR */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_4);
		val |= WM8994_IN2R_TO_MIXINR;
		wm8994_write(codec, WM8994_INPUT_MIXER_4, val);

	} else {
		/* Mute IN2 */
		val = wm8994_read(codec, WM8994_LEFT_LINE_INPUT_3_4_VOLUME);
		val |= WM8994_IN2L_VU | WM8994_IN2L_MUTE;
		wm8994_write(codec, WM8994_LEFT_LINE_INPUT_3_4_VOLUME, val);

		val = wm8994_read(codec, WM8994_RIGHT_LINE_INPUT_3_4_VOLUME);
		val |= WM8994_IN2R_VU | WM8994_IN2R_MUTE;
		wm8994_write(codec, WM8994_RIGHT_LINE_INPUT_3_4_VOLUME, val);

		/* Disable IN2LN to IN2L, IN2RN to IN2R */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_2);
		val &= ~(WM8994_IN2LN_TO_IN2L | WM8994_IN2RN_TO_IN2R);
		wm8994_write(codec, WM8994_INPUT_MIXER_2, val);

		/* disable IN2L to MIXINL */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_3);
		val &= ~WM8994_IN2L_TO_MIXINL_MASK;
		wm8994_write(codec, WM8994_INPUT_MIXER_3, val);

		/* disable IN2R to MIXINR */
		val = wm8994_read(codec, WM8994_INPUT_MIXER_4);
		val &= ~WM8994_IN2R_TO_MIXINR_MASK;
		wm8994_write(codec, WM8994_INPUT_MIXER_4, val);
	}

}

void wm8994_set_fmradio_common(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	DEBUG_LOG("");

	/* Cross Talk (H/W requested) */
	wm8994_write(codec, 0x39, 0x8);

	/* GPIO settings */
	wm8994_write(codec, WM8994_GPIO_3, 0x0100);
	wm8994_write(codec, WM8994_GPIO_4, 0x0100);
	wm8994_write(codec, WM8994_GPIO_5, 0x8100);
	wm8994_write(codec, WM8994_GPIO_6, 0xA101);
	wm8994_write(codec, WM8994_GPIO_7, 0x0100);

	/* Disable AIF1 timesolot0 EQ gain */
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_1, 0x0000);

	/* Enable AIF1CLK */
	val = wm8994_read(codec, WM8994_AIF1_CLOCKING_1);
	val |= WM8994_AIF1CLK_ENA;
	wm8994_write(codec, WM8994_AIF1_CLOCKING_1, val);

	/* AIF2CLK : FLL1 */
	val = wm8994_read(codec, WM8994_AIF2_CLOCKING_1);
	val &= ~WM8994_AIF2CLK_SRC_MASK;
	val |= (WM8994_AIF2CLK_ENA | 0x2 << WM8994_AIF2CLK_SRC_SHIFT);
	wm8994_write(codec, WM8994_AIF2_CLOCKING_1, val);

	/* Enable AIF1, AIF2 processing clock, digital mixing processor clock
	 * SYSCLK source is AIF1CLK
	 */
	val = wm8994_read(codec, WM8994_CLOCKING_1);
	val &= ~WM8994_SYSCLK_SRC_MASK;
	val |= (WM8994_DSP_FS1CLK_ENA | WM8994_DSP_FS2CLK_ENA |
		WM8994_DSP_FSINTCLK_ENA);
	wm8994_write(codec, WM8994_CLOCKING_1, val);

	/* Disable oversampling */
	wm8994_write(codec, WM8994_OVERSAMPLING, 0x0000);

	/* AIF2 Master, Forces CLK to be enabled
	 * when all AIF2 audio channels are disabled
	 */
	val = wm8994_read(codec, WM8994_AIF2_MASTER_SLAVE);
	val |= (WM8994_AIF2_LRCLK_FRC | WM8994_AIF2_CLK_FRC | WM8994_AIF2_MSTR);
	wm8994_write(codec, WM8994_AIF2_MASTER_SLAVE, val);

	/* DAC1L(601h:0x05) settings */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING);
	val &= ~WM8994_AIF1DAC2L_TO_DAC1L_MASK;
	val |= (WM8994_AIF2DACL_TO_DAC1L | WM8994_AIF1DAC1L_TO_DAC1L);
	wm8994_write(codec, WM8994_DAC1_LEFT_MIXER_ROUTING, val);

	/* DAC1R(602h:0x05) settings */
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING);
	val &= ~WM8994_AIF1DAC2R_TO_DAC1R_MASK;
	val |= (WM8994_AIF1DAC1R_TO_DAC1R | WM8994_AIF2DACR_TO_DAC1R);
	wm8994_write(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING, val);

	/* DAC2 Mixer Vol : 0dB (-36dB ~ 0dB) */
	wm8994_write(codec, WM8994_DAC2_MIXER_VOLUMES, 0x018C);

	/* Enable AIF2 Loopback */
	val = wm8994_read(codec, WM8994_AIF2_CONTROL_2);
	val |= (WM8994_AIF2_LOOPBACK);
	wm8994_write(codec, WM8994_AIF2_CONTROL_2, val);

	/* DRC for Noise-gate (AIF2) */
	wm8994_write(codec, WM8994_AIF2_ADC_FILTERS, 0xF800);
	wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, 0x0036);
	wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_2, 0x0010);
	wm8994_write(codec, WM8994_AIF2_DRC_2, 0x0840);
	wm8994_write(codec, WM8994_AIF2_DRC_3, 0x2400);
	wm8994_write(codec, WM8994_AIF2_DRC_4, 0x0000);
	wm8994_write(codec, WM8994_AIF2_DRC_5, 0x0000);
	wm8994_write(codec, WM8994_AIF2_DRC_1, 0x019C);

	if (wm8994->fmr_mix_path != FMR_DUAL_MIX)
		wm8994_set_fmradio_input_active(codec, 1);

	/* Enable ADC1 to DAC2L */
	val = wm8994_read(codec, WM8994_DAC2_LEFT_MIXER_ROUTING);
	val |= (WM8994_ADC1_TO_DAC2L);
	wm8994_write(codec, WM8994_DAC2_LEFT_MIXER_ROUTING, val);

	/* Enable ADC2 to DAC2R */
	val = wm8994_read(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING);
	val |= (WM8994_ADC2_TO_DAC2R);
	wm8994_write(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING, val);
}

void wm8994_set_fmradio_headset(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	DEBUG_LOG("Routing FM Radio: Headset");

	wm8994->fmradio_path = FMR_HP;

	wm8994_disable_fmradio_path(codec, FMR_SPK);

	/* Enable HPF, cut-off freq is 370Hz */
	wm8994_write(codec, WM8994_SIDETONE, 0x01C0);

	/* enable power in analog domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~WM8994_VMID_SEL_MASK;
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL | WM8994_HPOUT1R_ENA |
		WM8994_HPOUT1L_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	wm8994_set_headset_common(codec, DCS_FMRADIO);

	/* enable power in analog domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_2);
	val |= (WM8994_MIXINL_ENA | WM8994_MIXINR_ENA | WM8994_IN2L_ENA |
		WM8994_IN2R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2 , val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
	val |= (WM8994_MIXOUTLVOL_ENA | WM8994_MIXOUTRVOL_ENA |
		WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

	/* enable power in digital domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
	val |= (WM8994_AIF2ADCL_ENA | WM8994_AIF2ADCR_ENA | WM8994_ADCL_ENA |
		WM8994_ADCR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val |= (WM8994_AIF2DACL_ENA | WM8994_AIF2DACR_ENA |
		WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* intermediate HP settings */
	val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
	val |= (WM8994_HPOUT1L_RMV_SHORT | WM8994_HPOUT1L_OUTP |
		WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_RMV_SHORT |
		WM8994_HPOUT1R_OUTP | WM8994_HPOUT1R_DLY);
	wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

	wm8994_set_fmradio_common(codec);

	wm8994_set_codec_gain(codec, FMRADIO_MODE, FMRADIO_HP);

	/* enable DAC1 to MIXOUT */
	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val |= (WM8994_DAC1L_TO_MIXOUTL);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val |= (WM8994_DAC1R_TO_MIXOUTR);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	/* soft unmute settings */
	wm8994_write(codec, WM8994_DAC_SOFTMUTE, WM8994_DAC_SOFTMUTEMODE |
		WM8994_DAC_MUTERATE);

	/* unmute MIXOUT */
	val = wm8994_read(codec, WM8994_LEFT_OPGA_VOLUME);
	val |= WM8994_MIXOUT_VU | WM8994_MIXOUTL_MUTE_N;
	wm8994_write(codec, WM8994_LEFT_OPGA_VOLUME, val);

	val = wm8994_read(codec, WM8994_RIGHT_OPGA_VOLUME);
	val |= WM8994_MIXOUT_VU | WM8994_MIXOUTR_MUTE_N;
	wm8994_write(codec, WM8994_RIGHT_OPGA_VOLUME, val);

	/* unmute DAC1 */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
	val &= ~WM8994_DAC1L_MUTE_MASK;
	val |= WM8994_DAC1_VU;
	wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, val);

	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
	val &= ~WM8994_DAC1R_MUTE_MASK;
	val |= WM8994_DAC1_VU;
	wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, val);

	/* unmute DAC2 */
	val = wm8994_read(codec, WM8994_DAC2_LEFT_VOLUME);
	val &= ~WM8994_DAC2L_MUTE_MASK;
	val |= WM8994_DAC2_VU;
	wm8994_write(codec, WM8994_DAC2_LEFT_VOLUME, val);

	val = wm8994_read(codec, WM8994_DAC2_RIGHT_VOLUME);
	val &= ~WM8994_DAC2R_MUTE_MASK;
	val |= WM8994_DAC2_VU;
	wm8994_write(codec, WM8994_DAC2_RIGHT_VOLUME, val);

	/* Unmute AIF1DAC1 */
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, 0x0000);

	/* Unmute AIF2DAC */
	val = wm8994_read(codec, WM8994_AIF2_DAC_FILTERS_1);
	val &= ~(WM8994_AIF2DAC_MUTE_MASK);
	val |= WM8994_AIF2DAC_UNMUTE_RAMP | WM8994_AIF2DAC_MUTERATE;
	wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val);

}

void wm8994_set_fmradio_speaker(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	DEBUG_LOG("Routing FM Radio: Speaker");

	wm8994_disable_fmradio_path(codec, FMR_HP);

	wm8994->fmradio_path = FMR_SPK;

	/* enable power in analog domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~WM8994_VMID_SEL_MASK;
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL | WM8994_SPKOUTL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_2);
	val |= (WM8994_MIXINL_ENA | WM8994_IN2L_ENA |
		WM8994_MIXINR_ENA | WM8994_IN2R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2 , val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
	val |= WM8994_SPKLVOL_ENA;
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

	/* enable power in digital domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
	val |= (WM8994_AIF2ADCL_ENA | WM8994_ADCL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val |= (WM8994_AIF2DACL_ENA | WM8994_AIF1DAC1L_ENA |
		WM8994_DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1R_ENA | WM8994_AIF2DACR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* Enable themal sensor, OPCLK */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_2);
	val |= (WM8994_TSHUT_ENA | WM8994_TSHUT_OPDIS | WM8994_OPCLK_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2, val);

	/* Unmute SPKOUTL */
	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_LEFT);
	val |= WM8994_SPKOUT_VU | WM8994_SPKOUTL_MUTE_N;
	wm8994_write(codec, WM8994_SPEAKER_VOLUME_LEFT, val);

	wm8994_set_fmradio_common(codec);

	wm8994_set_codec_gain(codec, FMRADIO_MODE, FMRADIO_SPK);

	/* Enable SPKMIXL to SPKOUTL */
	val = wm8994_read(codec, WM8994_SPKOUT_MIXERS);
	val &= ~(WM8994_SPKMIXR_TO_SPKOUTL_MASK |
		WM8994_SPKMIXR_TO_SPKOUTR_MASK);
	val |= (WM8994_SPKMIXL_TO_SPKOUTL);
	wm8994_write(codec, WM8994_SPKOUT_MIXERS, val);

	/* Enable DAC1L to SPKMIXL */
	val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
	val &= ~(WM8994_MIXINL_TO_SPKMIXL_MASK | WM8994_MIXINR_TO_SPKMIXR_MASK |
		WM8994_DAC1R_TO_SPKMIXR_MASK);
	val |= (WM8994_DAC1L_TO_SPKMIXL);
	wm8994_write(codec, WM8994_SPEAKER_MIXER, val);

	/* Soft unmute settings */
	wm8994_write(codec, WM8994_DAC_SOFTMUTE, WM8994_DAC_SOFTMUTEMODE |
		WM8994_DAC_MUTERATE);

	/* Unmute DAC1L */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
	val &= ~WM8994_DAC1L_MUTE_MASK;
	val |= WM8994_DAC1_VU;
	wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, val);

	/* Unmute DAC1R */
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
	val &= ~WM8994_DAC1R_MUTE_MASK;
	val |= WM8994_DAC1_VU;
	wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, val);

	/* Unmute DAC2L */
	val = wm8994_read(codec, WM8994_DAC2_LEFT_VOLUME);
	val &= ~WM8994_DAC2L_MUTE_MASK;
	val |= WM8994_DAC2_VU;
	wm8994_write(codec, WM8994_DAC2_LEFT_VOLUME, val);

	/* Unmute DAC2R */
	val = wm8994_read(codec, WM8994_DAC2_RIGHT_VOLUME);
	val &= ~WM8994_DAC2R_MUTE_MASK;
	val |= WM8994_DAC2_VU;
	wm8994_write(codec, WM8994_DAC2_RIGHT_VOLUME, val);

	/* Unmute AIF2DAC */
	val = wm8994_read(codec, WM8994_AIF2_DAC_FILTERS_1);
	val &= ~(WM8994_AIF2DAC_MUTE_MASK);
	val |= WM8994_AIF2DAC_UNMUTE_RAMP | WM8994_AIF2DAC_MUTERATE;
	wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val);

	/* Unmute AIF1DAC1 */
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, 0x0000);

}

void wm8994_set_fmradio_speaker_headset_mix(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	DEBUG_LOG("");

	if (wm8994->fmradio_path == FMR_HP)
		wm8994_disable_fmradio_path(codec, FMR_HP);
	else
		wm8994_disable_fmradio_path(codec, FMR_SPK);

	wm8994->fmradio_path = FMR_DUAL_MIX;

	/* enable power in analog domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~WM8994_VMID_SEL_MASK;
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL | WM8994_HPOUT1R_ENA |
		WM8994_HPOUT1L_ENA | WM8994_SPKOUTL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	wm8994_write(codec, 0x5D, 0x0002);

	wm8994_set_headset_common(codec, DCS_FMRADIO_SPK_HP);

	/* enable power in analog domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_2);
	val |= (WM8994_MIXINL_ENA | WM8994_MIXINR_ENA | WM8994_IN2L_ENA |
		WM8994_IN2R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2 , val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
	val &= ~WM8994_SPKRVOL_ENA_MASK;
	val |= (WM8994_MIXOUTLVOL_ENA | WM8994_MIXOUTRVOL_ENA |
		WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA | WM8994_SPKLVOL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

	/* enable power in digital domain */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
	val |= (WM8994_AIF2ADCL_ENA | WM8994_AIF2ADCR_ENA | WM8994_ADCL_ENA |
		WM8994_ADCR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val |= (WM8994_AIF2DACL_ENA | WM8994_AIF2DACR_ENA |
		WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* intermediate HP settings */
	val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
	val &= ~(WM8994_HPOUT1R_DLY_MASK | WM8994_HPOUT1R_OUTP_MASK |
		WM8994_HPOUT1R_RMV_SHORT_MASK |	WM8994_HPOUT1L_DLY_MASK |
		WM8994_HPOUT1L_OUTP_MASK | WM8994_HPOUT1L_RMV_SHORT_MASK);
	val |= (WM8994_HPOUT1L_RMV_SHORT | WM8994_HPOUT1L_OUTP |
		WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_RMV_SHORT |
		WM8994_HPOUT1R_OUTP | WM8994_HPOUT1R_DLY);
	wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

	wm8994_set_fmradio_common(codec);

	wm8994_set_codec_gain(codec, FMRADIO_MODE, FMRADIO_SPK_HP);

	/* DRC for Noise-gate (AIF2) */
	wm8994_write(codec, 0x541, 0x0850);
	wm8994_write(codec, 0x542, 0x0800);
	wm8994_write(codec, 0x543, 0x0001);
	wm8994_write(codec, 0x544, 0x0008);
	wm8994_write(codec, 0x540, 0x01BC);

	/* enable DAC1 to MIXOUT */
	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val |= (WM8994_DAC1L_TO_MIXOUTL);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val |= (WM8994_DAC1R_TO_MIXOUTR);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	/* enable DAC1L to SPKMIXL */
	val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
	val |= WM8994_DAC1L_TO_SPKMIXL;
	wm8994_write(codec, WM8994_SPEAKER_MIXER, val);

	/* enable SPKMIXL to SPKOUTL */
	val = wm8994_read(codec, WM8994_SPKOUT_MIXERS);
	val &= ~(WM8994_SPKMIXR_TO_SPKOUTL_MASK |
		WM8994_SPKMIXR_TO_SPKOUTR_MASK);
	val |= WM8994_SPKMIXL_TO_SPKOUTL;
	wm8994_write(codec, WM8994_SPKOUT_MIXERS, val);

	/* unmute MIXOUTL */
	val = wm8994_read(codec, WM8994_LEFT_OPGA_VOLUME);
	val |= WM8994_MIXOUT_VU | WM8994_MIXOUTL_MUTE_N;
	wm8994_write(codec, WM8994_LEFT_OPGA_VOLUME, val);

	/* unmute MIXOUTR */
	val = wm8994_read(codec, WM8994_RIGHT_OPGA_VOLUME);
	val |= WM8994_MIXOUT_VU | WM8994_MIXOUTR_MUTE_N;
	wm8994_write(codec, WM8994_RIGHT_OPGA_VOLUME, val);

	/* unmute SPKOUTL */
	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_LEFT);
	val |= WM8994_SPKOUT_VU | WM8994_SPKOUTL_MUTE_N;
	wm8994_write(codec, WM8994_SPEAKER_VOLUME_LEFT, val);

	/* soft unmute settings */
	wm8994_write(codec, WM8994_DAC_SOFTMUTE, WM8994_DAC_SOFTMUTEMODE |
		WM8994_DAC_MUTERATE);

	/* unmute DAC1 */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
	val &= ~WM8994_DAC1L_MUTE_MASK;
	val |= WM8994_DAC1_VU;
	wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, val);

	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
	val &= ~WM8994_DAC1R_MUTE_MASK;
	val |= WM8994_DAC1_VU;
	wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, val);

	/* unmute DAC2 */
	val = wm8994_read(codec, WM8994_DAC2_LEFT_VOLUME);
	val &= ~WM8994_DAC2L_MUTE_MASK;
	val |= WM8994_DAC2_VU;
	wm8994_write(codec, WM8994_DAC2_LEFT_VOLUME, val);

	val = wm8994_read(codec, WM8994_DAC2_RIGHT_VOLUME);
	val &= ~WM8994_DAC2R_MUTE_MASK;
	val |= WM8994_DAC2_VU;
	wm8994_write(codec, WM8994_DAC2_RIGHT_VOLUME, val);

	/* unmute AIF1DAC1 , mono mix */
	val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1);
	val &= ~WM8994_AIF1DAC1_MUTE_MASK;
	val |= WM8994_AIF1DAC1_MONO;
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val);

	/* unmute AIF2DAC */
	val = wm8994_read(codec, WM8994_AIF2_DAC_FILTERS_1);
	val &= ~WM8994_AIF2DAC_MUTE_MASK;
	wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val);

}
int wm8994_set_codec_gain(struct snd_soc_codec *codec, u16 mode, u16 device)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	struct gain_info_t *default_gain_table_p = NULL;
	u32 gain_set_bits = COMMON_SET_BIT;
	u16 val;
	int table_num = 0;
	int i;

	if (wm8994->testmode_config_flag) {
		pr_info("%s: Testmode: skip to set codec gain\n", __func__);
		return 0;
	}

	if (mode == PLAYBACK_MODE) {
		default_gain_table_p = playback_gain_table;
		table_num = PLAYBACK_GAIN_NUM;

		switch (device) {
		case PLAYBACK_RCV:
			gain_set_bits |= PLAYBACK_RCV;
			break;
		case PLAYBACK_SPK:
			gain_set_bits |= PLAYBACK_SPK;
			break;
		case PLAYBACK_HP:
			gain_set_bits |= PLAYBACK_HP;
			break;
		case PLAYBACK_HP_NO_MIC:
			gain_set_bits |= PLAYBACK_HP_NO_MIC;
			break;
		case PLAYBACK_BT:
			gain_set_bits |= PLAYBACK_BT;
			break;
		case PLAYBACK_SPK_HP:
			gain_set_bits |= PLAYBACK_SPK_HP;
			break;
		case PLAYBACK_EXTRA_DOCK:
			gain_set_bits |= PLAYBACK_EXTRA_DOCK;
			break;
		case PLAYBACK_RING_SPK:
			gain_set_bits |= (PLAYBACK_SPK | PLAYBACK_RING_SPK);
			break;
		case PLAYBACK_RING_HP:
			gain_set_bits |= (PLAYBACK_HP | PLAYBACK_RING_HP);
			break;
		case PLAYBACK_RING_SPK_HP:
			gain_set_bits |= (PLAYBACK_SPK_HP |
					PLAYBACK_RING_SPK_HP);
			break;
		case PLAYBACK_VOIP_RCV:
			gain_set_bits |= (PLAYBACK_RCV | PLAYBACK_VOIP_RCV);
			break;
		case PLAYBACK_VOIP_SPK:
			gain_set_bits |= (PLAYBACK_SPK | PLAYBACK_VOIP_SPK);
			break;
		case PLAYBACK_VOIP_HP:
			gain_set_bits |= (PLAYBACK_HP |	PLAYBACK_VOIP_HP);
			break;
		case PLAYBACK_VOIP_BT:
			gain_set_bits |= (PLAYBACK_BT |	PLAYBACK_VOIP_BT);
			break;
		default:
			pr_err("%s: playback gain flag is wrong\n", __func__);
			break;
		}
	} else if (mode == VOICECALL_MODE) {
		default_gain_table_p = voicecall_gain_table;
		table_num = VOICECALL_GAIN_NUM;

		switch (device) {
		case VOICECALL_RCV:
			gain_set_bits |= VOICECALL_RCV;
			break;
		case VOICECALL_SPK:
			gain_set_bits |= VOICECALL_SPK;
			break;
		case VOICECALL_HP:
			gain_set_bits |= VOICECALL_HP;
			break;
		case VOICECALL_HP_NO_MIC:
			gain_set_bits |= VOICECALL_HP_NO_MIC;
			break;
		case VOICECALL_BT:
			gain_set_bits |= VOICECALL_BT;
			break;
		default:
			pr_err("%s: voicecall gain flag is wrong\n", __func__);
		}
	} else if (mode  == RECORDING_MODE) {
		default_gain_table_p = recording_gain_table;
		table_num = RECORDING_GAIN_NUM;

		switch (device) {
		case RECORDING_MAIN:
			gain_set_bits |= RECORDING_MAIN;
			break;
		case RECORDING_HP:
			gain_set_bits |= RECORDING_HP;
			break;
		case RECORDING_BT:
			gain_set_bits |= RECORDING_BT;
			break;
		case RECORDING_REC_MAIN:
			gain_set_bits |= RECORDING_REC_MAIN;
			break;
		case RECORDING_REC_HP:
			gain_set_bits |= RECORDING_REC_HP;
			break;
		case RECORDING_REC_BT:
			gain_set_bits |= RECORDING_REC_BT;
			break;
		case RECORDING_CAM_MAIN:
			gain_set_bits |= RECORDING_CAM_MAIN;
			break;
		case RECORDING_CAM_HP:
			gain_set_bits |= RECORDING_CAM_HP;
			break;
		case RECORDING_CAM_BT:
			gain_set_bits |= RECORDING_CAM_BT;
			break;
		case RECORDING_VOIP_MAIN:
			gain_set_bits |= RECORDING_VOIP_MAIN;
			break;
		case RECORDING_VOIP_HP:
			gain_set_bits |= RECORDING_VOIP_HP;
			break;
		case RECORDING_VOIP_BT:
			gain_set_bits |= RECORDING_VOIP_BT;
			break;
		default:
			pr_err("%s: recording gain flag is wrong\n", __func__);
		}

	} else if (mode == FMRADIO_MODE) {
		default_gain_table_p = fmradio_gain_table;
		table_num = FMRADIO_GAIN_NUM;

		switch (device) {
		case FMRADIO_HP:
			gain_set_bits |= FMRADIO_HP;
			break;
		case FMRADIO_SPK:
			gain_set_bits |= FMRADIO_SPK;
			break;
		case FMRADIO_SPK_HP:
			gain_set_bits |= FMRADIO_SPK_HP;
			break;
		default:
			pr_err("%s: fmradio gain flag is wrong\n", __func__);
		}
	}

	DEBUG_LOG("Set gain mode = 0x%x, device = 0x%x, gain_bits = 0x%x,\
		table_num=%d\n",
		mode, device, gain_set_bits, table_num);

	/* default gain table setting */
	for (i = 0; i < table_num; i++) {
		if ((default_gain_table_p + i)->mode & gain_set_bits) {
			val = wm8994_read(codec,
				(default_gain_table_p + i)->reg);
			val &= ~((default_gain_table_p + i)->mask);
			val |= (default_gain_table_p + i)->gain;
			wm8994_write(codec, (default_gain_table_p + i)->reg,
				val);
		}
	}

	return 0;

		}


