/* Includes ------------------------------------------------------------------*/

#include <bsp_api.h>
#include <stdarg.h>
#include <misc_utils.h>
#include "../../common/int/bsp_cmd.h"
#include <lcd_main.h>
#include <audio_main.h>
#include <input_main.h>
#include <debug.h>
#include <dev_io.h>
#include <nvic.h>
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

void fatal_error (char *message, ...)
{
    va_list argptr;

    va_start (argptr, message);
    dvprintf (message, argptr);
    va_end (argptr);

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
extern int32_t g_serial_rxtx_eol_sens;
    dev_io_init();

    bsp_stdin_register_if(con_echo);
    cmd_init();

    audio_init();
    input_bsp_init();
    profiler_init();
    vid_init();
    cmd_register_i32(&g_dev_debug_level, "dbglvl");
    cmd_register_i32(&g_serial_rxtx_eol_sens, "set_rxeof");
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
    char **argv;
    int argc;

    g_bspapi = bsp_api_attach();
    dev_hal_init();
    mpu_init();
    heap_init();

    bsp_drv_init();
    mpu_init();

    VID_PreConfig();
    mainloop(argc, argv);

    return 0;
}
