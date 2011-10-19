/*
 * qt602240_ts.h - AT42QT602240 Touchscreen driver
 *
 * Copyright (C) 2009 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_QT602240_TS_H
#define __LINUX_QT602240_TS_H

#define TOUCH_KEY_LEDS 420

/* The platform data for the AT42QT602240 touchscreen driver */
struct _touchkey_area {
      unsigned int y_start;
      unsigned int y_end;
      unsigned int x_start;
      unsigned int x_end; 
};

struct qt602240_platform_data {
        unsigned int x_line;
        unsigned int y_line;
        unsigned int x_size;
        unsigned int y_size;
        unsigned int blen;
        unsigned int threshold;
        unsigned int voltage;
        unsigned char orient;
	unsigned int irq;
	unsigned int touch_int;
	unsigned int touch_en;
	unsigned char is_tickertab;
};

#endif /* __LINUX_QT602240_TS_H */
