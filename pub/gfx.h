#ifndef _GFX_H_
#define _GFX_H_

#include "dev_conf.h"
#include "stdint.h"

#define GFX_COLOR_MODE_CLUT 1
#define GFX_COLOR_MODE_RGB565 2
#define GFX_COLOR_MODE_RGBA8888 3
#define GFX_COLOR_MODE_MAX 4
#define GFX_COLOR_MODE_AUTO (0xff)

#ifdef GFX_COLOR_MODE
#error "deprecated option, must be removed"
#endif

#define GFX_RGB565(r, g, b)			((((r & 0xF8) >> 3) << 11) | (((g & 0xFC) >> 2) << 5) | ((b & 0xF8) >> 3))

#define GFX_RGB565_R(color)			((0xF800 & color) >> 11)
#define GFX_RGB565_G(color)			((0x07E0 & color) >> 5)
#define GFX_RGB565_B(color)			(0x001F & color)

#define GFX_RGBA8888(r, g, b, a)	(((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF))

#define GFX_ARGB8888_R(color)		((color & 0x00FF0000) >> 16)
#define GFX_ARGB8888_G(color)		((color & 0x0000FF00) >> 8)
#define GFX_ARGB8888_B(color)		((color & 0x000000FF))
#define GFX_ARGB8888_A(color)		((color & 0xFF000000) >> 24)

#define GFX_TRANSPARENT				GFX_ARGB8888_A(LCD_COLOR_TRANSPARENT)
#define GFX_OPAQUE					GFX_ARGB8888_A(~LCD_COLOR_TRANSPARENT)

// RGB565 colors (RRRR RGGG GGGB BBBB)
#define RGB565_BLACK				0x0000
#define RGB565_WHITE				0xFFFF

#define RGB565_RED					0xF800
#define RGB565_GREEN				0x07E0
#define RGB565_BLUE					0x001F

#define RGB565_CYAN					0x07FF
#define RGB565_MAGENTA				0xF81F
#define RGB565_YELLOW				0xFFE0

#define RGB565_GRAY					0xF7DE

// RGB8888 colors (AAAA AAAA RRRR RRRR GGGG GGGG BBBB BBBB)
#define ARGB8888_BLACK				0xFF000000
#define ARGB8888_WHITE				0xFFFFFFFF

#define ARGB8888_RED				0xFFFF0000
#define ARGB8888_GREEN				0xFF00FF00
#define ARGB8888_BLUE				0xFF0000FF

#endif /*_GFX_H_*/
