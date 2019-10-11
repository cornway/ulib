#ifndef __HDMI_PUB_H__
#define __HDMI_PUB_H__

#if defined(USE_LCD_HDMI)

#include <stdint.h>

#define EDID_SEG_SIZE 0x100

#pragma anon_unions

typedef struct {
    union {
        uint8_t raw[EDID_SEG_SIZE];
    };
} hdmi_edid_seg_t;

typedef struct {
    unsigned timing_800x600_60   : 1;
    unsigned timing_800x600_56   : 1;
    unsigned timing_640x480_75   : 1;
    unsigned timing_640x480_72   : 1;
    unsigned timing_640x480_67   : 1;
    unsigned timing_640x480_60   : 1;
    unsigned timing_720x400_88   : 1;
    unsigned timing_720x400_70   : 1;

    unsigned timing_1280x1024_75 : 1;
    unsigned timing_1024x768_75  : 1;
    unsigned timing_1024x768_70  : 1;
    unsigned timing_1024x768_60  : 1;
    unsigned timing_1024x768_87  : 1;
    unsigned timing_832x624_75   : 1;
    unsigned timing_800x600_75   : 1;
    unsigned timing_800x600_72   : 1;
} hdmi_std_timing_t;

typedef struct {
    int xres, yres;
    float rate_hz;
    float pclk_mhz;
    int interlaced;
    int hres, hstart, hend, htotal;
    int vres, vstart, vend, vtotal;
    char hpol, vpol;

    hdmi_std_timing_t std;

} hdmi_timing_t;


int hdmi_parse_edid (hdmi_timing_t *timing, hdmi_edid_seg_t *edid, int size);

#endif /*defined(USE_LCD_HDMI)*/

#endif /*__HDMI_PUB_H__*/

