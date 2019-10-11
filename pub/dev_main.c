/* Includes ------------------------------------------------------------------*/
#include <bsp_api.h>
#include <stdarg.h>
#include <main.h>
#include <misc_utils.h>
#include <input_main.h>
#include <debug.h>
#include <dev_io.h>
#include <debug.h>
#include <nvic.h>
#include <mpu.h>
#include <heap.h>
#include <bsp_sys.h>

#if !defined(BSP_DRIVER)

extern void VID_PreConfig (void);
extern int mainloop (int argc, const char *argv[]);
extern char **bsp_argc_argv_get (int *argc);

static bsp_user_api_t user_api =
{
    .heap =
    {
        .malloc = heap_malloc,
        .free = heap_free
    },
};

int app_main (void)
{
    char **argv;
    int argc;

    g_bspapi = bsp_api_attach();
    sys_user_attach(&user_api);
    argv = bsp_argc_argv_get(&argc);

    dev_hal_init();
    heap_init();

    bsp_drv_init();
    VID_PreConfig();

    return mainloop(argc, argv);
}

#endif /*!defined(BSP_DRIVER)*/

