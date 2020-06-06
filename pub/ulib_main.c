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

#ifdef __MICROLIB
#error "I don't want to use microlib"
/*Microlib will reduce code size at the cost of performance,
    also due to HEAP replacement
*/
#endif

extern void boot_gui_preinit (void);
extern void VID_PreConfig (void);
extern int mainloop (int argc, const char *argv[]);

int g_dev_debug_level = DBG_ERR;

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
    heap_init();
    cmd_init();
    vid_init();
    mpu_init();

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
    uart_if_deinit();
}

int bsp_drv_main (void)
{
    char **argv = NULL;
    int argc = 0;

    dev_hal_init();
    bsp_drv_init();
    VID_PreConfig();
    g_bspapi = bsp_api_attach();
    mainloop(argc, argv);

    return 0;
}
