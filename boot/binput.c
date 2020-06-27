#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <debug.h>
#include <stdint.h>
#include <heap.h>
#include <bsp_sys.h>
#include <bsp_cmd.h>
#include <misc_utils.h>

#include "../../common/int/boot_int.h"

#include <input_main.h>
#include <gui.h>

static i_event_t *
b_deliver_input (i_event_t  *evts, i_event_t *event);

int boot_input_init (void)
{
    static const kbdmap_t kbdmap[JOY_STD_MAX] =
    {
        {GUI_KEY_1, PAD_FREQ_LOW},
        {GUI_KEY_2, 0},
        {GUI_KEY_3, 0},
        {GUI_KEY_4, PAD_FREQ_LOW},
        {GUI_KEY_5, 0},
        {GUI_KEY_6, 0},
        {GUI_KEY_7, 0},
        {GUI_KEY_8, 0},
        {GUI_KEY_RETURN, 0},
        {GUI_KEY_SELECT, PAD_FREQ_LOW},
        {GUI_KEY_UP, 0},
        {GUI_KEY_DOWN, 0},
        {GUI_KEY_LEFT, 0},
        {GUI_KEY_RIGHT, 0},
    };
    input_soft_init(b_deliver_input, kbdmap);
    return 0;
}

static i_event_t *b_deliver_input (i_event_t  *evts, i_event_t *event)
{
    static uint32_t inpost_tsf = 0;
    gevt_t evt;

    if (event->state == keydown) {
        if (d_rlimit_wrap(&inpost_tsf, 300) == 0) {
            return NULL;
        }
    }

    evt.p.x = event->x;
    evt.p.y = event->y;
    evt.e = event->state == keydown ? GUIACT : GUIRELEASE;
    evt.sym = event->sym;
    evt.symbolic = 0;

    if (event->x == 0 && event->y == 0) {
        evt.symbolic = 1;
    }

    boot_deliver_input_event(&evt);
    return NULL;
}

