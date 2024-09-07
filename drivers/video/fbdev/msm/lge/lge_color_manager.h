/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef LGE_COLOR_MANAGER_H
#define LGE_COLOR_MANAGER_H

#define HDR_OFF				0
#define HDR_ON 				1

#define SHA_OFF				0
#define SHA_ON				1

#define HL_MODE_OFF 		0
#define HL_MODE_ON 			1

#define SC_MODE_DEFAULT		2
#define SC_MODE_MAX			4

#define RGB_DEFAULT_PRESET	2
#define RGB_DEFAULT_RED		0
#define RGB_DEFAULT_BLUE 	0
#define RGB_DEFAULT_GREEN	0

#define SHARP_DEFAULT       0

#define DG_MODE_MAX			4
#define DG_OFF				0
#define DG_ON				1
#define STEP_DG_PRESET		5
#define NUM_DG_PRESET		7
#define STEP_GC_PRESET		4
#define OFFSET_DG_CTRL		8
#define NUM_DG_CTRL			0x10

#define LGE_SCREEN_TUNE_OFF	0
#define LGE_SCREEN_TUNE_ON	1
#define LGE_SCREEN_TUNE_GAM	2
#define LGE_SCREEN_TUNE_GAL	3
#define LGE_SAT_GAM_MODE	3
#define LGE_SAT_GAL_MODE	5

#define ECO_CABC_30         30
#define ECO_CABC_50         50
#define ECO_CABC_DEFAULT    ECO_CABC_30

enum {
	RED      = 0,
	GREEN    = 1,
	BLUE     = 2,
	RGB_ALL  = 3
};

enum {
	PRESET_SETP0_INDEX = 0,
	PRESET_SETP1_INDEX = 6,
	PRESET_SETP2_INDEX = 12,
};

enum {
	PRESET_SETP0_OFFSET = 0,
	PRESET_SETP1_OFFSET = 1,//2
	PRESET_SETP2_OFFSET = 2//5
};

enum lge_screen_color_mode {
	LGE_COLOR_OPT = 0,
//	LGE_COLOR_ECO,	/* TBD */
	LGE_COLOR_CIN = 1,
	LGE_COLOR_SPO = 4,
	LGE_COLOR_GAM = 5,
	LGE_COLOR_MAN = 10,
	LGE_COLOR_MAX,
};

enum lge_gamma_correction_mode {
	LGE_GC_MOD_NOR = 0,
	LGE_GC_MOD_CIN,
	LGE_GC_MOD_SPO,
	LGE_GC_MOD_GAM,
	LGE_GC_MOD_MAX,
};

#endif /* LGE_COLOR_MANAGER_H */
