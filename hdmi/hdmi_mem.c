/* vim: set et fde fdm=syntax ft=c.doxygen ts=4 sts=4 sw=4 : */
/*
 * Copyright © 2011 Saleem Abdulrasool <compnerd@compnerd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(BSP_DRIVER)

#include "stm32f769i_discovery_lcd.h"
#include "main.h"
#include "debug.h"
#include "arch.h"
#include <string.h>
#include <math.h>
#include "misc_utils.h"

#ifndef strchrnul
const char *strchrnul (const char *str, int ch)
{
    const char *p = str;
    while (*p) {
        if (*p == ch) break;
        p++;
    }
    return p;
}
#endif /*strchrnul*/

#if defined(USE_LCD_HDMI)

#define CM_2_MM(cm)                             ((cm) * 10)
#define CM_2_IN(cm)                             ((cm) * 0.3937f)

#define HZ_2_MHZ(hz)                            ((hz) / 1000000.0f)

#define CEA861_NO_DTDS_PRESENT (0x04)

/*! \todo figure out a better way to determine the offsets */
#define HDMI_VSDB_EXTENSION_FLAGS_OFFSET        (0x06)
#define HDMI_VSDB_MAX_TMDS_OFFSET               (0x07)
#define HDMI_VSDB_LATENCY_FIELDS_OFFSET         (0x08)

enum edid_aspect_ratio {
    EDID_ASPECT_RATIO_16_10,
    EDID_ASPECT_RATIO_4_3,
    EDID_ASPECT_RATIO_5_4,
    EDID_ASPECT_RATIO_16_9,
};

enum edid_display_type {
    EDID_DISPLAY_TYPE_MONOCHROME,
    EDID_DISPLAY_TYPE_RGB,
    EDID_DISPLAY_TYPE_NON_RGB,
    EDID_DISPLAY_TYPE_UNDEFINED,
};

enum edid_monitor_descriptor_type {
    EDID_MONTIOR_DESCRIPTOR_MANUFACTURER_DEFINED        = 0x0f,
    EDID_MONITOR_DESCRIPTOR_STANDARD_TIMING_IDENTIFIERS = 0xfa,
    EDID_MONITOR_DESCRIPTOR_COLOR_POINT                 = 0xfb,
    EDID_MONITOR_DESCRIPTOR_MONITOR_NAME                = 0xfc,
    EDID_MONITOR_DESCRIPTOR_MONITOR_RANGE_LIMITS        = 0xfd,
    EDID_MONITOR_DESCRIPTOR_ASCII_STRING                = 0xfe,
    EDID_MONITOR_DESCRIPTOR_MONITOR_SERIAL_NUMBER       = 0xff,
};

enum cea861_data_block_type {
    CEA861_DATA_BLOCK_TYPE_RESERVED0,
    CEA861_DATA_BLOCK_TYPE_AUDIO,
    CEA861_DATA_BLOCK_TYPE_VIDEO,
    CEA861_DATA_BLOCK_TYPE_VENDOR_SPECIFIC,
    CEA861_DATA_BLOCK_TYPE_SPEAKER_ALLOCATION,
    CEA861_DATA_BLOCK_TYPE_VESA_DTC,
    CEA861_DATA_BLOCK_TYPE_RESERVED6,
    CEA861_DATA_BLOCK_TYPE_EXTENDED,
};

enum cea861_audio_format {
    CEA861_AUDIO_FORMAT_RESERVED,
    CEA861_AUDIO_FORMAT_LPCM,
    CEA861_AUDIO_FORMAT_AC_3,
    CEA861_AUDIO_FORMAT_MPEG_1,
    CEA861_AUDIO_FORMAT_MP3,
    CEA861_AUDIO_FORMAT_MPEG2,
    CEA861_AUDIO_FORMAT_AAC_LC,
    CEA861_AUDIO_FORMAT_DTS,
    CEA861_AUDIO_FORMAT_ATRAC,
    CEA861_AUDIO_FORMAT_DSD,
    CEA861_AUDIO_FORMAT_E_AC_3,
    CEA861_AUDIO_FORMAT_DTS_HD,
    CEA861_AUDIO_FORMAT_MLP,
    CEA861_AUDIO_FORMAT_DST,
    CEA861_AUDIO_FORMAT_WMA_PRO,
    CEA861_AUDIO_FORMAT_EXTENDED,
};

enum edid_extension_type {
    EDID_EXTENSION_TIMING           = 0x01, // Timing Extension
    EDID_EXTENSION_CEA              = 0x02, // Additional Timing Block Data (CEA EDID Timing Extension)
    EDID_EXTENSION_VTB              = 0x10, // Video Timing Block Extension (VTB-EXT)
    EDID_EXTENSION_EDID_2_0         = 0x20, // EDID 2.0 Extension
    EDID_EXTENSION_DI               = 0x40, // Display Information Extension (DI-EXT)
    EDID_EXTENSION_LS               = 0x50, // Localised String Extension (LS-EXT)
    EDID_EXTENSION_MI               = 0x60, // Microdisplay Interface Extension (MI-EXT)
    EDID_EXTENSION_DTCDB_1          = 0xa7, // Display Transfer Characteristics Data Block (DTCDB)
    EDID_EXTENSION_DTCDB_2          = 0xaf,
    EDID_EXTENSION_DTCDB_3          = 0xbf,
    EDID_EXTENSION_BLOCK_MAP        = 0xf0, // Block Map
    EDID_EXTENSION_DDDB             = 0xff, // Display Device Data Block (DDDB)
};

struct cea861_timing_block {
    /* CEA Extension Header */
    uint8_t  tag;
    uint8_t  revision;
    uint8_t  dtd_offset;

    /* Global Declarations */
    unsigned native_dtds           : 4;
    unsigned yuv_422_supported     : 1;
    unsigned yuv_444_supported     : 1;
    unsigned basic_audio_supported : 1;
    unsigned underscan_supported   : 1;

    uint8_t  data[123];

    uint8_t  checksum;
};

struct V_PREPACK cea861_data_block_header {
    unsigned length : 5;
    unsigned tag    : 3;
} V_POSTPACK;

struct V_PREPACK cea861_short_video_descriptor {
    unsigned video_identification_code : 7;
    unsigned native                    : 1;
} V_POSTPACK;

struct V_PREPACK cea861_video_data_block {
    struct cea861_data_block_header      header;
    struct cea861_short_video_descriptor svd[];
} V_POSTPACK;

struct V_PREPACK cea861_short_audio_descriptor {
    unsigned channels              : 3; /* = value + 1 */
    unsigned audio_format          : 4;
    unsigned                       : 1;

    unsigned sample_rate_32_kHz    : 1;
    unsigned sample_rate_44_1_kHz  : 1;
    unsigned sample_rate_48_kHz    : 1;
    unsigned sample_rate_88_2_kHz  : 1;
    unsigned sample_rate_96_kHz    : 1;
    unsigned sample_rate_176_4_kHz : 1;
    unsigned sample_rate_192_kHz   : 1;
    unsigned                       : 1;

    union V_PREPACK {
        struct V_PREPACK {
            unsigned bitrate_16_bit : 1;
            unsigned bitrate_20_bit : 1;
            unsigned bitrate_24_bit : 1;
            unsigned                : 5;
        } V_POSTPACK lpcm;

        uint8_t maximum_bit_rate;       /* formats 2-8; = value * 8 kHz */

        uint8_t format_dependent;       /* formats 9-13; */

        struct V_PREPACK {
            unsigned profile : 3;
            unsigned         : 5;
        } V_POSTPACK wma_pro;

        struct V_PREPACK {
            unsigned      : 3;
            unsigned code : 5;
        } V_POSTPACK extension;
    } V_POSTPACK flags;
} V_POSTPACK;

struct V_PREPACK cea861_audio_data_block {
    struct cea861_data_block_header      header;
    struct cea861_short_audio_descriptor sad[];
} V_POSTPACK;

struct V_PREPACK cea861_speaker_allocation {
    unsigned front_left_right        : 1;
    unsigned front_lfe               : 1;   /* low frequency effects */
    unsigned front_center            : 1;
    unsigned rear_left_right         : 1;
    unsigned rear_center             : 1;
    unsigned front_left_right_center : 1;
    unsigned rear_left_right_center  : 1;
    unsigned front_left_right_wide   : 1;

    unsigned front_left_right_high   : 1;
    unsigned top_center              : 1;
    unsigned front_center_high       : 1;
    unsigned                         : 5;

    unsigned                         : 8;
} V_POSTPACK;

struct V_PREPACK cea861_speaker_allocation_data_block {
    struct cea861_data_block_header  header;
    struct cea861_speaker_allocation payload;
} V_POSTPACK;

struct V_PREPACK cea861_vendor_specific_data_block {
    struct cea861_data_block_header  header;
    uint8_t                          ieee_registration[3];
    uint8_t                          data[30];
} V_POSTPACK;

struct V_PREPACK edid_monitor_descriptor {
    uint16_t flag0;
    uint8_t  flag1;
    uint8_t  tag;
    uint8_t  flag2;
    uint8_t  data[13];
} V_POSTPACK;

struct V_PREPACK edid_standard_timing_descriptor {
    uint8_t  horizontal_active_pixels;         /* = (value + 31) * 8 */

    unsigned refresh_rate       : 6;           /* = value + 60 */
    unsigned image_aspect_ratio : 2;
} V_POSTPACK;

struct V_PREPACK edid_detailed_timing_descriptor {
    uint16_t pixel_clock;                               /* = value * 10000 */

    uint8_t  horizontal_active_lo;
    uint8_t  horizontal_blanking_lo;

    unsigned horizontal_blanking_hi         : 4;
    unsigned horizontal_active_hi           : 4;

    uint8_t  vertical_active_lo;
    uint8_t  vertical_blanking_lo;

    unsigned vertical_blanking_hi           : 4;
    unsigned vertical_active_hi             : 4;

    uint8_t  horizontal_sync_offset_lo;
    uint8_t  horizontal_sync_pulse_width_lo;

    unsigned vertical_sync_pulse_width_lo   : 4;
    unsigned vertical_sync_offset_lo        : 4;

    unsigned vertical_sync_pulse_width_hi   : 2;
    unsigned vertical_sync_offset_hi        : 2;
    unsigned horizontal_sync_pulse_width_hi : 2;
    unsigned horizontal_sync_offset_hi      : 2;

    uint8_t  horizontal_image_size_lo;
    uint8_t  vertical_image_size_lo;

    unsigned vertical_image_size_hi         : 4;
    unsigned horizontal_image_size_hi       : 4;

    uint8_t  horizontal_border;
    uint8_t  vertical_border;

    unsigned stereo_mode_lo                 : 1;
    unsigned signal_pulse_polarity          : 1; /* pulse on sync, composite/horizontal polarity */
    unsigned signal_serration_polarity      : 1; /* serrate on sync, vertical polarity */
    unsigned signal_sync                    : 2;
    unsigned stereo_mode_hi                 : 2;
    unsigned interlaced                     : 1;
} V_POSTPACK;

struct V_PREPACK edid {
    /* header information */
    uint8_t  header[8];

    /* vendor/product identification */
    uint16_t manufacturer;

    uint8_t  product[2];
    uint8_t  serial_number[4];
    uint8_t  manufacture_week;
    uint8_t  manufacture_year;                  /* = value + 1990 */

    /* EDID version */
    uint8_t  version;
    uint8_t  revision;

    /* basic display parameters and features */
    union V_PREPACK {
        struct V_PREPACK {
            unsigned dfp_1x                 : 1;    /* VESA DFP 1.x */
            unsigned                        : 6;
            unsigned digital                : 1;
        } V_POSTPACK digital;
        struct V_PREPACK __attribute__ (( packed )) {
            unsigned vsync_serration        : 1;
            unsigned green_video_sync       : 1;
            unsigned composite_sync         : 1;
            unsigned separate_sync          : 1;
            unsigned blank_to_black_setup   : 1;
            unsigned signal_level_standard  : 2;
            unsigned digital                : 1;
        } V_POSTPACK analog;
    } V_POSTPACK video_input_definition;

    uint8_t  maximum_horizontal_image_size;     /* cm */
    uint8_t  maximum_vertical_image_size;       /* cm */

    uint8_t  display_transfer_characteristics;  /* gamma = (value + 100) / 100 */

    struct V_PREPACK {
        unsigned default_gtf                    : 1; /* generalised timing formula */
        unsigned preferred_timing_mode          : 1;
        unsigned standard_default_color_space   : 1;
        unsigned display_type                   : 2;
        unsigned active_off                     : 1;
        unsigned suspend                        : 1;
        unsigned standby                        : 1;
    } V_POSTPACK feature_support;

    /* color characteristics block */
    unsigned green_y_low    : 2;
    unsigned green_x_low    : 2;
    unsigned red_y_low      : 2;
    unsigned red_x_low      : 2;

    unsigned white_y_low    : 2;
    unsigned white_x_low    : 2;
    unsigned blue_y_low     : 2;
    unsigned blue_x_low     : 2;

    uint8_t  red_x;
    uint8_t  red_y;
    uint8_t  green_x;
    uint8_t  green_y;
    uint8_t  blue_x;
    uint8_t  blue_y;
    uint8_t  white_x;
    uint8_t  white_y;

    /* established timings */
    struct V_PREPACK {
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
    } V_POSTPACK established_timings;

    struct V_PREPACK {
        unsigned reserved            : 7;
        unsigned timing_1152x870_75  : 1;
    } V_POSTPACK manufacturer_timings;

    /* standard timing id */
    struct  edid_standard_timing_descriptor standard_timing_id[8];

    /* detailed timing */
    union V_PREPACK {
        struct edid_monitor_descriptor         monitor;
        struct edid_detailed_timing_descriptor timing;
    } V_POSTPACK detailed_timings[4];

    uint8_t  extensions;
    uint8_t  checksum;
} V_POSTPACK;

struct V_PREPACK edid_color_characteristics_data {
    struct V_PREPACK {
        uint16_t x;
        uint16_t y;
    } V_POSTPACK red, green, blue, white;
} V_POSTPACK;

struct V_PREPACK edid_monitor_range_limits {
    uint8_t  minimum_vertical_rate;             /* Hz */
    uint8_t  maximum_vertical_rate;             /* Hz */
    uint8_t  minimum_horizontal_rate;           /* kHz */
    uint8_t  maximum_horizontal_rate;           /* kHz */
    uint8_t  maximum_supported_pixel_clock;     /* = (value * 10) Mhz (round to 10 MHz) */

    /* secondary timing formula */
    uint8_t  secondary_timing_support;
    uint8_t  reserved;
    uint8_t  secondary_curve_start_frequency;   /* horizontal frequency / 2 kHz */
    uint8_t  c;                                 /* = (value >> 1) */
    uint16_t m;
    uint8_t  k;
    uint8_t  j;                                 /* = (value >> 1) */
} V_POSTPACK;

struct V_PREPACK edid_extension {
    uint8_t tag;
    uint8_t revision;
    uint8_t extension_data[125];
    uint8_t checksum;
} V_POSTPACK;

struct V_PREPACK hdmi_vendor_specific_data_block {
    struct cea861_data_block_header header;

    uint8_t  ieee_registration_id[3];           /* LSB */

    unsigned port_configuration_b      : 4;
    unsigned port_configuration_a      : 4;
    unsigned port_configuration_d      : 4;
    unsigned port_configuration_c      : 4;

    /* extension fields */
    unsigned dvi_dual_link             : 1;
    unsigned                           : 2;
    unsigned yuv_444_supported         : 1;
    unsigned colour_depth_30_bit       : 1;
    unsigned colour_depth_36_bit       : 1;
    unsigned colour_depth_48_bit       : 1;
    unsigned audio_info_frame          : 1;

    uint8_t  max_tmds_clock;                    /* = value * 5 */

    unsigned                           : 6;
    unsigned interlaced_latency_fields : 1;
    unsigned latency_fields            : 1;

    uint8_t  video_latency;                     /* = (value - 1) * 2 */
    uint8_t  audio_latency;                     /* = (value - 1) * 2 */
    uint8_t  interlaced_video_latency;
    uint8_t  interlaced_audio_latency;

    uint8_t  reserved[];
} V_POSTPACK;


typedef char edid_monitor_descriptor_string[sizeof_member(struct edid_monitor_descriptor, data) + 1];

typedef enum {
    INTERLACED,
    PROGRESSIVE,
} mode_t;

static const struct cea861_timing {
    const uint16_t hactive;
    const uint16_t vactive;
    mode_t mode;
    const uint16_t htotal;
    const uint16_t hblank;
    const uint16_t vtotal;
    const double   vblank;
    const double   hfreq;
    const double   vfreq;
    const double   pixclk;
} cea861_timings[] = {
    [1]  = {  640,  480, PROGRESSIVE,  800,  160,  525, 45.0,  31.469,  59.940,  25.175 },
    [2]  = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0,  31.469,  59.940,  27.000 },
    [3]  = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0,  31.469,  59.940,  27.000 },
    [4]  = { 1280,  720, PROGRESSIVE, 1650,  370,  750, 30.0,  45.000,  60.000,  74.250 },
    [5]  = { 1920, 1080,  INTERLACED, 2200,  280, 1125, 22.5,  33.750,  60.000,  72.250 },
    [6]  = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  15.734,  59.940,  27.000 },
    [7]  = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  15.734,  59.940,  27.000 },
    [8]  = { 1440,  240, PROGRESSIVE, 1716,  276,  262, 22.0,  15.734,  60.054,  27.000 },  /* 9 */
    [9]  = { 1440,  240, PROGRESSIVE, 1716,  276,  262, 22.0,  15.734,  59.826,  27.000 },  /* 8 */
    [10] = { 2880,  480,  INTERLACED, 3432,  552,  525, 22.5,  15.734,  59.940,  54.000 },
    [11] = { 2880,  480,  INTERLACED, 3432,  552,  525, 22.5,  15.734,  59.940,  54.000 },
    [12] = { 2880,  240, PROGRESSIVE, 3432,  552,  262, 22.0,  15.734,  60.054,  54.000 },  /* 13 */
    [13] = { 2880,  240, PROGRESSIVE, 3432,  552,  262, 22.0,  15.734,  59.826,  54.000 },  /* 12 */
    [14] = { 1440,  480, PROGRESSIVE, 1716,  276,  525, 45.0,  31.469,  59.940,  54.000 },
    [15] = { 1440,  480, PROGRESSIVE, 1716,  276,  525, 45.0,  31.469,  59.940,  54.000 },
    [16] = { 1920, 1080, PROGRESSIVE, 2200,  280, 1125, 45.0,  67.500,  60.000, 148.500 },
    [17] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  31.250,  50.000,  27.000 },
    [18] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  31.250,  50.000,  27.000 },
    [19] = { 1280,  720, PROGRESSIVE, 1980,  700,  750, 30.0,  37.500,  50.000,  74.250 },
    [20] = { 1920, 1080,  INTERLACED, 2640,  720, 1125, 22.5,  28.125,  50.000,  74.250 },
    [21] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  15.625,  50.000,  27.000 },
    [22] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  15.625,  50.000,  27.000 },
    [23] = { 1440,  288, PROGRESSIVE, 1728,  288,  312, 24.0,  15.625,  50.080,  27.000 },  /* 24 */
    [24] = { 1440,  288, PROGRESSIVE, 1728,  288,  313, 25.0,  15.625,  49.920,  27.000 },  /* 23 */
 // [24] = { 1440,  288, PROGRESSIVE, 1728,  288,  314, 26.0,  15.625,  49.761,  27.000 },
    [25] = { 2880,  576,  INTERLACED, 3456,  576,  625, 24.5,  15.625,  50.000,  54.000 },
    [26] = { 2880,  576,  INTERLACED, 3456,  576,  625, 24.5,  15.625,  50.000,  54.000 },
    [27] = { 2880,  288, PROGRESSIVE, 3456,  576,  312, 24.0,  15.625,  50.080,  54.000 },  /* 28 */
    [28] = { 2880,  288, PROGRESSIVE, 3456,  576,  313, 25.0,  15.625,  49.920,  54.000 },  /* 27 */
 // [28] = { 2880,  288, PROGRESSIVE, 3456,  576,  314, 26.0,  15.625,  49.761,  54.000 },
    [29] = { 1440,  576, PROGRESSIVE, 1728,  288,  625, 49.0,  31.250,  50.000,  54.000 },
    [30] = { 1440,  576, PROGRESSIVE, 1728,  288,  625, 49.0,  31.250,  50.000,  54.000 },
    [31] = { 1920, 1080, PROGRESSIVE, 2640,  720, 1125, 45.0,  56.250,  50.000, 148.500 },
    [32] = { 1920, 1080, PROGRESSIVE, 2750,  830, 1125, 45.0,  27.000,  24.000,  74.250 },
    [33] = { 1920, 1080, PROGRESSIVE, 2640,  720, 1125, 45.0,  28.125,  25.000,  74.250 },
    [34] = { 1920, 1080, PROGRESSIVE, 2200,  280, 1125, 45.0,  33.750,  30.000,  74.250 },
    [35] = { 2880,  480, PROGRESSIVE, 3432,  552,  525, 45.0,  31.469,  59.940, 108.500 },
    [36] = { 2880,  480, PROGRESSIVE, 3432,  552,  525, 45.0,  31.469,  59.940, 108.500 },
    [37] = { 2880,  576, PROGRESSIVE, 3456,  576,  625, 49.0,  31.250,  50.000, 108.000 },
    [38] = { 2880,  576, PROGRESSIVE, 3456,  576,  625, 49.0,  31.250,  50.000, 108.000 },
    [39] = { 1920, 1080,  INTERLACED, 2304,  384, 1250, 85.0,  31.250,  50.000,  72.000 },
    [40] = { 1920, 1080,  INTERLACED, 2640,  720, 1125, 22.5,  56.250, 100.000, 148.500 },
    [41] = { 1280,  720, PROGRESSIVE, 1980,  700,  750, 30.0,  75.000, 100.000, 148.500 },
    [42] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  62.500, 100.000,  54.000 },
    [43] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  62.500, 100.000,  54.000 },
    [44] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  31.250, 100.000,  54.000 },
    [45] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  31.250, 100.000,  54.000 },
    [46] = { 1920, 1080,  INTERLACED, 2200,  280, 1125, 22.5,  67.500, 120.000, 148.500 },
    [47] = { 1280,  720, PROGRESSIVE, 1650,  370,  750, 30.0,  90.000, 120.000, 148.500 },
    [48] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0,  62.937, 119.880,  54.000 },
    [49] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0,  62.937, 119.880,  54.000 },
    [50] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  31.469, 119.880,  54.000 },
    [51] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  31.469, 119.880,  54.000 },
    [52] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0, 125.000, 200.000, 108.000 },
    [53] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0, 125.000, 200.000, 108.000 },
    [54] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  62.500, 200.000, 108.000 },
    [55] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  62.500, 200.000, 108.000 },
    [56] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0, 125.874, 239.760, 108.000 },
    [57] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0, 125.874, 239.760, 108.000 },
    [58] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  62.937, 239.760, 108.000 },
    [59] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  62.937, 239.760, 108.000 },
    [60] = { 1280,  720, PROGRESSIVE, 3300, 2020,  750, 30.0,  18.000,  24.000,  59.400 },
    [61] = { 1280,  720, PROGRESSIVE, 3960, 2680,  750, 30.0,  18.750,  25.000,  74.250 },
    [62] = { 1280,  720, PROGRESSIVE, 3300, 2020,  750, 30.0,  22.500,  30.000,  74.250 },
    [63] = { 1920, 1080, PROGRESSIVE, 2200,  280, 1125, 45.0, 135.000, 120.000, 297.000 },
    [64] = { 1920, 1080, PROGRESSIVE, 2640,  720, 1125, 45.0, 112.500, 100.000, 297.000 },
};

static const uint8_t EDID_STANDARD_TIMING_DESCRIPTOR_INVALID[] = { 0x01, 0x01 };
static const uint8_t HDMI_OUI[]                 = { 0x00, 0x0C, 0x03 };
#define EDID_HDR_SIZE 8
static const uint8_t EDID_HEADER[EDID_HDR_SIZE] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

static inline const char *
_aspect_ratio(const uint16_t hres, const uint16_t vres)
{
#define HAS_RATIO_OF(x, y)  (hres == (vres * (x) / (y)) && !((vres * (x)) % (y)))

    if (HAS_RATIO_OF(16, 10))
        return "16:10";
    if (HAS_RATIO_OF(4, 3))
        return "4:3";
    if (HAS_RATIO_OF(5, 4))
        return "5:4";
    if (HAS_RATIO_OF(16, 9))
        return "16:9";

#undef HAS_RATIO

    return "unknown";
}

static inline uint32_t
edid_detailed_timing_pixel_clock(const struct edid_detailed_timing_descriptor * const dtb)
{
    return dtb->pixel_clock * 10000;
}

static inline uint16_t
edid_detailed_timing_horizontal_blanking(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->horizontal_blanking_hi << 8) | dtb->horizontal_blanking_lo;
}

static inline uint16_t
edid_detailed_timing_horizontal_active(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->horizontal_active_hi << 8) | dtb->horizontal_active_lo;
}

static inline uint16_t
edid_detailed_timing_vertical_blanking(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->vertical_blanking_hi << 8) | dtb->vertical_blanking_lo;
}

static inline uint16_t
edid_detailed_timing_vertical_active(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->vertical_active_hi << 8) | dtb->vertical_active_lo;
}

static inline uint8_t
edid_detailed_timing_horizontal_sync_offset(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->horizontal_sync_offset_hi << 4) | dtb->horizontal_sync_offset_lo;
}

static inline uint8_t
edid_detailed_timing_horizontal_sync_pulse_width(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->horizontal_sync_pulse_width_hi << 4) | dtb->horizontal_sync_pulse_width_lo;
}

static inline uint16_t
edid_detailed_timing_horizontal_image_size(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->horizontal_image_size_hi << 8) | dtb->horizontal_image_size_lo;
}

static inline uint16_t
edid_detailed_timing_vertical_image_size(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->vertical_image_size_hi << 8) | dtb->vertical_image_size_lo;
}

static inline uint8_t
edid_detailed_timing_vertical_sync_offset(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->vertical_sync_offset_hi << 4) | dtb->vertical_sync_offset_lo;
}

static inline uint8_t
edid_detailed_timing_vertical_sync_pulse_width(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->vertical_sync_pulse_width_hi << 4) | dtb->vertical_sync_pulse_width_lo;
}

static inline uint8_t
edid_detailed_timing_stereo_mode(const struct edid_detailed_timing_descriptor * const dtb)
{
    return (dtb->stereo_mode_hi << 2 | dtb->stereo_mode_lo);
}

static inline double
edid_gamma(const struct edid * const edid)
{
    return (edid->display_transfer_characteristics + 100) / 100.0;
}

static inline uint32_t
edid_standard_timing_horizontal_active(const struct edid_standard_timing_descriptor * const desc)
{
    return ((desc->horizontal_active_pixels + 31) << 3);
}

static inline uint32_t
edid_standard_timing_vertical_active(const struct edid_standard_timing_descriptor * const desc)
{
    const uint32_t hres = edid_standard_timing_horizontal_active(desc);

    switch (desc->image_aspect_ratio) {
    case EDID_ASPECT_RATIO_16_10:
        return ((hres * 10) >> 4);
    case EDID_ASPECT_RATIO_4_3:
        return ((hres * 3) >> 2);
    case EDID_ASPECT_RATIO_5_4:
        return ((hres << 2) / 5);
    case EDID_ASPECT_RATIO_16_9:
        return ((hres * 9) >> 4);
    }

    return hres;
}

static inline uint32_t
edid_standard_timing_refresh_rate(const struct edid_standard_timing_descriptor * const desc)
{
    return (desc->refresh_rate + 60);
}

static inline float
edid_decode_fixed_point(uint16_t value)
{
    double result = 0.0;

    assert((~value & 0xfc00) == 0xfc00);    /* edid fraction is 10 bits */

    for (uint8_t i = 0; value && (i < 10); i++, value >>= 1)
        result = result + ((value & 0x1) * (1.0 / (1 << (10 - i))));

    return result;
}

void
__edid_timing_extract (hdmi_timing_t *timing, const struct edid_detailed_timing_descriptor * const dtb)
{
    const uint16_t hres = edid_detailed_timing_horizontal_active(dtb);
    const uint16_t vres = edid_detailed_timing_vertical_active(dtb);
    const uint32_t htotal = hres + edid_detailed_timing_horizontal_blanking(dtb);
    const uint32_t vtotal = vres + edid_detailed_timing_vertical_blanking(dtb);

    timing->hres = hres;
    timing->vres = vres;
    timing->htotal = htotal;
    timing->vtotal = vtotal;
    timing->rate_hz = edid_detailed_timing_pixel_clock(dtb) / (vtotal * htotal);
    timing->interlaced = dtb->interlaced;
}

void
__edid_timing_print (hdmi_timing_t *timing)
{
    dprintf("%ux%u%c at %.fHz (%s)\n",
             timing->hres,
             timing->vres,
             timing->interlaced ? 'i' : 'p',
             timing->rate_hz,
             _aspect_ratio(timing->hres, timing->vres));
}


void
__edid_mode_extract(hdmi_timing_t *timing, const struct edid_detailed_timing_descriptor * const dtb)
{
    const uint16_t xres = edid_detailed_timing_horizontal_active(dtb);
    const uint16_t yres = edid_detailed_timing_vertical_active(dtb);
    const uint32_t pixclk = edid_detailed_timing_pixel_clock(dtb);
    const uint16_t lower_margin = edid_detailed_timing_vertical_sync_offset(dtb);
    const uint16_t right_margin = edid_detailed_timing_horizontal_sync_offset(dtb);

    timing->xres            = xres;
    timing->yres            = yres;
    timing->pclk_mhz        = HZ_2_MHZ((double) pixclk);

    timing->hres            = timing->xres;
    timing->hstart          = timing->hres + right_margin;
    timing->hend            = timing->hstart + edid_detailed_timing_horizontal_sync_pulse_width(dtb);
    timing->htotal          = timing->xres + edid_detailed_timing_horizontal_blanking(dtb);

    timing->vres            = timing->yres;
    timing->vstart          = timing->vres + lower_margin;
    timing->vend            = timing->vstart + edid_detailed_timing_vertical_sync_pulse_width(dtb);
    timing->vtotal          = timing->yres + edid_detailed_timing_vertical_blanking(dtb);

    timing->hpol = dtb->signal_pulse_polarity ? '+' : '-';
    timing->vpol = dtb->signal_serration_polarity ? '+' : '-';
}

void
__edid_mode_print (hdmi_timing_t *timing)
{
    dprintf("   \"%ux%u\" %.3f MHz\n", timing->xres, timing->yres, timing->pclk_mhz);
    dprintf("    horizontal timings :\n");
    dprintf("      hres= %d, hstart= %d, hend= %d, htotal= %d\n",
            timing->hres, timing->hstart, timing->hend, timing->htotal);
    dprintf("    vertical timings :\n");
    dprintf("      vres= %d, vstart= %d, vend= %d, vtotal= %d\n",
            timing->vres, timing->vstart, timing->vend, timing->vtotal);
    dprintf("    %chpol, %cvpol\n", timing->hpol, timing->vpol);
}

static inline void
edid_manufacturer(const struct edid * const edid, char manufacturer[4])
{
    manufacturer[0] = '@' + ((edid->manufacturer & 0x007c) >> 2);
    manufacturer[1] = '@' + (((edid->manufacturer & 0x0003) >> 00) << 3)
                          | (((edid->manufacturer & 0xe000) >> 13) << 0);
    manufacturer[2] = '@' + ((edid->manufacturer & 0x1f00) >> 8);
    manufacturer[3] = '\0';
}

static inline struct edid_color_characteristics_data
edid_color_characteristics(const struct edid * const edid)
{
    const struct edid_color_characteristics_data characteristics = {
        .red = {
            .x = (edid->red_x << 2) | edid->red_x_low,
            .y = (edid->red_y << 2) | edid->red_y_low,
        },
        .green = {
            .x = (edid->green_x << 2) | edid->green_x_low,
            .y = (edid->green_y << 2) | edid->green_y_low,
        },
        .blue = {
            .x = (edid->blue_x << 2) | edid->blue_x_low,
            .y = (edid->blue_y << 2) | edid->blue_y_low,
        },
        .white = {
            .x = (edid->white_x << 2) | edid->white_x_low,
            .y = (edid->white_y << 2) | edid->white_y_low,
        },
    };

    return characteristics;
}

static inline d_bool
edid_detailed_timing_is_monitor_descriptor(const struct edid * const edid,
                                           const uint8_t timing)
{
    const struct edid_monitor_descriptor * const mon =
        &edid->detailed_timings[timing].monitor;

    assert(timing < arrlen(edid->detailed_timings));

    return mon->flag0 == 0x00 && mon->flag1 == 0x00 && mon->flag2 == 0x00;
}


static void
dump_section(const char * const name,
             const uint8_t * const buffer,
             const uint8_t offset,
             const uint8_t length)
{
    const uint8_t *value = buffer + offset;
    uint8_t i;

    dprintf("%s: ", name);

    for (i = 0; i < length; i++) {
        dprintf("%02x ", *value++);
    }

    dprintf("\b\n");
}

static void
dump_edid1(const uint8_t * const buffer)
{
    dprintf("Raw dump :\n");
    dump_section("header",                            buffer, 0x00, 0x08);
    dump_section("vendor/product identification",     buffer, 0x08, 0x0a);
    dump_section("edid struct version/revision",      buffer, 0x12, 0x02);
    dump_section("basic display parameters/features", buffer, 0x14, 0x05);
    dump_section("color characteristics",             buffer, 0x19, 0x0a);
    dump_section("established timings",               buffer, 0x23, 0x03);
    dump_section("standard timing identification",    buffer, 0x26, 0x10);
    dump_section("detailed timing 0",                 buffer, 0x36, 0x12);
    dump_section("detailed timing 1",                 buffer, 0x48, 0x12);
    dump_section("detailed timing 2",                 buffer, 0x5a, 0x12);
    dump_section("detailed timing 3",                 buffer, 0x6c, 0x12);
    dump_section("extensions",                        buffer, 0x7e, 0x01);
    dump_section("checksum",                          buffer, 0x7f, 0x01);

    dprintf("\n");
}

static void
dump_cea861(const uint8_t * const buffer)
{
    const struct edid_detailed_timing_descriptor *dtd = NULL;
    const struct cea861_timing_block * const ctb =
        (struct cea861_timing_block *) buffer;
    const uint8_t dof = offsetof(struct cea861_timing_block, data);

    dump_section("cea extension header",  buffer, 0x00, 0x04);

    if (ctb->dtd_offset - dof)
        dump_section("data block collection", buffer, 0x04, ctb->dtd_offset - dof);

    dtd = (struct edid_detailed_timing_descriptor *) (buffer + ctb->dtd_offset);
    for (uint8_t i = 0; dtd->pixel_clock; i++, dtd++) {
        char header[128];

        snprintf(header, sizeof(header), "detailed timing descriptor %03u", i);
        dump_section(header, (uint8_t *) dtd, 0x00, sizeof(*dtd));
    }

    dump_section("padding",  buffer, (uint8_t *) dtd - buffer,
                 dof + sizeof(ctb->data) - ((uint8_t *) dtd - buffer));
    dump_section("checksum", buffer, 0x7f, 0x01);

    dprintf("\n");
}

static void
print_edid1(hdmi_timing_t *timing, const struct edid * const edid)
{
    const struct edid_monitor_range_limits *monitor_range_limits = NULL;
    edid_monitor_descriptor_string monitor_serial_number = {0};
    edid_monitor_descriptor_string monitor_model_name = {0};
    d_bool has_ascii_string = d_false;
    char manufacturer[4] = {0};
    char *p_c;

    struct edid_color_characteristics_data characteristics;
    const uint8_t vlen = edid->maximum_vertical_image_size;
    const uint8_t hlen = edid->maximum_horizontal_image_size;
    uint8_t i;

    static const char * const display_type[] = {
        [EDID_DISPLAY_TYPE_MONOCHROME] = "Monochrome or greyscale",
        [EDID_DISPLAY_TYPE_RGB]        = "sRGB colour",
        [EDID_DISPLAY_TYPE_NON_RGB]    = "Non-sRGB colour",
        [EDID_DISPLAY_TYPE_UNDEFINED]  = "Undefined",
    };

    edid_manufacturer(edid, manufacturer);
    characteristics = edid_color_characteristics(edid);


    for (i = 0; i < arrlen(edid->detailed_timings); i++) {
        const struct edid_monitor_descriptor * const mon =
            &edid->detailed_timings[i].monitor;

        if (!edid_detailed_timing_is_monitor_descriptor(edid, i))
            continue;

        switch (mon->tag) {
        case EDID_MONTIOR_DESCRIPTOR_MANUFACTURER_DEFINED:
            /* This is arbitrary data, just silently ignore it. */
            break;
        case EDID_MONITOR_DESCRIPTOR_ASCII_STRING:
            has_ascii_string = d_true;
            break;
        case EDID_MONITOR_DESCRIPTOR_MONITOR_NAME:
            strncpy(monitor_model_name, (char *) mon->data,
                    sizeof(monitor_model_name) - 1);
            p_c = (char *)strchrnul(monitor_model_name, '\n');
            *p_c = '\0';
            break;
        case EDID_MONITOR_DESCRIPTOR_MONITOR_RANGE_LIMITS:
            monitor_range_limits = (struct edid_monitor_range_limits *) &mon->data;
            break;
        case EDID_MONITOR_DESCRIPTOR_MONITOR_SERIAL_NUMBER:
            strncpy(monitor_serial_number, (char *) mon->data,
                    sizeof(monitor_serial_number) - 1);
            p_c = (char *)strchrnul(monitor_serial_number, '\n');
            *p_c = '\0';
            break;
        default:
            dprintf("unknown monitor descriptor type 0x%02x\n", mon->tag);
            break;
        }
    }

    dprintf("Monitor\n");

    dprintf("  Model name............... %s\n",
           *monitor_model_name ? monitor_model_name : "n/a");

    dprintf("  Manufacturer............. %s\n",
           manufacturer);

    dprintf("  Product code............. %u\n",
           *(uint16_t *) edid->product);

    if (*(uint32_t *) edid->serial_number)
        dprintf("  Module serial number..... %u\n",
               *(uint32_t *) edid->serial_number);

#if defined(DISPLAY_UNKNOWN)
    dprintf("  Plug and Play ID......... %s\n", NULL);
#endif

    dprintf("  Serial number............ %s\n",
           *monitor_serial_number ? monitor_serial_number : "n/a");

    dprintf("  Manufacture date......... %u", edid->manufacture_year + 1990);
    if (edid->manufacture_week <= 52)
        dprintf(", ISO week %u", edid->manufacture_week);
    dprintf("\n");

    dprintf("  EDID revision............ %u.%u\n",
           edid->version, edid->revision);

    dprintf("  Input signal type........ %s\n",
           edid->video_input_definition.digital.digital ? "Digital" : "Analog");

    if (edid->video_input_definition.digital.digital) {
        dprintf("  VESA DFP 1.x supported... %s\n",
               edid->video_input_definition.digital.dfp_1x ? "Yes" : "No");
    } else {
        /* TODO print analog flags */
    }

#if defined(DISPLAY_UNKNOWN)
    dprintf("  Color bit depth.......... %s\n", NULL);
#endif

    dprintf("  Display type............. %s\n",
           display_type[edid->feature_support.display_type]);

    dprintf("  Screen size.............. %u mm x %u mm (%.1f in)\n",
           CM_2_MM(hlen), CM_2_MM(vlen),
           CM_2_IN(sqrt(hlen * hlen + vlen * vlen)));

    dprintf("  Power management......... %s%s%s%s\n",
           edid->feature_support.active_off ? "Active off, " : "",
           edid->feature_support.suspend ? "Suspend, " : "",
           edid->feature_support.standby ? "Standby, " : "",

           (edid->feature_support.active_off ||
            edid->feature_support.suspend    ||
            edid->feature_support.standby) ? "\b\b  " : "n/a");

    dprintf("  Extension blocks......... %u\n",
           edid->extensions);

#if defined(DISPLAY_UNKNOWN)
    dprintf("  DDC/CI................... %s\n", NULL);
#endif

    dprintf("\n");

    if (has_ascii_string) {
        edid_monitor_descriptor_string string = {0};

        dprintf("General purpose ASCII string\n");

        for (i = 0; i < arrlen(edid->detailed_timings); i++) {
            const struct edid_monitor_descriptor * const mon =
                &edid->detailed_timings[i].monitor;

            if (!edid_detailed_timing_is_monitor_descriptor(edid, i))
                continue;

            if (mon->tag == EDID_MONITOR_DESCRIPTOR_ASCII_STRING) {
                strncpy(string, (char *) mon->data, sizeof(string) - 1);
                p_c = (char *)strchrnul(string, '\n');
                *p_c = '\0';

                dprintf("  ASCII string............. %s\n", string);
            }
        }

        dprintf("\n");
    }

    dprintf("Color characteristics\n");

    dprintf("  Default color space...... %ssRGB\n",
           edid->feature_support.standard_default_color_space ? "" : "Non-");

    dprintf("  Display gamma............ %.2f\n",
           edid_gamma(edid));

    dprintf("  Red chromaticity......... Rx %0.3f - Ry %0.3f\n",
           edid_decode_fixed_point(characteristics.red.x),
           edid_decode_fixed_point(characteristics.red.y));

    dprintf("  Green chromaticity....... Gx %0.3f - Gy %0.3f\n",
           edid_decode_fixed_point(characteristics.green.x),
           edid_decode_fixed_point(characteristics.green.y));

    dprintf("  Blue chromaticity........ Bx %0.3f - By %0.3f\n",
           edid_decode_fixed_point(characteristics.blue.x),
           edid_decode_fixed_point(characteristics.blue.y));

    dprintf("  White point (default).... Wx %0.3f - Wy %0.3f\n",
           edid_decode_fixed_point(characteristics.white.x),
           edid_decode_fixed_point(characteristics.white.y));

#if defined(DISPLAY_UNKNOWN)
    dprintf("  Additional descriptors... %s\n", NULL);
#endif

    dprintf("\n");

    dprintf("Timing characteristics\n");

    if (monitor_range_limits) {
        dprintf("  Horizontal scan range.... %u - %u kHz\n",
               monitor_range_limits->minimum_horizontal_rate,
               monitor_range_limits->maximum_horizontal_rate);

        dprintf("  Vertical scan range...... %u - %u Hz\n",
               monitor_range_limits->minimum_vertical_rate,
               monitor_range_limits->maximum_vertical_rate);

        dprintf("  Video bandwidth.......... %u MHz\n",
               monitor_range_limits->maximum_supported_pixel_clock * 10);
    }

#if defined(DISPLAY_UNKNOWN)
    dprintf("  CVT standard............. %s\n", NULL);
#endif

    dprintf("  GTF standard............. %sSupported\n",
           edid->feature_support.default_gtf ? "" : "Not ");

#if defined(DISPLAY_UNKNOWN)
    dprintf("  Additional descriptors... %s\n", NULL);
#endif

    dprintf("  Preferred timing......... %s\n",
           edid->feature_support.preferred_timing_mode ? "Yes" : "No");

    if (edid->feature_support.preferred_timing_mode) { 

        //_edid_timing_string(string, sizeof(string), &edid->detailed_timings[0].timing);
        dprintf("  Native/preferred timing..\n");
        __edid_timing_extract(timing, &edid->detailed_timings[0].timing);
        __edid_timing_print(timing);

        //_edid_mode_string(string, sizeof(string), &edid->detailed_timings[0].timing);
        dprintf("  Modeline...............\n");
        __edid_mode_extract(timing, &edid->detailed_timings[0].timing);
        __edid_mode_print(timing);
    } else {
        dprintf("  Native/preferred timing.. n/a\n");
    }

    dprintf("\n");

    dprintf("Standard timings supported\n");
    if (edid->established_timings.timing_720x400_70) {
        timing->std.timing_720x400_70 = 1;
        dprintf("   720 x  400p @ 70Hz - IBM VGA\n");
    }
    if (edid->established_timings.timing_720x400_88) {
        timing->std.timing_720x400_88 = 1;
        dprintf("   720 x  400p @ 88Hz - IBM XGA2\n");
    }
    if (edid->established_timings.timing_640x480_60) {
        timing->std.timing_640x480_60 = 1;
        dprintf("   640 x  480p @ 60Hz - IBM VGA\n");
    }
    if (edid->established_timings.timing_640x480_67) {
        timing->std.timing_640x480_67 = 1;
        dprintf("   640 x  480p @ 67Hz - Apple Mac II\n");
    }
    if (edid->established_timings.timing_640x480_72) {
        timing->std.timing_640x480_72 = 1;
        dprintf("   640 x  480p @ 72Hz - VESA\n");
    }
    if (edid->established_timings.timing_640x480_75) {
        timing->std.timing_640x480_75 = 1;
        dprintf("   640 x  480p @ 75Hz - VESA\n");
    }
    if (edid->established_timings.timing_800x600_56) {
        timing->std.timing_800x600_56 = 1;
        dprintf("   800 x  600p @ 56Hz - VESA\n");
    }
    if (edid->established_timings.timing_800x600_60) {
        timing->std.timing_800x600_60 = 1;
        dprintf("   800 x  600p @ 60Hz - VESA\n");
    }
    if (edid->established_timings.timing_800x600_72) {
        timing->std.timing_800x600_72 = 1;
        dprintf("   800 x  600p @ 72Hz - VESA\n");
    }
    if (edid->established_timings.timing_800x600_75) {
        timing->std.timing_800x600_75 = 1;
        dprintf("   800 x  600p @ 75Hz - VESA\n");
    }
    if (edid->established_timings.timing_832x624_75) {
        timing->std.timing_832x624_75 = 1;
        dprintf("   832 x  624p @ 75Hz - Apple Mac II\n");
    }
    if (edid->established_timings.timing_1024x768_87) {
        timing->std.timing_1024x768_87 = 1;
        dprintf("  1024 x  768i @ 87Hz - VESA\n");
    }
    if (edid->established_timings.timing_1024x768_60) {
        timing->std.timing_1024x768_60 = 1;
        dprintf("  1024 x  768p @ 60Hz - VESA\n");
    }
    if (edid->established_timings.timing_1024x768_70) {
        timing->std.timing_1024x768_70 = 1;
        dprintf("  1024 x  768p @ 70Hz - VESA\n");
    }
    if (edid->established_timings.timing_1024x768_75) {
        timing->std.timing_1024x768_75 = 1;
        dprintf("  1024 x  768p @ 75Hz - VESA\n");
    }
    if (edid->established_timings.timing_1280x1024_75) {
        timing->std.timing_1280x1024_75 = 1;
        dprintf("  1280 x 1024p @ 75Hz - VESA\n");
    }


    for (i = 0; i < arrlen(edid->standard_timing_id); i++) {
        const struct edid_standard_timing_descriptor * const desc =
            &edid->standard_timing_id[i];

        if (!memcmp(desc, EDID_STANDARD_TIMING_DESCRIPTOR_INVALID, sizeof(*desc)))
            continue;

        dprintf("  %4u x %4u%c @ %uHz - VESA STD\n",
               edid_standard_timing_horizontal_active(desc),
               edid_standard_timing_vertical_active(desc),
               'p',
               edid_standard_timing_refresh_rate(desc));
    }

    dprintf("\n");
}

static inline void
disp_cea861_audio_data(const struct cea861_audio_data_block * const adb)
{
    const uint8_t descriptors = adb->header.length / sizeof(*adb->sad);

    dprintf("CE audio data (formats supported)\n");
    for (uint8_t i = 0; i < descriptors; i++) {
        const struct cea861_short_audio_descriptor * const sad =
            (struct cea861_short_audio_descriptor *) &adb->sad[i];

        switch (sad->audio_format) {
        case CEA861_AUDIO_FORMAT_LPCM:
            dprintf("  LPCM    %u-channel, %s%s%s\b%s",
                   sad->channels + 1,
                   sad->flags.lpcm.bitrate_16_bit ? "16/" : "",
                   sad->flags.lpcm.bitrate_20_bit ? "20/" : "",
                   sad->flags.lpcm.bitrate_24_bit ? "24/" : "",

                   ((sad->flags.lpcm.bitrate_16_bit +
                     sad->flags.lpcm.bitrate_20_bit +
                     sad->flags.lpcm.bitrate_24_bit) > 1) ? " bit depths" : "-bit");
            break;
        case CEA861_AUDIO_FORMAT_AC_3:
            dprintf("  AC-3    %u-channel, %4uk max. bit rate",
                   sad->channels + 1,
                   (sad->flags.maximum_bit_rate << 3));
            break;
        default:
            dprintf("unknown audio format 0x%02x\n",
                    sad->audio_format);
            continue;
        }

        dprintf(" at %s%s%s%s%s%s%s\b kHz\n",
               sad->sample_rate_32_kHz ? "32/" : "",
               sad->sample_rate_44_1_kHz ? "44.1/" : "",
               sad->sample_rate_48_kHz ? "48/" : "",
               sad->sample_rate_88_2_kHz ? "88.2/" : "",
               sad->sample_rate_96_kHz ? "96/" : "",
               sad->sample_rate_176_4_kHz ? "176.4/" : "",
               sad->sample_rate_192_kHz ? "192/" : "");
    }

    dprintf("\n");
}


static inline void
disp_cea861_video_data(const struct cea861_video_data_block * const vdb)
{
    dprintf("CE video identifiers (VICs) - timing/formats supported\n");
    for (uint8_t i = 0; i < vdb->header.length; i++) {
        const struct cea861_timing * const timing =
            &cea861_timings[vdb->svd[i].video_identification_code];

        dprintf(" %s CEA Mode %02u: %4u x %4u%c @ %.fHz\n",
               vdb->svd[i].native ? "*" : " ",
               vdb->svd[i].video_identification_code,
               timing->hactive, timing->vactive,
               (timing->mode == INTERLACED) ? 'i' : 'p',
               timing->vfreq);
    }

    dprintf("\n");
}

static inline void
disp_cea861_vendor_data(const struct cea861_vendor_specific_data_block * vsdb)
{
    const uint8_t oui[] = { vsdb->ieee_registration[2],
                            vsdb->ieee_registration[1],
                            vsdb->ieee_registration[0] };

    dprintf("CEA vendor specific data (VSDB)\n");
    dprintf("  IEEE registration number. 0x");
    for (uint8_t i = 0; i < arrlen(oui); i++)
        dprintf("%02X", oui[i]);
    dprintf("\n");

    if (!memcmp(oui, HDMI_OUI, sizeof(oui))) {
        const struct hdmi_vendor_specific_data_block * const hdmi =
            (struct hdmi_vendor_specific_data_block *) vsdb;

        dprintf("  CEC physical address..... %u.%u.%u.%u\n",
               hdmi->port_configuration_a,
               hdmi->port_configuration_b,
               hdmi->port_configuration_c,
               hdmi->port_configuration_d);

        if (hdmi->header.length >= HDMI_VSDB_EXTENSION_FLAGS_OFFSET) {
            dprintf("  Supports AI (ACP, ISRC).. %s\n",
                   hdmi->audio_info_frame ? "Yes" : "No");
            dprintf("  Supports 48bpp........... %s\n",
                   hdmi->colour_depth_48_bit ? "Yes" : "No");
            dprintf("  Supports 36bpp........... %s\n",
                   hdmi->colour_depth_36_bit ? "Yes" : "No");
            dprintf("  Supports 30bpp........... %s\n",
                   hdmi->colour_depth_30_bit ? "Yes" : "No");
            dprintf("  Supports YCbCr 4:4:4..... %s\n",
                   hdmi->yuv_444_supported ? "Yes" : "No");
            dprintf("  Supports dual-link DVI... %s\n",
                   hdmi->dvi_dual_link ? "Yes" : "No");
        }

        if (hdmi->header.length >= HDMI_VSDB_MAX_TMDS_OFFSET) {
            if (hdmi->max_tmds_clock)
                dprintf("  Maximum TMDS clock....... %uMHz\n",
                       hdmi->max_tmds_clock * 5);
            else
                dprintf("  Maximum TMDS clock....... n/a\n");
        }

        if (hdmi->header.length >= HDMI_VSDB_LATENCY_FIELDS_OFFSET) {
            if (hdmi->latency_fields) {
                dprintf("  Video latency %s........ %ums\n",
                       hdmi->interlaced_latency_fields ? "(p)" : "...",
                       (hdmi->video_latency - 1) << 1);
                dprintf("  Audio latency %s........ %ums\n",
                       hdmi->interlaced_latency_fields ? "(p)" : "...",
                       (hdmi->audio_latency - 1) << 1);
            }

            if (hdmi->interlaced_latency_fields) {
                dprintf("  Video latency (i)........ %ums\n",
                       hdmi->interlaced_video_latency);
                dprintf("  Audio latency (i)........ %ums\n",
                       hdmi->interlaced_audio_latency);
            }
        }
    }

    dprintf("\n");
}

static inline void
disp_cea861_speaker_allocation_data(const struct cea861_speaker_allocation_data_block * const sadb)
{
    const struct cea861_speaker_allocation * const sa = &sadb->payload;
    const uint8_t * const channel_configuration = (uint8_t *) sa;

    dprintf("CEA speaker allocation data\n");
    dprintf("  Channel configuration.... %u.%u\n",
           (__builtin_popcountll(channel_configuration[0] & 0xe9) << 1) +
           (__builtin_popcountll(channel_configuration[0] & 0x14) << 0) +
           (__builtin_popcountll(channel_configuration[1] & 0x01) << 1) +
           (__builtin_popcountll(channel_configuration[1] & 0x06) << 0),
           (channel_configuration[0] & 0x02));
    dprintf("  Front left/right......... %s\n",
           sa->front_left_right ? "Yes" : "No");
    dprintf("  Front LFE................ %s\n",
           sa->front_lfe ? "Yes" : "No");
    dprintf("  Front center............. %s\n",
           sa->front_center ? "Yes" : "No");
    dprintf("  Rear left/right.......... %s\n",
           sa->rear_left_right ? "Yes" : "No");
    dprintf("  Rear center.............. %s\n",
           sa->rear_center ? "Yes" : "No");
    dprintf("  Front left/right center.. %s\n",
           sa->front_left_right_center ? "Yes" : "No");
    dprintf("  Rear left/right center... %s\n",
           sa->rear_left_right_center ? "Yes" : "No");
    dprintf("  Front left/right wide.... %s\n",
           sa->front_left_right_wide ? "Yes" : "No");
    dprintf("  Front left/right high.... %s\n",
           sa->front_left_right_high ? "Yes" : "No");
    dprintf("  Top center............... %s\n",
           sa->top_center ? "Yes" : "No");
    dprintf("  Front center high........ %s\n",
           sa->front_center_high ? "Yes" : "No");

    dprintf("\n");
}

static void
disp_cea861(const struct edid_extension * const ext)
{
    const struct edid_detailed_timing_descriptor *dtd = NULL;
    const struct cea861_timing_block * const ctb =
        (struct cea861_timing_block *) ext;
    const uint8_t offset = offsetof(struct cea861_timing_block, data);
    hdmi_timing_t timing;
    uint8_t index = 0, i;

    /*! \todo handle invalid revision */

    dprintf("CEA-861 Information\n");
    dprintf("  Revision number.......... %u\n",
           ctb->revision);

    if (ctb->revision >= 2) {
        dprintf("  IT underscan............. %supported\n",
               ctb->underscan_supported ? "S" : "Not s");
        dprintf("  Basic audio.............. %supported\n",
               ctb->basic_audio_supported ? "S" : "Not s");
        dprintf("  YCbCr 4:4:4.............. %supported\n",
               ctb->yuv_444_supported ? "S" : "Not s");
        dprintf("  YCbCr 4:2:2.............. %supported\n",
               ctb->yuv_422_supported ? "S" : "Not s");
        dprintf("  Native formats........... %u\n",
               ctb->native_dtds);
    }

    dtd = (struct edid_detailed_timing_descriptor *) ((uint8_t *) ctb + ctb->dtd_offset);
    for (i = 0; dtd->pixel_clock; i++, dtd++) { 
        /*! \todo ensure that we are not overstepping bounds */

        //did_timing_string(string, sizeof(string), dtd);
        dprintf("  Detailed timing #%u.......\n", i + 1);
        memset(&timing, 0, sizeof(timing));
        __edid_timing_extract(&timing, dtd);
        __edid_timing_print(&timing);

        //_edid_mode_string(string, sizeof(string), dtd);
        dprintf("  Modeline...............\n");
        memset(&timing, 0, sizeof(timing));
        __edid_mode_extract(&timing, dtd);
        __edid_mode_print(&timing);
    }

    dprintf("\n");

    if (ctb->revision >= 3) {
        do {
            const struct cea861_data_block_header * const header =
                (struct cea861_data_block_header *) &ctb->data[index];

            switch (header->tag) {
            case CEA861_DATA_BLOCK_TYPE_AUDIO:
                {
                    const struct cea861_audio_data_block * const db =
                        (struct cea861_audio_data_block *) header;

                    disp_cea861_audio_data(db);
                }
                break;
            case CEA861_DATA_BLOCK_TYPE_VIDEO:
                {
                    const struct cea861_video_data_block * const db =
                        (struct cea861_video_data_block *) header;

                    disp_cea861_video_data(db);
                }
                break;
            case CEA861_DATA_BLOCK_TYPE_VENDOR_SPECIFIC:
                {
                    const struct cea861_vendor_specific_data_block * const db =
                        (struct cea861_vendor_specific_data_block *) header;

                    disp_cea861_vendor_data(db);
                }
                break;
            case CEA861_DATA_BLOCK_TYPE_SPEAKER_ALLOCATION:
                {
                    const struct cea861_speaker_allocation_data_block * const db =
                        (struct cea861_speaker_allocation_data_block *) header;

                    disp_cea861_speaker_allocation_data(db);
                }
                break;
            default:
                dprintf("unknown CEA-861 data block type 0x%02x\n", header->tag);
                break;
            }

            index = index + header->length + sizeof(*header);
        } while (index < ctb->dtd_offset - offset);
    }

    dprintf("\n");
}


struct edid_extension_handler {
    void (* const hex_dump)(const uint8_t * const);
    void (* const inf_disp)(const struct edid_extension * const);
};

static struct edid_extension_handler edid_extension_handlers[] = {
    [EDID_EXTENSION_CEA] = { dump_cea861, disp_cea861 },
};

static void
parse_edid(hdmi_timing_t *timing, const uint8_t * const data)
{
    const struct edid * const edid = (struct edid *) data;
    const struct edid_extension * const extensions =
        (struct edid_extension *) (data + sizeof(*edid));

    assert(!memcmp(EDID_HEADER, data, arrlen(EDID_HEADER)));

    dump_edid1((uint8_t *) edid);
    print_edid1(timing, edid);

    for (uint8_t i = 0; i < edid->extensions; i++) {
        const struct edid_extension * const extension = &extensions[i];
        const struct edid_extension_handler * const handler =
            &edid_extension_handlers[extension->tag];

        if (!handler) {
            dprintf("WARNING: block %u contains unknown extension (%#04x)\n",
                    i, extensions[i].tag);
            continue;
        }

        if (handler->hex_dump)
            (*handler->hex_dump)((uint8_t *) extension);

        if (handler->inf_disp)
            (*handler->inf_disp)(extension);
    }
}

int hdmi_parse_edid (hdmi_timing_t *timing, hdmi_edid_seg_t *edid, int size)
{
    dprintf("\nEDID Parse enter\n\n");

    parse_edid(timing, edid->raw);

    dprintf("\nEDID Parse exit\n\n");
    return 0;
}

#endif /*defined(USE_LCD_HDMI)*/

#endif

