#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <input_main.h>
#include <bsp_cmd.h>
#include <debug.h>
#include <dev_io.h>
#include <gfx.h>
#include <gfx2d_mem.h>

#include <input_main.h>
#include "../../common/int/input_int.h"

#include <lcd_main.h>
#include "../../common/int/lcd_int.h"

#define TSENS_SLEEP_TIME 250
#define JOY_FREEZE_TIME 150/*ms*/

extern int joypad_read (int8_t *pads);

static int bsp_input_errno = 0;

static kbdmap_t input_kbdmap[JOY_STD_MAX];

static input_evt_handler_t user_handler = NULL;

#define input_post_key(a, b) {              \
    user_handler ? user_handler(a, b) : NULL; \
}

static uint8_t ts_freeze_ticks = 0;

static uint32_t joypad_timestamp = 0;

static int ts_screen_zones[2][3];
static uint8_t ts_zones_keymap[3][4];

/*---1--- 2--- 3--- 4---*/
/*
--1   up      ent    esc   sr
    |
--2  left     sl     use     right
    |
--3  down  fire  wpn r   map
*/

static int ts_attach_keys (uint8_t tsmap[3][4], const kbdmap_t kbdmap[JOY_STD_MAX])
{
    int xmax, ymax, x_keyzone_size, y_keyzone_size;
    screen_t screen;

    tsmap[0][0] = kbdmap[JOY_UPARROW].key;
    tsmap[0][1] = kbdmap[JOY_K10].key;
    tsmap[0][2] = kbdmap[JOY_K9].key;
    tsmap[0][3] = kbdmap[JOY_K8].key;

    tsmap[1][0] = kbdmap[JOY_LEFTARROW].key;
    tsmap[1][1] = kbdmap[JOY_K7].key;
    tsmap[1][2] = kbdmap[JOY_K4].key;
    tsmap[1][3] = kbdmap[JOY_RIGHTARROW].key;

    tsmap[2][0] = kbdmap[JOY_DOWNARROW].key;
    tsmap[2][1] = kbdmap[JOY_K1].key;
    tsmap[2][2] = kbdmap[JOY_K3].key;
    tsmap[2][3] = kbdmap[JOY_K2].key;

    vid_wh(&screen);
    xmax = screen.width;
    ymax = screen.height;
    x_keyzone_size = xmax / 4;
    y_keyzone_size = ymax / 3;

    ts_screen_zones[0][0] = x_keyzone_size;
    ts_screen_zones[0][1] = x_keyzone_size * 2;
    ts_screen_zones[0][2] = x_keyzone_size * 3;

    ts_screen_zones[1][0] = y_keyzone_size;
    ts_screen_zones[1][1] = y_keyzone_size * 2;
    ts_screen_zones[1][2] = 0; /*???*/
    return 0;
}

static int
ts_get_key (int x, int y)
{
    int row = 2, col = 3;
    for (int i = 0; i < 3; i++) {
        if (x < ts_screen_zones[0][i]) {
            col = i;
            break;
        }
    }
    for (int i = 0; i < 2; i++) {
        if (y < ts_screen_zones[1][i]) {
            row = i;
            break;
        }
    }
    return ts_zones_keymap[row][col];
}

d_bool input_is_touch_avail (void)
{
    return screen_hal_ts_available();
}

static inline int
joypad_freezed ()
{
    if (d_time() > joypad_timestamp)
        return 0;
    return 1;
}

static inline void
joypad_freeze (uint8_t flags)
{
    if (flags & PAD_FREQ_LOW) {
        joypad_timestamp = d_time() + JOY_FREEZE_TIME;
    }
}

static inline void
post_key_up (uint16_t key)
{
    i_event_t event = {key, keyup};
    input_post_key(NULL, &event);
}

static inline void
post_key_down (uint16_t key)
{
    i_event_t event = {key, keydown};
    input_post_key(NULL, &event);
}

static inline void
post_event (
        i_event_t *event,
        kbdmap_t *kbd_key,
        int8_t action)
{
    if (action) {
        post_key_down(kbd_key->key);
    }

    ts_freeze_ticks = TSENS_SLEEP_TIME;
    joypad_freeze(kbd_key->flags);
}

void input_proc_keys (i_event_t *evts)
{
    i_event_t event = {0, keyup, 0, 0};
    ts_status_t ts_status = {TOUCH_IDLE, 0, 0};

    if (bsp_input_errno) {
        return;
    }

    if (input_is_touch_avail()) {
        if (!ts_freeze_ticks) {
            /*Skip sensor processing while gamepad active*/
            input_hal_read_ts(&ts_status);
        }
    }
    if (ts_status.status)
    {
        event.state = (ts_status.status == TOUCH_PRESSED) ? keydown : keyup;
        event.sym = ts_get_key(ts_status.x, ts_status.y);
        event.x = ts_status.x;
        event.y = ts_status.y;
        input_post_key(evts, &event);
    } else {
        int8_t joy_pads[JOY_STD_MAX];
        int keys_cnt;

        d_memset(joy_pads, -1, sizeof(joy_pads));
        keys_cnt = joypad_read(joy_pads);
        if (keys_cnt <= 0 || joypad_freezed()) {
            return;
        }
        for (int i = 0; i < keys_cnt; i++) {

            if (joy_pads[i] >= 0) {
                post_event(&event, &input_kbdmap[i], joy_pads[i]);
            } else {
                post_key_up(input_kbdmap[i].key);
            }
            if (ts_freeze_ticks) {
                ts_freeze_ticks--;
            }
        }
    }
}

void input_bsp_deinit (void)
{
    if (bsp_input_errno) {
        return;
    }
    user_handler = NULL;
    input_hal_deinit();
    joypad_bsp_deinit();
}

int input_bsp_init (void)
{
    bsp_input_errno = input_hal_init();

    if (bsp_input_errno < 0) {
        dprintf("%s(): Faied, err %d\n", __func__, bsp_input_errno);
        return bsp_input_errno;
    }
    joypad_bsp_init();
    dprintf("%s():  OK\n", __func__);
    return 0;
}

void input_soft_init (input_evt_handler_t handler, const kbdmap_t *kbdmap)
{
    if (bsp_input_errno) {
        return;
    }
    d_memcpy(&input_kbdmap[0], &kbdmap[0], sizeof(input_kbdmap));
    user_handler = handler;
    ts_attach_keys(ts_zones_keymap, kbdmap);
}

void input_bind_extra (int type, int sym)
{
    if (bsp_input_errno) {
        return;
    }
    if (type >= K_EX_MAX) {
        dprintf("%s() : type >= JOY_MAX\n", __func__);
    }
}

void input_tickle (void)
{
    if (!bsp_input_errno) {
        joypad_tickle();
    }
}

