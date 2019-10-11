#ifndef _INPUT_MAIN_H
#define _INPUT_MAIN_H

#include <stdint.h>
#include <bsp_api.h>
#include <touch.h>

#define GAMEPAD_USE_FLYLOOK 1

enum {
    TS_IDLE,
    TS_PRESSED,
    TS_CLICK,
    TS_RELEASED,
};

enum {
    TS_PRESS_OFF,
    TS_PRESS_ON,
};

#define PAD_FREQ_LOW        0x1
#define PAD_FUNCTION        0x2
#define PAD_SET_FLYLOOK     0x4
#define PAD_LOOK_CONTROL    0x8
#define PAD_LOOK_UP         0x10
#define PAD_LOOK_DOWN       0x20
#define PAD_LOOK_CENTRE     0x40

typedef struct kbdmap_s {
    int16_t key;
    int16_t flags;
} kbdmap_t;

typedef enum {
    keyup,
    keydown,
} i_key_state_t;

typedef struct {
    int sym;
    i_key_state_t state;
    int16_t x, y;
} i_event_t;

enum {
    JOY_KEY_OFFSET  = 0,
    JOY_K1          = JOY_KEY_OFFSET,
    JOY_K2          = JOY_K1 + 1,
    JOY_K3          = JOY_K2 + 1,
    JOY_K4          = JOY_K3 + 1,
    JOY_K5          = JOY_K4 + 1,
    JOY_K6          = JOY_K5 + 1,
    JOY_K7          = JOY_K6 + 1,
    JOY_K8          = JOY_K7 + 1,
    JOY_K9          = JOY_K8 + 1,
    JOY_K10         = JOY_K9 + 1,
    JOY_KMAX        = JOY_K10 + 1,

    JOY_UPARROW     = JOY_KMAX,
    JOY_DOWNARROW   = JOY_UPARROW + 1,
    JOY_LEFTARROW   = JOY_DOWNARROW + 1,
    JOY_RIGHTARROW  = JOY_LEFTARROW + 1,
    JOY_STD_MAX     = JOY_RIGHTARROW + 1,
    JOY_ANALOG      = JOY_STD_MAX,
    JOY_MAX         = JOY_ANALOG + 1,
}; /*regular keys*/

enum {
    K_EX_LOOKUP,
    K_EX_LOOKDOWN,
    K_EX_LOOKCENTER,
    K_EX_MAX,
}; /*extra keys (activsted by hold 'ctrl' key)*/

typedef i_event_t *(*input_evt_handler_t) (i_event_t *, i_event_t *);


typedef struct bsp_input_api_s {
    bspdev_t dev;
    void (*soft_init) (input_evt_handler_t, const kbdmap_t *);
    void (*bind_extra) (int, int);
    void (*tickle) (void);
    void (*proc_keys) (i_event_t *);
    void *(*post_key) (void *, i_event_t *event);
    int (*touch_present) (void);
} bsp_input_api_t;

#define BSP_IN_API(func) ((bsp_input_api_t *)(g_bspapi->in))->func

#if BSP_INDIR_API

#define input_bsp_init        BSP_IN_API(dev.init)
#define input_bsp_deinit      BSP_IN_API(dev.deinit)
#define input_bsp_conf        BSP_IN_API(dev.conf)
#define input_bsp_info        BSP_IN_API(dev.info)
#define input_bsp_priv        BSP_IN_API(dev.priv)

#define input_soft_init       BSP_IN_API(soft_init)
#define input_bind_extra      BSP_IN_API(bind_extra)
#define input_tickle          BSP_IN_API(tickle)
#define input_proc_keys       BSP_IN_API(proc_keys)
#define input_post_key        BSP_IN_API(post_key)
#define input_is_touch_avail BSP_IN_API(touch_present)

#else
int input_bsp_init (void);
void input_bsp_deinit (void);
void input_soft_init (input_evt_handler_t, const kbdmap_t *kbdmap);
void input_bind_extra (int type, int sym);
void input_tickle (void);
void input_proc_keys (i_event_t *evts);
d_bool input_is_touch_avail (void);
#endif

#endif /*_INPUT_MAIN_H*/
