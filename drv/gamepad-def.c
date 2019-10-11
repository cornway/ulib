
#include "../int/input_int.h"

#include <misc_utils.h>
#include <input_main.h>
#include <heap.h>

#define LR_GPOS         0
#define LR_MASK         (0xffUL << LR_GPOS)
#define LR_IDLE         (0x7fUL << LR_GPOS)

#define UP_DOWN_GPOS    8
#define UP_DOWN_MASK    (0xffUL << UP_DOWN_GPOS)
#define UP_DOWN_IDLE    (0x7fUL << UP_DOWN_GPOS)

#define ANALOG_BK_GP      16
#define ANALOG_BK_MASK    (0xffUL << ANALOG_BK_GP)
#define ANALOG_BK_IDLE    (0x8CUL << ANALOG_BK_GP)

#define KEYS_GPOS       20
#define KEYS_MASK       (0xfffUL << KEYS_GPOS)
#define KEYS_IDLE       (0x0UL << KEYS_GPOS)

enum {
    __LEFTRIGHT = 0,
    __UPDOWN = 1,
    __ANALOG_BK = 2,
    __ANALOG_0 = 3,
    __ANALOG_1 = 4,
    __KSTART = 5,
    __KEND = 7,
    __MODE = 7,
    __MODE_ANALOG = 0x40,
    __MODE_STD = 0xc0,

    __KSTART_SHIFT = 4,
};

#define ACT_IDLE        ((KEYS_IDLE | SHIFT_IDLE | CTL_IDLE) << 32)

#define JOY_IDLE_MASK   (UP_DOWN_IDLE | LR_IDLE | ACT_IDLE)
/*0x[C0 - analog off, 4 - on]   000  f  7f7f--[8C - analog off val]--7f7f*/
#define JOY_IDLE(in) ((in & JOY_IDLE_MASK) == JOY_IDLE_MASK)

#define M_UP     0
#define M_DOWN   0xff
#define M_LEFT   0
#define M_RIGHT  0xff
#define M_IDLE   0x7f

#define M_K1     0x1
#define M_K2     0x2
#define M_K3     0x4
#define M_K4     0x8
#define M_BL     1
#define M_TL     2
#define M_BR     4
#define M_TR     8
#define M_START  1
#define M_SELECT 2

typedef union {
    uint8_t data[8];
    uint64_t dword;
} usb_data8_t;

static inline int
set_key_state (int8_t *keypads, int pad_idx, int state)
{
    if (state) {
        keypads[pad_idx] = 1;
    } else if (keypads[pad_idx] >= 0) {
        keypads[pad_idx]--;
    }
    if (keypads[pad_idx] > 0) {
        return 1;
    }
    return 0;
}

static int
_joypad_read_std (gamepad_drv_t *drv, int8_t *pads, void *_data)
{
    int i = 0, bit;
    uint32_t keys;
    uint8_t temp;
    usb_data8_t *data = (usb_data8_t *)_data;
    int8_t *keypads = (int8_t *)drv->user;

    keys = readLong(&data->data[__KSTART]) >> __KSTART_SHIFT;

    for (bit = 0, i = JOY_KEY_OFFSET; i < JOY_KMAX; i++, bit++) {
        set_key_state(keypads, i, (keys >> bit) & 0x1);
    }

    temp = data->data[__UPDOWN];
    set_key_state(keypads, JOY_UPARROW, temp == M_UP);
    set_key_state(keypads, JOY_DOWNARROW, temp == M_DOWN);

    temp = data->data[__LEFTRIGHT];
    set_key_state(keypads, JOY_LEFTARROW, temp == M_LEFT);
    set_key_state(keypads, JOY_RIGHTARROW, temp == M_RIGHT);
    pads[JOY_ANALOG] = data->data[__ANALOG_BK];
    d_memcpy(pads, keypads, JOY_MAX);
    return JOY_STD_MAX;
}

void *
__joypad_init_std (gamepad_drv_t *drv, uint32_t bufsize)
{
    void *p;
    int8_t *keypads;
    p = heap_malloc(bufsize + (sizeof(int8_t) * JOY_MAX));
    if (!p) {
        return NULL;
    }
    drv->user = p;
    keypads = (int8_t *)p;
    p = (void *)((arch_word_t)p + (JOY_MAX * sizeof(int8_t)));
    d_memzero(p, bufsize);
    for (int i = 0; i < JOY_STD_MAX; i++) {
        keypads[i] = -1;
    }
    return p;
}

static void *
_joypad_init_std (gamepad_drv_t *drv, uint32_t *bufsize)
{
    *bufsize = sizeof(usb_data8_t);
    return __joypad_init_std(drv, sizeof(usb_data8_t));
}

static void
_joypad_deinit_std (gamepad_drv_t *drv)
{
    heap_free(drv->user);
    drv->user = NULL;
}

void joypad_attach_def (gamepad_drv_t *drv)
{
    drv->init = _joypad_init_std;
    drv->read = _joypad_read_std;
    drv->deinit = _joypad_deinit_std;
    drv->user = NULL;
}

