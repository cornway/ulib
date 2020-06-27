/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <nvic.h>
#include <serial.h>
#include <bsp_cmd.h>
#include <gfx2d_mem.h>
#include <lcd_main.h>
#include <audio_main.h>
#include <input_main.h>
#include <debug.h>
#include <dev_io.h>
#include "../../common/int/mpu.h"
#include <heap.h>
#include <bsp_sys.h>
#include <gpio.h>

#ifdef __MICROLIB
#error "I don't want to use microlib"
/*Microlib will reduce code size at the cost of performance,
    also due to HEAP replacement
*/
#endif

extern void boot_gui_preinit (void);
extern void VID_PreConfig (void);
extern int mainloop (int argc, const char *argv[]);
extern void dev_hal_tickle (void);

int g_dev_debug_level = DBG_ERR;

#include "../../ulib/misc/inc/swtim.h"

static void *led_swtim;
void dev_led_tickle (void *unused)
{
    static int led_status = 0;

    led_status = 1 - led_status;
    if (led_status) {
        status_led_on();
    } else {
        status_led_off();
    }
    swtim_timeout(led_swtim, 500);
}

#if (_USE_LFN == 3)
#error "ff_malloc, ff_free must be redefined to Sys_HeapAlloc"
#endif

void dumpstack (void)
{
}

void UserExceptionH (void *stack)
{
    uint32_t *frame = (uint32_t *)stack;
    dprintf("Stack <%p> : 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
            frame, frame[0], frame[1], frame[2], frame[3], frame[4], frame[5]);
}

void fatal_error (char *fmt, ...)
{
    va_list argptr;

    va_start (argptr, fmt);
    dvprintf(fmt, argptr);
    va_end (argptr);

    arch_rise(NULL);
    serial_safe_mode(1);
    serial_flush();
    arch_soft_reset();
    for(;;) {}
}

/* Private function prototypes -----------------------------------------------*/

static int con_echo (int argc, const char **argv)
{
    int i;
    char buf[128];

    dprintf("@> ");

    for (i = 0; i < argc; i++) {
        snprintf(buf, sizeof(buf), "%s", argv[i]);
        dprintf(" %s", buf);
    }
    dprintf("\n");

    return argc; /*let it be processed by others*/
}

void (*dev_deinit_callback) (void) = NULL;

int bsp_drv_init (void)
{
    dev_io_init();

    bsp_stdin_register_if(con_echo);
    heap_stat();
    mpu_init();
    cmd_init();
    vid_init();

    audio_init();
    input_bsp_init();
    profiler_init();
    cmd_register_i32(&g_dev_debug_level, "dbglvl");
    return 0;
}


void dev_deinit (void)
{
    if (dev_deinit_callback) {
        dev_deinit_callback();
        dev_deinit_callback = NULL;
    }

    dprintf("%s() :\n", __func__);
    bsp_stdin_unreg_if(con_echo);

    cmd_deinit();
    dev_io_deinit();
    audio_deinit();
    profiler_deinit();
    input_bsp_deinit();
    vid_deinit();
    heap_dump();
    hal_tty_destroy_any();
}

int bsp_drv_main (void)
{
    const char **argv = NULL;
    int argc = 0;

    dev_hal_preinit();
    heap_init();
    dev_hal_init();
    bsp_drv_init();
    VID_PreConfig();
    swtim_core_init();
    led_swtim = swtim_init(dev_led_tickle, NULL, 0);
    swtim_timeout(led_swtim, 500);
    g_bspapi = bsp_api_attach();
    mainloop(argc, argv);

    return 0;
}

void *_sbrk (int amount)
{
    return NULL;
}

int _write(int handle, char *buf, int count)
{
    dprintf("%s() : [%d] %s\n", __func__, handle, buf);
    return -1;
}

int _close (int fd)
{
    return -1;
}

long _lseek(int fd, long offset, int origin)
{
    return -1;
} 

int _read(int const fd, void * const buffer, unsigned const buffer_size)
{
    return -1;
}

void __c_hard_fault (arch_word_t p0, arch_word_t p1)
{
    while (1) {};
}

void SysTick_Handler (void)
{
    dev_hal_tickle();
    swtim_tick();
}


