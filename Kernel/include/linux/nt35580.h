/*include/linux/nt35580.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * Header file for Sony LCD Panel(TFT) driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/types.h>

struct s5p_panel_data_tft {
	const u16 *seq_set;
	const u16 *sleep_in;
	const u16 *display_on;
	const u16 *display_off;
	u16 *brightness_set;
};


