#ifndef __BSP_SYS_H__
#define __BSP_SYS_H__

#include <bsp_api.h>
#include <heap.h>

typedef struct {
    bsp_heap_api_t heap;
} bsp_user_api_t;

typedef struct bsp_sytem_api_s {
    bspdev_t dev;
    int (*hal_init) (void);
    void *(*user_alloc) (int);
    void (*user_free) (void *);
    int  (*user_api_attach) (bsp_user_api_t *);
    void (*fatal) (char *, ...);
    void (*prof_enter) (const char *, int);
    void (*prof_exit) (const char *, int);
    void (*prof_reset) (void);
    void (*prof_init) (void);
    void (*prof_deinit) (void);
} bsp_sytem_api_t;

#define BSP_SYS_API(func) ((bsp_sytem_api_t *)(g_bspapi->sys))->func

#if BSP_INDIR_API

#define bsp_drv_init      BSP_SYS_API(dev.init)

#define sys_init          BSP_SYS_API(dev.init)
#define dev_hal_init        BSP_SYS_API(hal_init)
#define sys_deinit        BSP_SYS_API(dev.deinit)
#define sys_conf          BSP_SYS_API(dev.conf)
#define sys_info          BSP_SYS_API(dev.info)
#define sys_priv          BSP_SYS_API(dev.priv)

#define sys_user_alloc   BSP_SYS_API(user_alloc)
#define sys_user_free    BSP_SYS_API(user_free)
#define sys_user_attach  BSP_SYS_API(user_api_attach)

#define fatal_error         BSP_SYS_API(fatal)
#define _profiler_enter     BSP_SYS_API(prof_enter)
#define _profiler_exit      BSP_SYS_API(prof_exit)
#define profiler_reset      BSP_SYS_API(prof_reset)
#define profiler_init       BSP_SYS_API(prof_init)
#define profiler_deinit     BSP_SYS_API(prof_deinit)

#else /*BSP_INDIR_API*/

void fatal_error (char *message, ...);
void _profiler_enter (const char *func, int line);
void _profiler_exit (const char *func, int line);
void profiler_reset (void);
void profiler_init (void);
void profiler_deinit (void);

void *sys_user_alloc (int size);
void sys_user_free (void *p);
int  sys_user_attach (bsp_user_api_t *api);

int dev_hal_init (void);
int bsp_drv_init (void);
void dev_deinit (void);

#endif /*BSP_INDIR_API*/

void bsp_tickle (void);

#define profiler_enter() _profiler_enter(__func__, __LINE__)
#define profiler_exit() _profiler_exit(__func__, __LINE__)

typedef enum {
    EXEC_DRIVER,
    EXEC_APPLICATION,
    EXEC_MODULE,
    EXEC_ISR,
    EXEC_MAX,
} exec_region_t;

typedef enum {
    EXEC_ROM, /*on-board FLASH*/
    EXEC_SDRAM,
    EXEC_SRAM,
    EXEC_INVAL,
} exec_mem_type_t;

exec_mem_type_t __bhal_get_memory_type (arch_word_t addr);

#if defined(BOOT)
extern exec_region_t g_exec_region;

#define EXEC_REGION_APP() (g_exec_region == EXEC_APPLICATION)
#define EXEC_REGION_MODULE() (g_exec_region == EXEC_MODULE)
#define EXEC_REGION_DRIVER() (g_exec_region == EXEC_DRIVER)
#endif /*BOOT*/

#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR "/"
#endif

int bsp_argv_check (const char *name);
const char *bsp_argv_get (int num);

void bsp_argc_argv_set (const char *arg);
int bsp_argc_argv_check (const char *arg);

#endif /*__BSP_SYS_H__*/
