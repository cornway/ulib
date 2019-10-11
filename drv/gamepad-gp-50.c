
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
    __LEFTRIGHT = 3,
    __UPDOWN = 4,
    __ANALOG_BK = 2,
    __ANALOG_0 = 3,
    __ANALOG_1 = 4,
    __KSTART = 5,
};

#define ACT_IDLE        ((KEYS_IDLE | SHIFT_IDLE | CTL_IDLE) << 32)

#define JOY_IDLE_MASK   (UP_DOWN_IDLE | LR_IDLE | ACT_IDLE)
/*0x[C0 - analog off, 4 - on]   000  f  7f7f--[8C - analog off val]--7f7f*/
#define JOY_IDLE(in) ((in & JOY_IDLE_MASK) == JOY_IDLE_MASK)

#define M_IDLE   0x7f
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
    uint8_t data[16];
    uint64_t dword[2];
} usb_data16_t;

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

static inline int
_joypad_check_gp_50 (gamepad_drv_t *drv, void *_data, int num)
{
    usb_data16_t *usb_data = (usb_data16_t *)_data;
    uint8_t *data = (uint8_t *)&usb_data->dword[num];
    uint8_t key = data[__KSTART];

    if (key & 0xf0) {
        return 1;
    }
    key = data[__KSTART + 1];
    if (key) {
        return 1;
    }
    key = data[__UPDOWN] & data[__LEFTRIGHT];
    if (key != M_IDLE) {
        return 1;
    }
    return 0;
}

static int
_joypad_read_gp_50_joypad (gamepad_drv_t *drv, void *_data, int8_t *pads, int joypadnum)
{
    uint8_t keytmp;
    uint8_t temp;
    usb_data16_t *usb_data = (usb_data16_t *)_data;
    uint8_t *data = (uint8_t *)&usb_data->dword[joypadnum];
    int8_t *keypads = (int8_t *)drv->user;

    keytmp = data[__KSTART] >> 4;

    set_key_state(keypads, JOY_K1, keytmp & 0x1);
    set_key_state(keypads, JOY_K2, keytmp & 0x2);
    set_key_state(keypads, JOY_K3, keytmp & 0x4);
    set_key_state(keypads, JOY_K4, keytmp & 0x8);

    keytmp = data[__KSTART + 1] >> 4;

    set_key_state(keypads, JOY_K9, keytmp & 0x1);
    set_key_state(keypads, JOY_K10, keytmp & 0x2);

    keytmp = data[__KSTART + 1] & 0xf;

    set_key_state(keypads, JOY_K5, keytmp & 0x4);
    set_key_state(keypads, JOY_K6, keytmp & 0x8);
    set_key_state(keypads, JOY_K7, keytmp & 0x1);
    set_key_state(keypads, JOY_K8, keytmp & 0x2);

    temp = data[__UPDOWN];
    set_key_state(keypads, JOY_UPARROW, temp == M_UP);
    set_key_state(keypads, JOY_DOWNARROW, temp == M_DOWN);

    temp = data[__LEFTRIGHT];
    set_key_state(keypads, JOY_LEFTARROW, temp == M_LEFT);
    set_key_state(keypads, JOY_RIGHTARROW, temp == M_RIGHT);
    pads[JOY_ANALOG] = -1;

    d_memcpy(pads, keypads, JOY_MAX);
    return JOY_STD_MAX;
}

static int
_joypad_read_gp_50 (gamepad_drv_t *drv, int8_t *pads, void *_data)
{
    if (_joypad_check_gp_50(drv, _data, 1)) {
        return _joypad_read_gp_50_joypad(drv, _data, pads, 1);
    }
    return _joypad_read_gp_50_joypad(drv, _data, pads, 0);
}

static void
_joypad_deinit_gp_50 (gamepad_drv_t *drv)
{
    heap_free(drv->user);
    drv->user = NULL;
}

static void *
_joypad_init_gp_50 (gamepad_drv_t *drv, uint32_t *bufsize)
{
extern void *
__joypad_init_std (gamepad_drv_t *drv, uint32_t bufsize);

    *bufsize = sizeof(usb_data16_t);
    return __joypad_init_std(drv, sizeof(usb_data16_t));
}

void joypad_attach_gp_50 (gamepad_drv_t *drv)
{
    drv->init = _joypad_init_gp_50;
    drv->read = _joypad_read_gp_50;
    drv->deinit = _joypad_deinit_gp_50;
    drv->user = NULL;
}


