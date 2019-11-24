
#include <misc_utils.h>
#include <hdmi_pub.h>

void SCREEN_SetDefTiming 
            (hdmi_timing_t *timing,
            const SCREEN_Std_FormatTypeDef *std)
{
    timing->hres    = std->hact;
    timing->hstart  = std->hstart;
    timing->hend    = std->hend;
    timing->htotal  = std->htotal;
    timing->hpol    = std->hpol;
    timing->vres    = std->vact;
    timing->vstart  = std->vstart;
    timing->vend    = std->vend;
    timing->vtotal  = std->vtotal;
    timing->vpol    = std->vpol;
}

SCREEN_Std_FormatTypeDef HDMI_Format_VESA_640_480_60 = 
{ 25.2f, 640, 656, 752, 800, 480, 490, 492, 525, '-', '-',};

SCREEN_Std_FormatTypeDef HDMI_Format_VESA_640_480_75 =
{ 31.5f, 640, 656, 720, 840, 480, 481, 484, 500, '-', '-', };

SCREEN_Std_FormatTypeDef HDMI_Format_VESA_800_600_60 =
{ 40.0f, 800, 840, 968, 1056, 600, 601, 605, 628, '+', '+', };

SCREEN_Std_FormatTypeDef HDMI_Format_VESA_720_400_70 =
{ 35.5f, 720, 756, 828, 936, 400, 401, 404, 446, '-', '+', };

SCREEN_Std_FormatTypeDef HDMI_Format_VESA_720_480_30 =
{ 30.0f, 720, 744, 788, 880, 480, 490, 493, 525, '-', '+', };

SCREEN_Std_FormatTypeDef *
SCREEN_GetStdConf (hdmi_timing_t *timing, hdmi_std_timing_t pref)
{
    if (timing->std.timing_720x400_70 && pref.timing_720x400_70) {
        return &HDMI_Format_VESA_720_400_70;
    } else if (timing->std.timing_720x400_88 && pref.timing_720x400_88) {

    } else if (timing->std.timing_720x480_30 && pref.timing_720x480_30) {
        return &HDMI_Format_VESA_720_480_30;
    } else if (timing->std.timing_640x480_60 && pref.timing_640x480_60) {
        return &HDMI_Format_VESA_640_480_60;
    } else if (timing->std.timing_640x480_67 && pref.timing_640x480_67) {

    } else if (timing->std.timing_640x480_72 && pref.timing_640x480_72) {

    } else if (timing->std.timing_640x480_75 && pref.timing_640x480_75) {
        return &HDMI_Format_VESA_640_480_75;
    } else if (timing->std.timing_800x600_56 && pref.timing_800x600_56) {

    } else if (timing->std.timing_800x600_60 && pref.timing_800x600_60) {
        return &HDMI_Format_VESA_800_600_60;
    } else if (timing->std.timing_800x600_72 && pref.timing_800x600_72) {

    } else if (timing->std.timing_800x600_75 && pref.timing_800x600_75) {

    }
    return NULL;
}


