
#if defined(BSP_DRIVER)

#include <stdlib.h>
#include "int/input_int.h"
#include <misc_utils.h>
#include <input_main.h>
#include <debug.h>
#include "stm32f769i_discovery_ts.h"



#define TS_DEF_CD_COUNT 0
#define TSENS_SLEEP_TIME 250
#define JOY_FREEZE_TIME 150/*ms*/

/*
extern bool menuactive;
extern bool automapactive;
extern int followplayer;
FIXME :
*/
extern int joypad_read (int8_t *pads);

static kbdmap_t input_kbdmap[JOY_STD_MAX];

static input_evt_handler_t user_handler = NULL;

#define input_post_key(a, b) {              \
    user_handler ? user_handler(a, b) : NULL; \
}

static uint8_t ts_states_map[4][2];
static uint8_t ts_prev_state = TS_IDLE;
static uint8_t ts_state_cooldown_cnt = 0;
static uint8_t ts_freeze_ticks = 0;

static uint32_t joypad_timestamp = 0;

static int ts_screen_zones[2][3];
static uint8_t ts_zones_keymap[3][4];

static void input_fatal (char *msg)
{
    extern void fatal_error (char *message, ...);
    fatal_error(msg);
}

void ts_init_states (void)
{
    ts_states_map[TS_IDLE][TS_PRESS_ON]         = TS_CLICK;
    ts_states_map[TS_IDLE][TS_PRESS_OFF]        = TS_IDLE;
    ts_states_map[TS_CLICK][TS_PRESS_ON]        = TS_PRESSED;
    ts_states_map[TS_CLICK][TS_PRESS_OFF]       = TS_RELEASED;
    ts_states_map[TS_PRESSED][TS_PRESS_ON]      = TS_PRESSED;
    ts_states_map[TS_PRESSED][TS_PRESS_OFF]     = TS_RELEASED;
    ts_states_map[TS_RELEASED][TS_PRESS_ON]     = TS_IDLE;
    ts_states_map[TS_RELEASED][TS_PRESS_OFF]    = TS_IDLE;
}

static void ts_read_status (ts_status_t *ts_status)
{
    TS_StateTypeDef  TS_State;
    uint8_t state = 0;

    if (BSP_TS_GetState(&TS_State) != TS_OK) {
        input_fatal("BSP_TS_GetState != TS_OK\n");
    }

    ts_status->status = TOUCH_IDLE;
    state = ts_states_map[ts_prev_state][TS_State.touchDetected ? TS_PRESS_ON : TS_PRESS_OFF];
    switch (state) {
        case TS_IDLE:
            if (ts_state_cooldown_cnt) {
                ts_state_cooldown_cnt--;
                return;
            }
            break;
        case TS_PRESSED:
            break;
        case TS_CLICK:
            ts_status->status = TOUCH_PRESSED;
            ts_status->x = TS_State.touchX[0];
            ts_status->y = TS_State.touchY[0];
            break;
        case TS_RELEASED:
            ts_status->status = TOUCH_RELEASED;
            ts_state_cooldown_cnt = TS_DEF_CD_COUNT;
            break;
        default:
            break;
    };
    ts_prev_state = state;
}

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

    xmax = BSP_LCD_GetXSize();
    ymax = BSP_LCD_GetYSize();
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
    if (BSP_LCD_UseHDMI()) {
        return d_false;
    }
    return d_true;
}

static inline int
joypad_freezed ()
{
    if (HAL_GetTick() > joypad_timestamp)
        return 0;
    return 1;
}

static inline void
joypad_freeze (uint8_t flags)
{
    if (flags & PAD_FREQ_LOW) {

        joypad_timestamp = HAL_GetTick() + JOY_FREEZE_TIME;
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

    if (input_is_touch_avail()) {
        if (!ts_freeze_ticks) {
            /*Skip sensor processing while gamepad active*/
            ts_read_status(&ts_status);
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

        memset(joy_pads, -1, sizeof(joy_pads));
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
    dprintf("%s() :\n", __func__);
    user_handler = NULL;
    if (input_is_touch_avail()) {
        BSP_TS_DeInit();
    }
    joypad_bsp_deinit();
}

int input_bsp_init (void)
{
    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
    ts_init_states();

    joypad_bsp_init();
    return 0;
}

void input_soft_init (input_evt_handler_t handler, const kbdmap_t *kbdmap)
{
    memcpy(&input_kbdmap[0], &kbdmap[0], sizeof(input_kbdmap));
    user_handler = handler;
    ts_attach_keys(ts_zones_keymap, kbdmap);
}

void input_bind_extra (int type, int sym)
{
    if (type >= K_EX_MAX) {
        input_fatal("input_bind_extra : type >= JOY_MAX\n");
    }
}

void input_tickle (void)
{
    joypad_tickle();
}

#endif
