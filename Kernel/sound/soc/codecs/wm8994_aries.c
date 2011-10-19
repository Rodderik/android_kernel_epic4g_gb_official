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

/*
 * Debug Feature
 */
#define SUBJECT "wm8994_crespo.c"

#ifdef FEATURE_SS_AUDIO_CAL
extern unsigned int tty_mode;
extern unsigned int loopback_mode;
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

struct gain_info_t gain_code_table[GAIN_CODE_NUM] = {
	/* Playback */
	{/* HP */
		.mode = PLAYBACK_HP | PLAYBACK_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x31		/* -8dB */
	}, {
		.mode = PLAYBACK_HP | PLAYBACK_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x31		/* -8dB */
	}, {/* HP_NO_MIC */
		.mode = PLAYBACK_HP_NO_MIC | PLAYBACK_MODE | GAIN_DIVISION_BIT,
	        .reg  = WM8994_LEFT_OUTPUT_VOLUME,  /* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x31	 /* -8dB */
	}, {
		.mode = PLAYBACK_HP_NO_MIC | PLAYBACK_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME, /* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x31	 /* -8dB */
	},	{/* Voicecall RCV */
		.mode = VOICECALL_RCV | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x14		/* +13.5dB */
	}, {/* SPK */
		.mode = VOICECALL_SPK | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x0D		/* +3dB */
	}, {
		.mode = VOICECALL_SPK | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_SPEAKER_VOLUME_LEFT,	/* 26h */
		.mask = WM8994_SPKOUTL_VOL_MASK,
		.gain = WM8994_SPKOUT_VU | 0x3A		/* +1dB */
	}, {/* HP */
		.mode = VOICECALL_HP | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_RIGHT_LINE_INPUT_1_2_VOLUME,	/* 1Ah */
		.mask = WM8994_IN1R_VOL_MASK,
		.gain = WM8994_IN1R_VU | 0x1D		/* +27dB */
	}, {
		.mode = VOICECALL_HP | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3a		/* +1dB */
	}, {
		.mode = VOICECALL_HP | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3a		/* +1dB */
	}, {/* HP_NO_MIC */
		.mode = VOICECALL_HP_NO_MIC | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_LEFT_LINE_INPUT_1_2_VOLUME,	/* 18h */
		.mask = WM8994_IN1L_VOL_MASK,
		.gain = WM8994_IN1L_VU | 0x12	/* +10.5dB */
	}, {
		.mode = VOICECALL_HP_NO_MIC | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_LEFT_OUTPUT_VOLUME,	/* 1Ch */
		.mask = WM8994_HPOUT1L_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3a		/* +1dB */
	}, {
		.mode = VOICECALL_HP_NO_MIC | VOICECALL_MODE | GAIN_DIVISION_BIT,
		.reg  = WM8994_RIGHT_OUTPUT_VOLUME,	/* 1Dh */
		.mask = WM8994_HPOUT1R_VOL_MASK,
		.gain = WM8994_HPOUT1_VU | 0x3a		/* +1dB */
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
	DEBUG_LOG("enable = [%d]", enable);

	if (!pdata)
		pr_err("failed to turn off micbias pin\n");
	else {
		if (enable)
			pdata->set_mic_bias(true);
		else
			pdata->set_mic_bias(false);
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

	DEBUG_LOG("Recording through Headset Mic\n");

	wm8994_write(codec, WM8994_ANTIPOP_2, 0x68);

	/* Enable high pass filter to control bounce on startup */
	val = wm8994_read(codec, WM8994_AIF1_ADC1_FILTERS);
	val &= ~(WM8994_AIF1ADC1L_HPF_MASK | WM8994_AIF1ADC1R_HPF_MASK);
	val |= (WM8994_AIF1ADC1R_HPF);
	wm8994_write(codec, WM8994_AIF1_ADC1_FILTERS, val);

	/* Enable mic bias, vmid, bias generator */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_1);
	val &= ~(WM8994_INPUTS_CLAMP_MASK);
	val |= (WM8994_INPUTS_CLAMP);
	wm8994_write(codec, WM8994_INPUT_MIXER_1, val);

	val = (WM8994_MIXINR_ENA | WM8994_IN1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2, val);


	val = (WM8994_IN1RN_TO_IN1R | WM8994_IN1RP_TO_IN1R);
	wm8994_write(codec, WM8994_INPUT_MIXER_2, val);

	val = wm8994_read(codec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME);
	val &= ~(WM8994_IN1R_MUTE_MASK);
	wm8994_write(codec, WM8994_RIGHT_LINE_INPUT_1_2_VOLUME, val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_4);
	val &= ~(WM8994_IN1R_TO_MIXINR_MASK);
	val |= (WM8994_IN1R_TO_MIXINR);
	wm8994_write(codec, WM8994_INPUT_MIXER_4 , val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_1);
	val &= ~(WM8994_INPUTS_CLAMP_MASK);
	wm8994_write(codec, WM8994_INPUT_MIXER_1, val);

	val = wm8994_read(codec, WM8994_AIF1_ADC1_RIGHT_VOLUME);
	val |= (WM8994_AIF1ADC1_VU);
	wm8994_write(codec, WM8994_AIF1_ADC1_RIGHT_VOLUME, val);

	val = wm8994_read(codec, WM8994_AIF1_ADC1_FILTERS);
	val &= ~(WM8994_AIF1ADC1L_HPF_MASK | WM8994_AIF1ADC1R_HPF_MASK);
	val |= (WM8994_AIF1ADC1R_HPF | 0x2000);
	wm8994_write(codec, WM8994_AIF1_ADC1_FILTERS, val);

	val = wm8994_read(codec, WM8994_AIF1_MASTER_SLAVE);
	val |= (WM8994_AIF1_MSTR | WM8994_AIF1_CLK_FRC | WM8994_AIF1_LRCLK_FRC);
	wm8994_write(codec, WM8994_AIF1_MASTER_SLAVE, val);

	wm8994_write(codec, WM8994_GPIO_1, 0xA101);

	/* Mixing left channel output to right channel */
	val = wm8994_read(codec, WM8994_AIF1_CONTROL_1);
	val &= ~(WM8994_AIF1ADCL_SRC_MASK | WM8994_AIF1ADCR_SRC_MASK);
	val |= (WM8994_AIF1ADCL_SRC | WM8994_AIF1ADCR_SRC);
	wm8994_write(codec, WM8994_AIF1_CONTROL_1, val);

	/* Digital Paths  */
	/* Enable right ADC and time slot */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
	val &= ~(WM8994_ADCR_ENA_MASK | WM8994_AIF1ADC1R_ENA_MASK);
	val |= (WM8994_AIF1ADC1R_ENA | WM8994_ADCR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);


	/* ADC Right mixer routing */
	val = wm8994_read(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_ADC1R_TO_AIF1ADC1R_MASK);
	val |= WM8994_ADC1R_TO_AIF1ADC1R;
	wm8994_write(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
	val &= ~WM8994_MIXINL_TO_SPKMIXL_MASK;
	wm8994_write(codec, WM8994_SPEAKER_MIXER, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val &= ~WM8994_MIXINL_TO_MIXOUTL_MASK;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val &= ~WM8994_MIXINR_TO_MIXOUTR_MASK;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	val = wm8994_read(codec, WM8994_DAC2_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_ADC1_TO_DAC2L_MASK);
	wm8994_write(codec, WM8994_DAC2_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_ADC1_TO_DAC2R_MASK);
	wm8994_write(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING, val);

	if (wm8994->input_source == RECOGNITION)
		wm8994_set_codec_gain(codec, RECORDING_MODE, RECORDING_REC_HP);
	else if (wm8994->input_source == CAMCORDER)
		wm8994_set_codec_gain(codec, RECORDING_MODE, RECORDING_CAM_HP);
	else
		wm8994_set_codec_gain(codec, RECORDING_MODE, RECORDING_HP);

}

void wm8994_record_main_mic(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	DEBUG_LOG("Recording through Main Mic\n");
	audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);

	/* Main mic volume issue fix: requested H/W */
	wm8994_write(codec, WM8994_ANTIPOP_2, 0x68);

	/* High pass filter to control bounce on enable */
	val = wm8994_read(codec, WM8994_AIF1_ADC1_FILTERS);
	val &= ~(WM8994_AIF1ADC1L_HPF_MASK | WM8994_AIF1ADC1R_HPF_MASK);
	val |= (WM8994_AIF1ADC1L_HPF);
	wm8994_write(codec, WM8994_AIF1_ADC1_FILTERS, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_1);
	val &= ~(WM8994_INPUTS_CLAMP_MASK);
	val |= (WM8994_INPUTS_CLAMP);
	wm8994_write(codec, WM8994_INPUT_MIXER_1, val);

	val = (WM8994_MIXINL_ENA | WM8994_IN1L_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2, val);

	val = (WM8994_IN1LP_TO_IN1L | WM8994_IN1LN_TO_IN1L);
	wm8994_write(codec, WM8994_INPUT_MIXER_2, val);


	val = wm8994_read(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME);
	val &= ~(WM8994_IN1L_MUTE_MASK);
	wm8994_write(codec, WM8994_LEFT_LINE_INPUT_1_2_VOLUME, val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_3);
	val &= ~(WM8994_IN1L_TO_MIXINL_MASK);
	val |= (WM8994_IN1L_TO_MIXINL);
	wm8994_write(codec, WM8994_INPUT_MIXER_3, val);

	val = wm8994_read(codec, WM8994_INPUT_MIXER_1);
	val &= ~(WM8994_INPUTS_CLAMP_MASK);
	wm8994_write(codec, WM8994_INPUT_MIXER_1, val);


	val = wm8994_read(codec, WM8994_AIF1_ADC1_LEFT_VOLUME);
	val |= (WM8994_AIF1ADC1_VU);
	wm8994_write(codec, WM8994_AIF1_ADC1_LEFT_VOLUME, val);

	val = wm8994_read(codec, WM8994_AIF1_ADC1_FILTERS);
	val &= ~(WM8994_AIF1ADC1L_HPF_MASK | WM8994_AIF1ADC1R_HPF_MASK);
	val |= (WM8994_AIF1ADC1L_HPF | 0x2000);
	wm8994_write(codec, WM8994_AIF1_ADC1_FILTERS, val);

	val = wm8994_read(codec, WM8994_AIF1_MASTER_SLAVE);
	val |= (WM8994_AIF1_MSTR | WM8994_AIF1_CLK_FRC | WM8994_AIF1_LRCLK_FRC);
	wm8994_write(codec, WM8994_AIF1_MASTER_SLAVE, val);

	wm8994_write(codec, WM8994_GPIO_1, 0xA101);

	val = wm8994_read(codec, WM8994_AIF1_CONTROL_1);
	val &= ~(WM8994_AIF1ADCL_SRC_MASK | WM8994_AIF1ADCR_SRC_MASK);
	val |= (WM8994_AIF1ADCR_SRC);
	wm8994_write(codec, WM8994_AIF1_CONTROL_1, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
	val &= ~(WM8994_ADCL_ENA_MASK | WM8994_AIF1ADC1L_ENA_MASK);
	val |= (WM8994_AIF1ADC1L_ENA | WM8994_ADCL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);

	/* Enable timeslots */
	val = wm8994_read(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING);
	val |= WM8994_ADC1L_TO_AIF1ADC1L;
	wm8994_write(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
	val &= ~WM8994_MIXINL_TO_SPKMIXL_MASK;
	wm8994_write(codec, WM8994_SPEAKER_MIXER, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val &= ~WM8994_MIXINL_TO_MIXOUTL_MASK;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val &= ~WM8994_MIXINR_TO_MIXOUTR_MASK;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	val = wm8994_read(codec, WM8994_DAC2_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_ADC1_TO_DAC2L_MASK);
	wm8994_write(codec, WM8994_DAC2_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_ADC1_TO_DAC2R_MASK);
	wm8994_write(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING, val);

	if (wm8994->input_source == RECOGNITION)
		wm8994_set_codec_gain(codec, RECORDING_MODE,
				RECORDING_REC_MAIN);
	else if (wm8994->input_source == CAMCORDER)
		wm8994_set_codec_gain(codec, RECORDING_MODE,
				RECORDING_CAM_MAIN);
	else
		wm8994_set_codec_gain(codec, RECORDING_MODE, RECORDING_MAIN);

}

void wm8994_record_bluetooth(struct snd_soc_codec *codec)
{
	u16 val;

	DEBUG_LOG("BT Record Path for Voice Command\n");

	wm8994_set_bluetooth_common_setting(codec);

	val = wm8994_read(codec, WM8994_DAC2_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_ADC1_TO_DAC2L_MASK);
	wm8994_write(codec, WM8994_DAC2_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_ADC1_TO_DAC2R_MASK);
	wm8994_write(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2, 0x0000);

	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, 0x0000);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
	val &= ~(WM8994_AIF1ADC1L_ENA_MASK | WM8994_AIF1ADC1R_ENA_MASK);
	val |= (WM8994_AIF1ADC1L_ENA | WM8994_AIF1ADC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_4 , val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_AIF2DACL_ENA_MASK | WM8994_AIF2DACR_ENA_MASK);
	val |= (WM8994_AIF2DACL_ENA | WM8994_AIF2DACR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_6);
	val &= ~(WM8994_AIF3_ADCDAT_SRC_MASK | WM8994_AIF2_DACDAT_SRC_MASK);
	val |= (0x1 << WM8994_AIF3_ADCDAT_SRC_SHIFT | WM8994_AIF2_DACDAT_SRC);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_6, val);

	val = wm8994_read(codec, WM8994_AIF2_DAC_FILTERS_1);
	val &= ~(WM8994_AIF2DAC_MUTE_MASK);
	wm8994_write(codec, WM8994_AIF2_DAC_FILTERS_1, val);

	val = wm8994_read(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF2DACL_TO_AIF1ADC1L_MASK);
	val |= (WM8994_AIF2DACL_TO_AIF1ADC1L);
	wm8994_write(codec, WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_AIF2DACR_TO_AIF1ADC1R_MASK);
	val |= (WM8994_AIF2DACR_TO_AIF1ADC1R);
	wm8994_write(codec, WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING, val);

	wm8994_write(codec, WM8994_AIF2_CLOCKING_1, 0x0019);

	wm8994_write(codec, WM8994_OVERSAMPLING, 0X0000);

	wm8994_write(codec, WM8994_GPIO_8, WM8994_GP8_DIR | WM8994_GP8_DB);
	wm8994_write(codec, WM8994_GPIO_9, WM8994_GP9_DB);
	wm8994_write(codec, WM8994_GPIO_10, WM8994_GP10_DB);
	wm8994_write(codec, WM8994_GPIO_11, WM8994_GP11_DB);
}
void wm8994_set_playback_receiver(struct snd_soc_codec *codec)
{
	u16 val;

	DEBUG_LOG("");

	val = wm8994_read(codec, WM8994_LEFT_OPGA_VOLUME);
	val &= ~(WM8994_MIXOUTL_MUTE_N_MASK);
	val |= (WM8994_MIXOUTL_MUTE_N);
	wm8994_write(codec, WM8994_LEFT_OPGA_VOLUME, val);

	val = wm8994_read(codec, WM8994_RIGHT_OPGA_VOLUME);
	val &= ~(WM8994_MIXOUTR_MUTE_N_MASK);
	val |= (WM8994_MIXOUTR_MUTE_N);
	wm8994_write(codec, WM8994_RIGHT_OPGA_VOLUME, val);

	val = wm8994_read(codec, WM8994_HPOUT2_VOLUME);
	val &= ~(WM8994_HPOUT2_MUTE_MASK);
	wm8994_write(codec, WM8994_HPOUT2_VOLUME, val);

	/* Unmute DAC1 left */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
	val &= ~(WM8994_DAC1L_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, val);

	/* Unmute and volume ctrl RightDAC */
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
	val &= ~(WM8994_DAC1R_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val &= ~(WM8994_DAC1L_TO_MIXOUTL_MASK);
	val |= (WM8994_DAC1L_TO_MIXOUTL);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val &= ~(WM8994_DAC1R_TO_MIXOUTR_MASK);
	val |= (WM8994_DAC1R_TO_MIXOUTR);
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	val = wm8994_read(codec, WM8994_HPOUT2_MIXER);
	val &= ~(WM8994_MIXOUTLVOL_TO_HPOUT2_MASK |
			WM8994_MIXOUTRVOL_TO_HPOUT2_MASK);
	val |= (WM8994_MIXOUTRVOL_TO_HPOUT2 | WM8994_MIXOUTLVOL_TO_HPOUT2);
	wm8994_write(codec, WM8994_HPOUT2_MIXER, val);

	wm8994_set_codec_gain(codec, PLAYBACK_MODE, PLAYBACK_RCV);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_DAC1R_ENA_MASK | WM8994_DAC1L_ENA_MASK |
			WM8994_AIF1DAC1R_ENA_MASK | WM8994_AIF1DAC1L_ENA_MASK);
	val |= (WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
			WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK);
	val |= (WM8994_AIF1DAC1L_TO_DAC1L);
	wm8994_write(codec, WM8994_DAC1_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1R_TO_DAC1R_MASK);
	val |= (WM8994_AIF1DAC1R_TO_DAC1R);
	wm8994_write(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_CLOCKING_1);
	val &= ~(WM8994_DSP_FS1CLK_ENA_MASK | WM8994_DSP_FSINTCLK_ENA_MASK);
	val |= (WM8994_DSP_FS1CLK_ENA | WM8994_DSP_FSINTCLK_ENA);
	wm8994_write(codec, WM8994_CLOCKING_1, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
	val &= ~(WM8994_MIXOUTLVOL_ENA_MASK | WM8994_MIXOUTRVOL_ENA_MASK |
	      WM8994_MIXOUTL_ENA_MASK | WM8994_MIXOUTR_ENA_MASK);
	val |= (WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA |
			WM8994_MIXOUTRVOL_ENA | WM8994_MIXOUTLVOL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK |
			WM8994_HPOUT2_ENA_MASK | WM8994_HPOUT1L_ENA_MASK |
			WM8994_HPOUT1R_ENA_MASK | WM8994_SPKOUTL_ENA_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL | WM8994_HPOUT2_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1);
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK | WM8994_AIF1DAC1_MONO_MASK);
	val |= (WM8994_AIF1DAC1_UNMUTE | WM8994_AIF1DAC1_MONO);
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val);

}

void wm8994_set_playback_headset(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	u16 testreturn1 = 0;
	u16 testreturn2 = 0;
	u16 testlow1 = 0;
	u16 testhigh1 = 0;
	u8 testlow = 0;
	u8 testhigh = 0;

	DEBUG_LOG("");

	wm8994_earsel_control(wm8994->pdata, 0);

	/* Enable the Timeslot0 to DAC1L */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK);
	val |= WM8994_AIF1DAC1L_TO_DAC1L;
	wm8994_write(codec, WM8994_DAC1_LEFT_MIXER_ROUTING, val);

	/* Enable the Timeslot0 to DAC1R */
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1R_TO_DAC1R_MASK);
	val |= WM8994_AIF1DAC1R_TO_DAC1R;
	wm8994_write(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING, val);

	val = wm8994_read(codec, 0x102);
	val &= ~(0x0003);
	val = 0x0003;
	wm8994_write(codec, 0x102, val);

	val = wm8994_read(codec, 0x56);
	val &= ~(0x0003);
	val = 0x0003;
	wm8994_write(codec, 0x56, val);

	val = wm8994_read(codec, 0x102);
	val &= ~(0x0000);
	val = 0x0000;
	wm8994_write(codec, 0x102, val);

	val = wm8994_read(codec, WM8994_CLASS_W_1);
	val &= ~(0x0005);
	val |= 0x0005;
	wm8994_write(codec, WM8994_CLASS_W_1, val);

	val = wm8994_read(codec, WM8994_LEFT_OUTPUT_VOLUME);
	val &= ~(WM8994_HPOUT1L_MUTE_N_MASK);
	val |= (WM8994_HPOUT1L_MUTE_N);
	wm8994_write(codec, WM8994_LEFT_OUTPUT_VOLUME, val);

	val = wm8994_read(codec, WM8994_RIGHT_OUTPUT_VOLUME);
	val &= ~(WM8994_HPOUT1R_MUTE_N_MASK);
	val |= (WM8994_HPOUT1R_MUTE_N);
	wm8994_write(codec, WM8994_RIGHT_OUTPUT_VOLUME, val);

	val = wm8994_read(codec, WM8994_LEFT_OPGA_VOLUME);
	val &= ~(WM8994_MIXOUTL_MUTE_N_MASK);
	val |= (WM8994_MIXOUTL_MUTE_N);
	wm8994_write(codec, WM8994_LEFT_OPGA_VOLUME, val);

	val = wm8994_read(codec, WM8994_RIGHT_OPGA_VOLUME);
	val &= ~(WM8994_MIXOUTR_MUTE_N_MASK);
	val |= (WM8994_MIXOUTR_MUTE_N);
	wm8994_write(codec, WM8994_RIGHT_OPGA_VOLUME, val);

	if (wm8994->ringtone_active)
		wm8994_set_codec_gain(codec, PLAYBACK_MODE, PLAYBACK_RING_HP);
	else if (wm8994->cur_path == HP_NO_MIC)
		wm8994_set_codec_gain(codec, PLAYBACK_MODE, PLAYBACK_HP_NO_MIC);
	else
		wm8994_set_codec_gain(codec, PLAYBACK_MODE, PLAYBACK_HP);

	val = wm8994_read(codec, WM8994_DC_SERVO_2);
	val &= ~(0x03E0);
	val = 0x03E0;
	wm8994_write(codec, WM8994_DC_SERVO_2, val);

	/* Enable vmid,bias, hp left and right */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK |
		WM8994_HPOUT1L_ENA_MASK | WM8994_HPOUT1R_ENA_MASK |
		WM8994_SPKOUTR_ENA_MASK | WM8994_SPKOUTL_ENA_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL |
		WM8994_HPOUT1R_ENA | WM8994_HPOUT1L_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
	val &= ~(0x0022);
	val = 0x0022;
	wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

	/* Enable Charge Pump */
	/* this is from wolfson */
	val = wm8994_read(codec, WM8994_CHARGE_PUMP_1);
	val &= ~WM8994_CP_ENA_MASK ;
	val |= WM8994_CP_ENA | WM8994_CP_ENA_DEFAULT;
	wm8994_write(codec, WM8994_CHARGE_PUMP_1, val);

	msleep(5);

	/* Enable Dac1 and DAC2 and the Timeslot0 for AIF1 */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_DAC1R_ENA_MASK | WM8994_DAC1L_ENA_MASK |
		WM8994_AIF1DAC1R_ENA_MASK | WM8994_AIF1DAC1L_ENA_MASK);
	val |= (WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* Enable DAC1L to HPOUT1L path */
	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val &=  ~(WM8994_DAC1L_TO_HPOUT1L_MASK | WM8994_DAC1L_TO_MIXOUTL_MASK);
	val |= WM8994_DAC1L_TO_MIXOUTL;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	/* Enable DAC1R to HPOUT1R path */
	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val &= ~(WM8994_DAC1R_TO_HPOUT1R_MASK | WM8994_DAC1R_TO_MIXOUTR_MASK);
	val |= WM8994_DAC1R_TO_MIXOUTR;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
	val &= ~(WM8994_MIXOUTLVOL_ENA_MASK | WM8994_MIXOUTRVOL_ENA_MASK |
		WM8994_MIXOUTL_ENA_MASK | WM8994_MIXOUTR_ENA_MASK |
		WM8994_SPKRVOL_ENA_MASK | WM8994_SPKLVOL_ENA_MASK);
	val |= (WM8994_MIXOUTLVOL_ENA | WM8994_MIXOUTRVOL_ENA |
		WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, 0x0030);

	if (!wm8994->dc_servo[DCS_MEDIA]) {
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
		testreturn2 = wm8994->dc_servo[DCS_MEDIA];
	}

	wm8994_write(codec, WM8994_DC_SERVO_4, testreturn2);
	wm8994->dc_servo[DCS_MEDIA] = testreturn2;

	wait_for_dc_servo(codec,
			  WM8994_DCS_TRIG_DAC_WR_0 | WM8994_DCS_TRIG_DAC_WR_1);

	/* Intermediate HP settings */
	val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
	val &= ~(WM8994_HPOUT1R_DLY_MASK | WM8994_HPOUT1R_OUTP_MASK |
		WM8994_HPOUT1R_RMV_SHORT_MASK | WM8994_HPOUT1L_DLY_MASK |
		WM8994_HPOUT1L_OUTP_MASK | WM8994_HPOUT1L_RMV_SHORT_MASK);
	val = (WM8994_HPOUT1L_RMV_SHORT | WM8994_HPOUT1L_OUTP|
		WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_RMV_SHORT |
		WM8994_HPOUT1R_OUTP | WM8994_HPOUT1R_DLY);
	wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

	/* Unmute DAC1 left */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
	val &= ~(WM8994_DAC1L_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, val);

	/* Unmute and volume ctrl RightDAC */
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
	val &= ~(WM8994_DAC1R_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, val);

	/* Unmute the AF1DAC1 */
	val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1);
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK | WM8994_AIF1DAC1_MONO_MASK);
	val |= WM8994_AIF1DAC1_UNMUTE;
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val);

}

void wm8994_set_playback_speaker(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	DEBUG_LOG("");

	/* Disable end point for preventing pop up noise.*/
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_SPKOUTL_ENA_MASK);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
	val &= ~(WM8994_MIXOUTLVOL_ENA_MASK | WM8994_MIXOUTRVOL_ENA_MASK |
		WM8994_MIXOUTL_ENA_MASK | WM8994_MIXOUTR_ENA_MASK |
		WM8994_SPKRVOL_ENA_MASK | WM8994_SPKLVOL_ENA_MASK);
	val |= WM8994_SPKLVOL_ENA;
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

	/* Speaker Volume Control */
	/* Unmute the SPKMIXVOLUME */
	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_LEFT);
	val &= ~(WM8994_SPKOUTL_MUTE_N_MASK);
	val |= (WM8994_SPKOUTL_MUTE_N);
	wm8994_write(codec, WM8994_SPEAKER_VOLUME_LEFT, val);

	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_RIGHT);
	val &= ~(WM8994_SPKOUTR_MUTE_N_MASK);
	wm8994_write(codec, WM8994_SPEAKER_VOLUME_RIGHT, val);

	/* Unmute DAC1 left */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
	val &= ~(WM8994_DAC1L_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, val);

	/* Unmute and volume ctrl RightDAC */
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
	val &= ~(WM8994_DAC1R_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, val);

	val = wm8994_read(codec, WM8994_SPKOUT_MIXERS);
	val &= ~(WM8994_SPKMIXL_TO_SPKOUTL_MASK |
		 WM8994_SPKMIXR_TO_SPKOUTL_MASK |
		 WM8994_SPKMIXR_TO_SPKOUTR_MASK);
	val |= WM8994_SPKMIXL_TO_SPKOUTL;
	wm8994_write(codec, WM8994_SPKOUT_MIXERS, val);

	/* Unmute the DAC path */
	val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
	val &= ~(WM8994_DAC1L_TO_SPKMIXL_MASK);
	val |= WM8994_DAC1L_TO_SPKMIXL;
	wm8994_write(codec, WM8994_SPEAKER_MIXER, val);

	/* Eable DAC1 Left and timeslot left */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_DAC1L_ENA_MASK | WM8994_AIF1DAC1R_ENA_MASK |
		WM8994_AIF1DAC1L_ENA_MASK);
	val |= (WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA | WM8994_DAC1L_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* DRC setting */
	wm8994_write(codec, WM8994_AIF1_DRC1_1, 0x00BC);
	wm8994_write(codec, WM8994_AIF1_DRC1_3, 0x0028);
	wm8994_write(codec, WM8994_AIF1_DRC1_4, 0x0186);

	/* EQ AIF1 setting */
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_1,   0x0019);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_GAINS_2,   0x6280);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_A,  0x0FC3);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_B,  0x03FD);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_1_PG, 0x00F4);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_A,  0x1F30);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_B,  0xF0CD);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_C,  0x040A);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_2_PG, 0x032C);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_A,  0x1C52);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_B,  0xF379);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_C,  0x040A);
	wm8994_write(codec, WM8994_AIF1_DAC1_EQ_BAND_3_PG, 0x0DC1);

	if (wm8994->ringtone_active)
		wm8994_set_codec_gain(codec, PLAYBACK_MODE, PLAYBACK_RING_SPK);
	else
		wm8994_set_codec_gain(codec, PLAYBACK_MODE, PLAYBACK_SPK);

	/* enable timeslot0 to left dac */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK);
	val |= WM8994_AIF1DAC1L_TO_DAC1L;
	wm8994_write(codec, WM8994_DAC1_LEFT_MIXER_ROUTING, val);

	/* Enbale bias,vmid and Left speaker */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK |
		WM8994_HPOUT1L_ENA_MASK | WM8994_HPOUT1R_ENA_MASK |
		WM8994_SPKOUTR_ENA_MASK | WM8994_SPKOUTL_ENA_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL | WM8994_SPKOUTL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	/* Unmute */
	val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1);
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK | WM8994_AIF1DAC1_MONO_MASK);
	val |= (WM8994_AIF1DAC1_UNMUTE | WM8994_AIF1DAC1_MONO);
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val);

}

void wm8994_set_playback_speaker_headset(struct snd_soc_codec *codec)
{
	struct wm8994_priv *wm8994 = codec->drvdata;

	u16 val;

	u16 nreadservo4val = 0;
	u16 ncompensationresult = 0;
	u16 ncompensationresultlow = 0;
	u16 ncompensationresulthigh = 0;
	u8  nservo4low = 0;
	u8  nservo4high = 0;

	wm8994_earsel_control(wm8994->pdata, 0);

	/* Enable the Timeslot0 to DAC1L */
	val = wm8994_read(codec, WM8994_DAC1_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1L_TO_DAC1L_MASK);
	val |= WM8994_AIF1DAC1L_TO_DAC1L;
	wm8994_write(codec, WM8994_DAC1_LEFT_MIXER_ROUTING, val);

	/* Enable the Timeslot0 to DAC1R */
	val = wm8994_read(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1R_TO_DAC1R_MASK);
	val |= WM8994_AIF1DAC1R_TO_DAC1R;
	wm8994_write(codec, WM8994_DAC1_RIGHT_MIXER_ROUTING, val);

	/* Speaker Volume Control */
	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_LEFT);
	val &= ~(WM8994_SPKOUTL_MUTE_N_MASK);
	val |= (WM8994_SPKOUTL_MUTE_N);
	wm8994_write(codec, WM8994_SPEAKER_VOLUME_LEFT, val);

	val = wm8994_read(codec, WM8994_SPEAKER_VOLUME_RIGHT);
	val &= ~(WM8994_SPKOUTR_MUTE_N_MASK);
	wm8994_write(codec, WM8994_SPEAKER_VOLUME_RIGHT, val);

	val = wm8994_read(codec, WM8994_SPKOUT_MIXERS);
	val &= ~(WM8994_SPKMIXL_TO_SPKOUTL_MASK |
		WM8994_SPKMIXR_TO_SPKOUTL_MASK |
		WM8994_SPKMIXR_TO_SPKOUTR_MASK);
	val |= WM8994_SPKMIXL_TO_SPKOUTL;
	wm8994_write(codec, WM8994_SPKOUT_MIXERS, val);

	/* Unmute the DAC path */
	val = wm8994_read(codec, WM8994_SPEAKER_MIXER);
	val &= ~(WM8994_DAC1L_TO_SPKMIXL_MASK);
	val |= WM8994_DAC1L_TO_SPKMIXL;
	wm8994_write(codec, WM8994_SPEAKER_MIXER, val);

	/* Configuring the Digital Paths */
	val = wm8994_read(codec, 0x102);
	val &= ~(0x0003);
	val = 0x0003;
	wm8994_write(codec, 0x102, val);

	val = wm8994_read(codec, 0x56);
	val &= ~(0x0003);
	val = 0x0003;
	wm8994_write(codec, 0x56, val);

	val = wm8994_read(codec, 0x102);
	val &= ~(0x0000);
	val = 0x0000;
	wm8994_write(codec, 0x102, val);

	val = wm8994_read(codec, WM8994_CLASS_W_1);
	val &= ~(0x0005);
	val = 0x0005;
	wm8994_write(codec, WM8994_CLASS_W_1, val);

	val = wm8994_read(codec, WM8994_LEFT_OUTPUT_VOLUME);
	val &= ~(WM8994_HPOUT1L_MUTE_N_MASK);
	val |= (WM8994_HPOUT1L_MUTE_N);
	wm8994_write(codec, WM8994_LEFT_OUTPUT_VOLUME, val);

	val = wm8994_read(codec, WM8994_RIGHT_OUTPUT_VOLUME);
	val &= ~(WM8994_HPOUT1R_MUTE_N_MASK);
	val |= (WM8994_HPOUT1R_MUTE_N);
	wm8994_write(codec, WM8994_RIGHT_OUTPUT_VOLUME, val);

	/* DC Servo Series Count */
	val = 0x03E0;
	wm8994_write(codec, WM8994_DC_SERVO_2, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK |
		WM8994_HPOUT1L_ENA_MASK | WM8994_HPOUT1R_ENA_MASK |
		WM8994_SPKOUTL_ENA_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL |
		WM8994_HPOUT1R_ENA | WM8994_HPOUT1L_ENA |
		WM8994_SPKOUTL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	val = (WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_DLY);
	wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

	/* Enable Charge Pump */
	/* this is from wolfson */
	val = wm8994_read(codec, WM8994_CHARGE_PUMP_1);
	val &= ~WM8994_CP_ENA_MASK ;
	val |= WM8994_CP_ENA | WM8994_CP_ENA_DEFAULT;
	wm8994_write(codec, WM8994_CHARGE_PUMP_1, val);

	msleep(5);

	/* Enable DAC1 and DAC2 and the Timeslot0 for AIF1 */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_DAC1R_ENA_MASK | WM8994_DAC1L_ENA_MASK |
		WM8994_AIF1DAC1R_ENA_MASK | WM8994_AIF1DAC1L_ENA_MASK);
	val |= (WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	/* Enbale DAC1L to HPOUT1L path */
	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_1);
	val &=  ~(WM8994_DAC1L_TO_HPOUT1L_MASK | WM8994_DAC1L_TO_MIXOUTL_MASK);
	val |=  WM8994_DAC1L_TO_MIXOUTL;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_1, val);

	/* Enbale DAC1R to HPOUT1R path */
	val = wm8994_read(codec, WM8994_OUTPUT_MIXER_2);
	val &= ~(WM8994_DAC1R_TO_HPOUT1R_MASK | WM8994_DAC1R_TO_MIXOUTR_MASK);
	val |= WM8994_DAC1R_TO_MIXOUTR;
	wm8994_write(codec, WM8994_OUTPUT_MIXER_2, val);

	/* Enbale bias,vmid, hp left and right and Left speaker */
	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_3);
	val &= ~(WM8994_MIXOUTLVOL_ENA_MASK | WM8994_MIXOUTRVOL_ENA_MASK |
		WM8994_MIXOUTL_ENA_MASK | WM8994_MIXOUTR_ENA_MASK |
		WM8994_SPKLVOL_ENA_MASK);
	val |= (WM8994_MIXOUTL_ENA | WM8994_MIXOUTR_ENA | WM8994_SPKLVOL_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, val);

	/* DC Servo */
	if (!wm8994->dc_servo[DCS_SPK_HP]) {
		wait_for_dc_servo(codec,
				  WM8994_DCS_TRIG_SERIES_0 |
				  WM8994_DCS_TRIG_SERIES_1);

		nreadservo4val = wm8994_read(codec, WM8994_DC_SERVO_4);
		nservo4low = (signed char)(nreadservo4val & 0xff);
		nservo4high = (signed char)((nreadservo4val>>8) & 0xff);

		ncompensationresultlow = ((signed short)nservo4low - 5)
			& 0x00ff;
		ncompensationresulthigh = ((signed short)(nservo4high - 5)<<8)
			& 0xff00;
		ncompensationresult = ncompensationresultlow |
			ncompensationresulthigh;
	} else {
		ncompensationresult = wm8994->dc_servo[DCS_SPK_HP];
	}

	wm8994_write(codec, WM8994_DC_SERVO_4, ncompensationresult);
	wm8994->dc_servo[DCS_SPK_HP] = ncompensationresult;

	wait_for_dc_servo(codec,
			  WM8994_DCS_TRIG_DAC_WR_1 | WM8994_DCS_TRIG_DAC_WR_0);

	val = wm8994_read(codec, WM8994_ANALOGUE_HP_1);
	val &= ~(WM8994_HPOUT1R_DLY_MASK | WM8994_HPOUT1R_OUTP_MASK |
		WM8994_HPOUT1R_RMV_SHORT_MASK |	WM8994_HPOUT1L_DLY_MASK |
		WM8994_HPOUT1L_OUTP_MASK | WM8994_HPOUT1L_RMV_SHORT_MASK);
	val |= (WM8994_HPOUT1L_RMV_SHORT | WM8994_HPOUT1L_OUTP |
		WM8994_HPOUT1L_DLY | WM8994_HPOUT1R_RMV_SHORT |
		WM8994_HPOUT1R_OUTP | WM8994_HPOUT1R_DLY);
	wm8994_write(codec, WM8994_ANALOGUE_HP_1, val);

	if (wm8994->ringtone_active)
		wm8994_set_codec_gain(codec, PLAYBACK_MODE,
				PLAYBACK_RING_SPK_HP);
	else
		wm8994_set_codec_gain(codec, PLAYBACK_MODE, PLAYBACK_SPK_HP);

	val = wm8994_read(codec, WM8994_DAC1_LEFT_VOLUME);
	val &= ~(WM8994_DAC1L_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_LEFT_VOLUME, val);

	val = wm8994_read(codec, WM8994_DAC1_RIGHT_VOLUME);
	val &= ~(WM8994_DAC1R_MUTE_MASK);
	wm8994_write(codec, WM8994_DAC1_RIGHT_VOLUME, val);

	val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1);
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK | WM8994_AIF1DAC1_MONO_MASK);
	val |= (WM8994_AIF1DAC1_UNMUTE | WM8994_AIF1DAC1_MONO);
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val);

}

void wm8994_set_playback_bluetooth(struct snd_soc_codec *codec)
{
	u16 val;

	DEBUG_LOG("BT Playback Path for SCO\n");

	wm8994_set_bluetooth_common_setting(codec);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_1);
	val &= ~(WM8994_BIAS_ENA_MASK | WM8994_VMID_SEL_MASK);
	val |= (WM8994_BIAS_ENA | WM8994_VMID_SEL_NORMAL);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_1, val);

	wm8994_write(codec, WM8994_POWER_MANAGEMENT_2, 0x0000);

	wm8994_write(codec, WM8994_POWER_MANAGEMENT_3, 0x0000);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_4);
	val &= ~(WM8994_AIF2ADCL_ENA_MASK | WM8994_AIF2ADCR_ENA_MASK);
	val |= (WM8994_AIF2ADCL_ENA | WM8994_AIF2ADCR_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_4, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_5);
	val &= ~(WM8994_AIF2DACL_ENA_MASK | WM8994_AIF2DACR_ENA_MASK |
		WM8994_AIF1DAC1L_ENA_MASK | WM8994_AIF1DAC1R_ENA_MASK |
		WM8994_DAC1L_ENA_MASK | WM8994_DAC1R_ENA_MASK);
	val |= (WM8994_AIF2DACL_ENA | WM8994_AIF2DACR_ENA |
		WM8994_AIF1DAC1L_ENA | WM8994_AIF1DAC1R_ENA |
		WM8994_DAC1L_ENA | WM8994_DAC1R_ENA);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_5, val);

	val = wm8994_read(codec, WM8994_POWER_MANAGEMENT_6);
	val &= ~(WM8994_AIF3_ADCDAT_SRC_MASK);
	val |= (0x0001 << WM8994_AIF3_ADCDAT_SRC_SHIFT);
	wm8994_write(codec, WM8994_POWER_MANAGEMENT_6, val);

	/* Mixer Routing*/
	val = wm8994_read(codec, WM8994_DAC2_LEFT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1L_TO_DAC2L_MASK);
	val |= (WM8994_AIF1DAC1L_TO_DAC2L);
	wm8994_write(codec, WM8994_DAC2_LEFT_MIXER_ROUTING, val);

	val = wm8994_read(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING);
	val &= ~(WM8994_AIF1DAC1R_TO_DAC2R_MASK);
	val |= (WM8994_AIF1DAC1R_TO_DAC2R);
	wm8994_write(codec, WM8994_DAC2_RIGHT_MIXER_ROUTING, val);

	/* Volume*/
	wm8994_write(codec, WM8994_DAC2_LEFT_VOLUME, 0x01C0);
	wm8994_write(codec, WM8994_DAC2_RIGHT_VOLUME, 0x01C0);

	wm8994_write(codec, WM8994_AIF2_CLOCKING_1, 0x0019);

	wm8994_write(codec, WM8994_OVERSAMPLING, 0X0000);

	/* GPIO Configuration*/
	wm8994_write(codec, WM8994_GPIO_8, WM8994_GP8_DIR | WM8994_GP8_DB);
	wm8994_write(codec, WM8994_GPIO_9, WM8994_GP9_DB);
	wm8994_write(codec, WM8994_GPIO_10, WM8994_GP10_DB);
	wm8994_write(codec, WM8994_GPIO_11, WM8994_GP11_DB);

	/* Un-Mute*/
	val = wm8994_read(codec, WM8994_AIF1_DAC1_FILTERS_1);
	val &= ~(WM8994_AIF1DAC1_MUTE_MASK | WM8994_AIF1DAC1_MONO_MASK);
	val |= (WM8994_AIF1DAC1_UNMUTE);
	wm8994_write(codec, WM8994_AIF1_DAC1_FILTERS_1, val);

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
	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
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
	if(loopback_mode == LOOPBACK_MODE_OFF)
	{
		wm8994_write(codec, 0x0018, 0x0116);	// Left Line Input 1&2 Volume
		wm8994_write(codec, 0x0500, 0x01C0);	// AIF2 ADC Left Volume
		wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
		wm8994_write(codec, 0x0603, 0x000C);	// DAC2 Mixer Volumes
		wm8994_write(codec, 0x0621, 0x01C0);	// Sidetone
	}
	else
	{
		wm8994_write(codec, 0x0018, 0x0110);	// Left Line Input 1&2 Volume
		wm8994_write(codec, 0x0500, 0x01C0);	// AIF2 ADC Left Volume
		wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
		wm8994_write(codec, 0x0603, 0x000C);	// DAC2 Mixer Volumes

		DEBUG_LOG("=====================================> Loopback Mode");
	}

	/* Output Path Volume */
	if(loopback_mode == LOOPBACK_MODE_OFF)
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
		if(wm8994->codec_state & CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
		}
		else
		{
			wm8994_write(codec, 0x001F, 0x0020);	// HPOUT2 Volume
		}
#endif
	}
	else
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Output Mixer 6
		wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume
		wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
		wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
		wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
	}

	wm8994_write(codec, 0x0015, 0x0000);	
	wm8994_write(codec, 0x0038, 0x0040);	// Anti Pop 1
	wm8994_write(codec, 0x0006, 0x0000);	// Power Management 6. Prevent the mute when the audio transfer is executed from the bluetooth.

	/* Sidetone */
	wm8994_write(codec, 0x0600, 0x0003);	// DAC1 Mixer Volume
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
	int upper_value = 0;
	int lower_value = 0;
	int value = 0;

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

	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
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
	}
	else if(tty_mode == TTY_MODE_HCO || tty_mode == TTY_MODE_FULL)
	{
		wm8994_write(codec, 0x0028, 0x0001);	// Input Mixer 2
		wm8994_write(codec, 0x0002, 0x6130);	// Power Management 2
		wm8994_write(codec, 0x002A, 0x0030);	// Input Mixer 4
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

	wm8994_earsel_control(wm8994->pdata, 1);

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
	else if(tty_mode == TTY_MODE_HCO || tty_mode == TTY_MODE_FULL)
	{
		wm8994_write(codec, 0x001A, 0x011F/*0x0112*/);	  // Right Line Input 1&2 Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
	}
	else if(loopback_mode == PBA_LOOPBACK_MODE_ON)
	{
		wm8994_write(codec, 0x001A, 0x0116);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
		wm8994_write(codec, 0x0501, 0x01C0);	// AIF2 Right ADC Volume

		DEBUG_LOG("===================================================> Loopback Mode = %d.", loopback_mode);
	}
	else if(loopback_mode == SIMPLETEST_LOOPBACK_MODE_ON)
	{
		wm8994_write(codec, 0x001A, 0x0116);	// Right Line Input 1&2 Volume
		wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
		wm8994_write(codec, 0x0501, 0x01EF);	// AIF2 Right ADC Volume

		DEBUG_LOG("===================================================> Loopback Mode = %d.", loopback_mode);
	}
	else
	{
		wm8994_write(codec, 0x001A, 0x0117);	// Right Line Input 1&2 Volume
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
#if 0
		wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
#else
		if(wm8994->codec_state & CALL_ACTIVE)
		{
			wm8994_write(codec, 0x001F, 0x0000);	// HPOUT2 Volume
		}
		else
		{
			wm8994_write(codec, 0x001F, 0x0020);	// HPOUT2 Volume
		}
#endif
	}
	else
	{
		wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
		wm8994_write(codec, 0x0032, 0x0000);	// Outupt Mixer 6
		wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
		wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume
#if 1
		if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
		{
			wm8994_write(codec, 0x001C, 0x0179);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0179);	// Right Output Volume
		}
		else
		{
			wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
		}
#else
		if(wm8994->codec_state & CALL_ACTIVE)
		{
			if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
			{
				wm8994_write(codec, 0x001C, 0x0179);	// Left Output Volume
				wm8994_write(codec, 0x001D, 0x0179);	// Right Output Volume
			}
			else
			{
				wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
				wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
			}
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
		wm8994_write(codec, 0x0001, 0x0833);	// Power Management 1
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
	int upper_value = 0;
	int lower_value = 0;
	int value = 0;

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

	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
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
	wm8994_write(codec, 0x0029, 0x0030);	// Input Mixer 
	wm8994_write(codec, 0x0004, 0x2002);	// Power Management 4
	wm8994_write(codec, 0x0604, 0x0030);	// DAC2 Left Mixer Routing
	wm8994_write(codec, 0x0605, 0x0030);	// DAC2 Right Mixer Routing

	audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);

	/* Output Path Routing */
	wm8994_write(codec, 0x0005, 0x3303);	// Power Management 5
	wm8994_write(codec, 0x0601, 0x0005);	// DAC1 Left Mixer Routing
	wm8994_write(codec, 0x0602, 0x0005);	// DAC1 Right Mixer Routing
	wm8994_write(codec, 0x002D, 0x0100);	// Output Mixer 1
	wm8994_write(codec, 0x002E, 0x0100);	// Output Mixer 2
	wm8994_write(codec, 0x0003, 0x00F0);	// Power Management 3(Playback)
	wm8994_write(codec, 0x0060, 0x00EE);	// Analogue HP 1
	wm8994_write(codec, 0x0420, 0x0080);	// AIF1 DAC1 Filter1

	/* Input Path Volume */
	wm8994_write(codec, 0x0018, 0x0116);	// Left Line Input 1&2 Volume
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0603, 0x018C);	// DAC2 Mixer Volumes

	/* Output Path Volume */
	wm8994_write(codec, 0x0031, 0x0000);	// Output Mixer 5
	wm8994_write(codec, 0x0032, 0x0000);	// Outupt Mixer 6
	wm8994_write(codec, 0x0020, 0x0179);	// Left OPGA Volume
	wm8994_write(codec, 0x0021, 0x0179);	// Right OPGA Volume
#if 1
	if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
	{
		wm8994_write(codec, 0x001C, 0x0179);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0179);	// Right Output Volume
	}
	else
	{
		wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
	}
#else
	if(wm8994->codec_state & CALL_ACTIVE)
	{
		if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
		{
			wm8994_write(codec, 0x001C, 0x0179);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0179);	// Right Output Volume
		}
		else
		{
			wm8994_write(codec, 0x001C, 0x0170);	// Left Output Volume
			wm8994_write(codec, 0x001D, 0x0170);	// Right Output Volume
		}
	}
	else
	{
		wm8994_write(codec, 0x001C, 0x0100);	// Left Output Volume
		wm8994_write(codec, 0x001D, 0x0100);	// Right Output Volume
	}
#endif
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume
	wm8994_write(codec, 0x0611, 0x01C0);	// DAC1 Right Volume
	
	wm8994_write(codec, 0x0006, 0x0000);	// Power Management 6. Prevent the mute when the audio transfer is executed from the bluetooth.	
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

	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
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
	wm8994_write(codec, 0x0311, 0x0000);	// AIF2 Control 2
	wm8994_write(codec, 0x0520, 0x0080);	// AIF2 DAC Filter1
	wm8994_write(codec, 0x0204, 0x0019);	// AIF2 Clocking 1

	/* Input Path Routing */
	wm8994_write(codec, 0x0028, 0x0040);	// Input Mixer 2. SPK Mic using the IN2NL | DM1CDAT1
	wm8994_write(codec, 0x0002, 0x6280);	// SPK Mic using the IN2NL | DM1CDAT1

	wm8994_write(codec, 0x0029, 0x0100);	// Input Mixer 3. SPK Mic using the IN2NL | DM1CDAT1
	wm8994_write(codec, 0x0004, 0x2002);	// Power Management 4
	wm8994_write(codec, 0x0604, 0x0010);	// DAC2 Left Mixer Routing
	wm8994_write(codec, 0x0605, 0x0010);	// DAC2 Right Mixer Routing

	audio_ctrl_mic_bias_gpio(wm8994->pdata, 1);

	/* Output Path Routing */
	wm8994_write(codec, 0x0005, 0x3303);	// Power Management 5
	wm8994_write(codec, 0x0003, 0x0300);	// Power Management 3
	wm8994_write(codec, 0x0601, 0x0005);	// DAC1 Left Mixer Routing. '0x0001' is added for a playback. (Original = 0x0004)
	wm8994_write(codec, 0x0602, 0x0001);	// DAC1 Right Mixer Routing(Playback)
#ifdef STEREO_SPEAKER_SUPPORT
	wm8994_write(codec, 0x0024, 0x0011);	// SPKOUT Mixers
#else
	wm8994_write(codec, 0x0024, 0x0010);	// SPKOUT Mixers
#endif
	wm8994_write(codec, 0x0420, 0x0080);	// AIF2 DAC Filter1(Playback)

	/* Input Path Volume */
	wm8994_write(codec, 0x0019, 0x0112);	// Left Line Input 3&4 Volume. SPK Mic using the IN2NL | DM1CDAT1
	wm8994_write(codec, 0x0603, 0x000C);	// DAC2 Mixer Volumes
	wm8994_write(codec, 0x0612, 0x01C0);	// DAC2 Left Volume
	wm8994_write(codec, 0x0613, 0x01C0);	// DAC2 Right Volume
	wm8994_write(codec, 0x0500, 0x01EF);	// AIF2 ADC Left Volume

	/* Output Path Volume */
	wm8994_write(codec, 0x0022, 0x0000);	// SPKMIXL Attenuation
#if 1
	wm8994_write(codec, 0x0026, 0x017E);	// Speaker Volume Left
#else
	if(wm8994->codec_state & CALL_ACTIVE)
	{
		wm8994_write(codec, 0x0026, 0x017E);	// Speaker Volume Left
	}
	else
	{
		wm8994_write(codec, 0x0026, 0x0100);	// Speaker Volume Left
	}
#endif
#ifdef STEREO_SPEAKER_SUPPORT
	wm8994_write(codec, 0x0025, ((0x0007 << 0x0003) | (0x0007 << 0x0000)));    // SPKOUT Boost
#else
	wm8994_write(codec, 0x0025, (0x0007 << 0x0003));	// SPKOUT Boost
#endif
	wm8994_write(codec, 0x0610, 0x01C0);	// DAC1 Left Volume

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

	wm8994_write(codec, 0x0244, 0x0C81);	// FLL2 Control 5
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
        /* Output Path Volume */
        wm8994_write(codec, 0x0610, 0x0100);    // DAC1 Left Volume
        wm8994_write(codec, 0x0611, 0x0100);    // DAC1 Right Volume
        wm8994_write(codec, 0x0020, 0x0040);    // Left OPGA Volume
        wm8994_write(codec, 0x0021, 0x0040);    // Right OPGA Volume
        wm8994_write(codec, 0x001F, 0x0020);    // HPOUT2 Volume

        /* Output Path Routing */
        wm8994_write(codec, 0x0033, 0x0000);    // HPOUT2 Mixer. '0x0008' is added for a playback. (Original = 0x0010)
        wm8994_write(codec, 0x0420, 0x0200);    // AIF1 DAC1 FIlter(Playback)

        /* Input Path Volume */
        wm8994_write(codec, 0x0018, 0x008B);    // Left Line Input 1&2 Volume
        wm8994_write(codec, 0x0612, 0x0100);    // DAC2 Left Volume
        wm8994_write(codec, 0x0603, 0x0000);    // DAC2 Mixer Volumes
    
        DEBUG_LOG("===========================> The receiver voice path is muted.");
    }
    else if(path == HP)
    {
        /* Output Path Volume */
        if(tty_mode == TTY_MODE_HCO)
        {
            wm8994_write(codec, 0x0020, 0x0000);    // Left OPGA Volume
            wm8994_write(codec, 0x0021, 0x0000);    // Right OPGA Volume
            wm8994_write(codec, 0x001F, 0x0020);    // HPOUT2 Volume
            wm8994_write(codec, 0x0610, 0x0100);    // DAC1 Left Volume
            wm8994_write(codec, 0x0611, 0x0100);    // DAC1 Right Volume
            wm8994_write(codec, 0x0033, 0x0000);    // HPOUT2 Mixer
        }
        else
        {
            wm8994_write(codec, 0x001C, 0x0100);    // Left Output Volume
            wm8994_write(codec, 0x001D, 0x0100);    // Right Output Volume
            wm8994_write(codec, 0x0020, 0x0000);    // Left OPGA Volume
            wm8994_write(codec, 0x0021, 0x0000);    // Right OPGA Volume
            wm8994_write(codec, 0x0610, 0x0100);    // DAC1 Left Volume
            wm8994_write(codec, 0x0611, 0x0100);    // DAC1 Right Volume
            wm8994_write(codec, 0x0060, 0x0000);    // Analogue HP 1
        }

        /* Output Path Routing */
        if(tty_mode == TTY_MODE_HCO)
        {
            wm8994_write(codec, 0x0033, 0x0000);    // HPOUT2 Mixer
            wm8994_write(codec, 0x0420, 0x0200);    // AIF1 DAC1 Filter1
        }
        else
        {
            wm8994_write(codec, 0x0060, 0x0000);    // Analogue HP 1
            wm8994_write(codec, 0x0420, 0x0200);    // AIF1 DAC1 Filter1
        }

        /* Input Path Volume */
        if(tty_mode == TTY_MODE_VCO)
        {
            wm8994_write(codec, 0x0018, 0x008B);    // Left Line Input 1&2 Volume
        }
        else
        {
            wm8994_write(codec, 0x001A, 0x008B);    // Right Line Input 1&2 Volume
        }
        wm8994_write(codec, 0x0612, 0x0100);    // DAC2 Left Volume
        wm8994_write(codec, 0x0613, 0x0100);    // DAC2 Right Volume
        wm8994_write(codec, 0x0603, 0x0000);    // DAC2 Mixer Volumes
        DEBUG_LOG("===========================> The headset voice path is muted.");
    }
    else if(path == SPK)
    {
        /* Output Path Volume */
        wm8994_write(codec, 0x0025, 0x0000);    // SPKOUT Boost
		wm8994_write(codec, 0x0026, 0x0100);    // Speaker Volume Left
		wm8994_write(codec, 0x0027, 0x0000);    // Speaker Volume Right 
      	wm8994_write(codec, 0x0613, 0x0100);    // DAC2 Right Volume

        /* Output Path Routing */
        wm8994_write(codec, 0x0024, 0x0000);    // SPKOUT Mixers
        wm8994_write(codec, 0x0420, 0x0200);    // AIF2 DAC Filter1(Playback)

        /* Input Path Volume */
        wm8994_write(codec, 0x0019, 0x008B);    // Left Line Input 3&4 Volume. SPK Mic using the IN2NL | DM1CDAT1
        wm8994_write(codec, 0x0604, 0x0000);    // DAC2 Left Mixer Routing
    	wm8994_write(codec, 0x0605, 0x0000);    // DAC2 Right Mixer Routing

        DEBUG_LOG("===========================> The speaker voice path is muted.");
    }
    else if(path == BT)
    {
        /* Output Path Volume */
        wm8994_write(codec, 0x0420, 0x0200);    // AIF1 DAC1 FIlter(Playback)
        
        /* Input Path Routing */
        wm8994_write(codec, 0x0604, 0x0007);    // DAC2 Left Mixer Routing(Playback)
        wm8994_write(codec, 0x0605, 0x0007);    // DAC2 Right Mixer(Playback)
    
        wm8994_write(codec, 0x0707, 0x8000);    // GPIO 8. BT PCM DOUT
        wm8994_write(codec, 0x0708, 0x0000);    // GPIO 9. BT PCM DIN
        wm8994_write(codec, 0x0709, 0x0000);    // GPIO 10. BT PCM SYNC
        wm8994_write(codec, 0x070A, 0x0000);    // GPIO 11. BT PCM CLK

        /* Input Path Volume */
    	wm8994_write(codec, 0x0612, 0x0100);    // DAC2 Left Volume(Playback)
    	wm8994_write(codec, 0x0613, 0x0100);    // DAC2 Right Volume(Playback)
    	wm8994_write(codec, 0x0402, (WM8994_AIF1DAC1_VU | 0x0000));    // AIF1 DAC1 Left Volume(Playback)

        DEBUG_LOG("===========================> The bluetooth voice path is muted.");
    }
}

void wm8994_set_end_point_volume(struct snd_soc_codec *codec, int path)
{
    switch(path)
    {
        case RCV :
        {
            wm8994_write(codec, 0x001F, 0x0000);    // HPOUT2 Volume

            DEBUG_LOG("===========================> The end point volume for a receiver is set.");
            break;
        }
        case HP :
        {
            if(tty_mode == TTY_MODE_HCO)
            {
                wm8994_write(codec, 0x001F, 0x0000);    // HPOUT2 Volume
            }
            else if(tty_mode == TTY_MODE_FULL || tty_mode == TTY_MODE_VCO)
            {
                wm8994_write(codec, 0x001C, 0x0179);    // Left Output Volume
                wm8994_write(codec, 0x001D, 0x0179);    // Right Output Volume
            }
            else
            {
                wm8994_write(codec, 0x001C, 0x0170);    // Left Output Volume
                wm8994_write(codec, 0x001D, 0x0170);    // Right Output Volume
            }
            DEBUG_LOG("===========================> The end point volume for a headset is set.");
            break;
        }
		case HP_NO_MIC:
		{
			wm8994_write(codec, 0x001C, 0x0179);    // Left Output Volume
            wm8994_write(codec, 0x001D, 0x0179);    // Right Output Volume

			DEBUG_LOG("===========================> The end point volume for a 3 polar headset is set.");
			break;
		}
        case SPK :
        {
            wm8994_write(codec, 0x0026, 0x017E);    // Speaker Volume Left

            DEBUG_LOG("===========================> The end point volume for a speaker is set.");
            break;
        }
        case BT :
        {
            wm8994_write(codec, 0x0612, 0x01C0);    // DAC2 Left Volume(Playback)
	        wm8994_write(codec, 0x0613, 0x01C0);    // DAC2 Right Volume(Playback)

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

int wm8994_set_codec_gain(struct snd_soc_codec *codec, u16 mode, u16 device)
{
	struct wm8994_priv *wm8994 = codec->drvdata;
	int i;
	u32 gain_set_bits = COMMON_SET_BIT;
	u16 val;
	struct gain_info_t *default_gain_table_p = NULL;
	int table_num = 0;

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
		case PLAYBACK_BT:
			gain_set_bits |= PLAYBACK_BT;
			break;
		case PLAYBACK_SPK_HP:
			gain_set_bits |= PLAYBACK_SPK_HP;
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
		case PLAYBACK_HP_NO_MIC:
			gain_set_bits |= PLAYBACK_HP_NO_MIC;
			break;
		default:
			pr_err("playback modo gain flag is wrong\n");
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
			pr_err("voicemode gain flag is wrong\n");
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
		default:
			pr_err("recording gain flag is wrong\n");
		}

	}

	DEBUG_LOG("Set gain mode = 0x%x, device = 0x%x, gain_bits = 0x%x,\
		table_num=%d, gain_code = %d\n",
		mode, device, gain_set_bits, table_num, wm8994->gain_code);

	/* default gain table setting */
	for (i = 0; i < table_num; i++) {
		if ((default_gain_table_p + i)->mode & gain_set_bits) {
			val = wm8994_read(codec, (default_gain_table_p + i)->reg);
			val &= ~((default_gain_table_p + i)->mask);
			val |= (default_gain_table_p + i)->gain;
			wm8994_write(codec, (default_gain_table_p + i)->reg, val);
		}
	}

	if (wm8994->gain_code) {
		gain_set_bits &= ~(COMMON_SET_BIT);
		gain_set_bits |= (mode | GAIN_DIVISION_BIT);
		default_gain_table_p = gain_code_table;
		table_num = GAIN_CODE_NUM;

		for (i = 0; i < table_num; i++) {
			if ((default_gain_table_p + i)->mode == gain_set_bits) {
				val = wm8994_read(codec, (default_gain_table_p + i)->reg);
				val &= ~((default_gain_table_p + i)->mask);
				val |= (default_gain_table_p + i)->gain;
				wm8994_write(codec, (default_gain_table_p + i)->reg, val);
			}
		}

	}
	return 0;

}

