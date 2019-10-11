#include <string.h>
#include <stdarg.h>
#include "int/bsp_mod_int.h"
#include "int/term_int.h"
#include <bsp_cmd.h>
#include <bsp_api.h>
#include <audio_main.h>
#include <misc_utils.h>
#include <input_main.h>
#include <dev_io.h>
#include <lcd_main.h>
#include <heap.h>
#include <bsp_sys.h>
#include <gui.h>
#include <debug.h>

bspapi_t *g_bspapi;

typedef struct {
    uint32_t size;
    uint32_t data[1];
} tlv_t;

typedef struct {
    uint32_t size;
    bspapi_t api;
    struct bsp_io_api_s     io;
    struct bsp_video_api_s  vid;
    struct bsp_sfx_api_s    sfx;
    struct bsp_cd_api_s     cd;
    struct bsp_sytem_api_s  sys;
    struct bsp_debug_api_s  dbg;
    struct bsp_input_api_s  in;
    struct app_gui_api_s    gui;
    struct bsp_cmd_api_s    cmd;
} bsp_api_int_t;

#if defined(BSP_DRIVER)

typedef struct {
    bsp_user_api_t api;
    d_bool attached;
} bsp_user_int_api_t;

static bsp_user_int_api_t user_api = {0};

#endif /*BSP_DRIVER*/

#if !BSP_INDIR_API

int dev_init_stub (void)
{
    dprintf("%s()\n", __func__);
    return -1;
}

void dev_deinit_stub (void)
{
    dprintf("%s()\n", __func__);
}

int dev_conf_stub (const char *arg)
{
    dprintf("%s() : \'%s\'\n", __func__, arg);
    return -1;
}

const char *dev_info_stub (void)
{
    dprintf("%s()\n", __func__);
    return "stub\n";
}

int dev_priv_stub (int c, void *v)
{
    dprintf("%s()\n", __func__);
    return 0;
}

#define API_SETUP(api, module) \
(api)->api.module = &(api)->module;

bspapi_t *bsp_api_attach (void)
{
#if defined(BOOT)
    arch_word_t *ptr, size;
    bsp_api_int_t *api;
    tlv_t *tlv;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    memset(ptr, 0, size);

    api = (bsp_api_int_t *)ptr;
    tlv = (tlv_t *)ptr;
    API_SETUP(api, io);
    API_SETUP(api, vid);
    API_SETUP(api, sfx);
    API_SETUP(api, cd);
    API_SETUP(api, sys);
    API_SETUP(api, dbg);
    API_SETUP(api, in);
    API_SETUP(api, gui);
    API_SETUP(api, cmd);

    api->size = sizeof(*api);
    assert(api->size == tlv->size);
    g_bspapi = &api->api;

    BSP_IO_API(dev.init)     = dev_io_init;
    BSP_IO_API(dev.deinit)   = dev_io_deinit;
    BSP_IO_API(dev.conf)    = dev_conf_stub;
    BSP_IO_API(dev.info)    = dev_info_stub;
    BSP_IO_API(dev.priv)    = dev_priv_stub;
    BSP_IO_API(open)        = d_open;
    BSP_IO_API(size)        = d_size;
    BSP_IO_API(tell)        = d_tell;
    BSP_IO_API(close)       = d_close;
    BSP_IO_API(unlink)      = d_unlink;
    BSP_IO_API(seek)        = d_seek;
    BSP_IO_API(eof)         = d_eof;
    BSP_IO_API(read)        = d_read;
    BSP_IO_API(gets)        = d_gets;
    BSP_IO_API(getc)        = d_getc;
    BSP_IO_API(write)       = d_write;
    BSP_IO_API(printf)      = d_printf;
    BSP_IO_API(mkdir)       = d_mkdir;
    BSP_IO_API(opendir)     = d_opendir;
    BSP_IO_API(closedir)    = d_closedir;
    BSP_IO_API(readdir)     = d_readdir;
    BSP_IO_API(time)        = d_time;
    BSP_IO_API(dirlist)     = d_dirlist;

    BSP_CMD_API(var_reg)    = cmd_register_var;
    BSP_CMD_API(var_int32)  = cmd_register_i32;
    BSP_CMD_API(var_float)  = cmd_register_float;
    BSP_CMD_API(var_str)    = cmd_register_str;
    BSP_CMD_API(var_func)   = cmd_register_func;
    BSP_CMD_API(exec)       = cmd_execute;
    BSP_CMD_API(tickle)     = cmd_tickle;

    BSP_VID_API(dev.init)   = vid_init;
    BSP_VID_API(dev.deinit) = vid_deinit;
    BSP_VID_API(dev.conf)   = dev_conf_stub;
    BSP_VID_API(dev.info)   = dev_info_stub;
    BSP_VID_API(dev.priv)   = dev_priv_stub;
    BSP_VID_API(get_wh)     = vid_wh;
    BSP_VID_API(mem_avail)  = vid_mem_avail;
    BSP_VID_API(win_cfg)    = vid_config;
    BSP_VID_API(set_clut)   = vid_set_clut;
    BSP_VID_API(update)     = vid_update;
    BSP_VID_API(direct)     = vid_direct;
    BSP_VID_API(vsync)      = vid_vsync;
    BSP_VID_API(input_align) = vid_ptr_align;

    BSP_SFX_API(dev.init)   = audio_init;
    BSP_SFX_API(dev.deinit) = audio_deinit;
    BSP_SFX_API(dev.conf)   = audio_conf;
    BSP_SFX_API(dev.info)   = dev_info_stub;
    BSP_SFX_API(dev.priv)   = dev_priv_stub;
    BSP_SFX_API(mixer_ext)  = audio_mixer_ext;
    BSP_SFX_API(play)       = audio_play_channel;
    BSP_SFX_API(stop)       = audio_stop_channel;
    BSP_SFX_API(pause)      = audio_pause;
    BSP_SFX_API(stop_all)   = audio_stop_all;
    BSP_SFX_API(is_play)    = audio_is_playing;
    BSP_SFX_API(check)      = audio_chk_priority;
    BSP_SFX_API(set_pan)    = audio_set_pan;
    BSP_SFX_API(sample_volume) = audio_sample_vol;
    BSP_SFX_API(tickle)     = audio_update;
    BSP_SFX_API(irq_save)   = audio_irq_save;
    BSP_SFX_API(irq_restore) = audio_irq_restore;

    BSP_SFX_API(wave_open)  = audio_wave_open;
    BSP_SFX_API(wave_size)  = audio_wave_size;
    BSP_SFX_API(wave_cache) = audio_wave_cache;
    BSP_SFX_API(wave_close) = audio_wave_close;

    BSP_CD_API(dev.init)    = dev_init_stub;
    BSP_CD_API(dev.deinit)  = dev_deinit_stub;
    BSP_CD_API(dev.conf)    = dev_conf_stub;
    BSP_CD_API(dev.info)    = dev_info_stub;
    BSP_CD_API(dev.priv)    = dev_priv_stub;
    BSP_CD_API(play)        = cd_play_name;
    BSP_CD_API(pause)       = cd_pause;
    BSP_CD_API(resume)      = cd_resume;
    BSP_CD_API(stop)        = cd_stop;
    BSP_CD_API(volume)      = cd_volume;
    BSP_CD_API(getvol)      = cd_getvol;
    BSP_CD_API(playing)     = cd_playing;

    BSP_SYS_API(dev.init)   = bsp_drv_init;
    BSP_SYS_API(dev.deinit) = dev_deinit_stub;
    BSP_SYS_API(dev.conf)   = dev_conf_stub;
    BSP_SYS_API(dev.info)   = dev_info_stub;
    BSP_SYS_API(dev.priv)   = dev_priv_stub;
    BSP_SYS_API(hal_init)   = dev_hal_init;
    BSP_SYS_API(fatal)      = fatal_error;
    BSP_SYS_API(prof_enter) = _profiler_enter;
    BSP_SYS_API(prof_exit)  = _profiler_exit;
    BSP_SYS_API(prof_reset) = profiler_reset;
    BSP_SYS_API(prof_init)  = profiler_init;
    BSP_SYS_API(prof_deinit)  = profiler_deinit;

    BSP_SYS_API(user_alloc) = sys_user_alloc;
    BSP_SYS_API(user_free)  = sys_user_free;
    BSP_SYS_API(user_api_attach) = sys_user_attach;

    BSP_DBG_API(dev.init)   = serial_init;
    BSP_DBG_API(dev.deinit) = dev_deinit_stub;
    BSP_DBG_API(dev.conf)   = dev_conf_stub;
    BSP_DBG_API(dev.info)   = dev_info_stub;
    BSP_DBG_API(dev.priv)   = dev_priv_stub;
    BSP_DBG_API(putc)       = serial_putc;
    BSP_DBG_API(getc)       = serial_getc;
    BSP_DBG_API(send)       = bsp_serial_send;
    BSP_DBG_API(flush)      = serial_flush;
    BSP_DBG_API(reg_clbk)   = bsp_stdin_register_if;
    BSP_DBG_API(unreg_clbk) = bsp_stdin_unreg_if;
    BSP_DBG_API(tickle)     = serial_tickle;
    BSP_DBG_API(dprintf)    = dprintf;

    BSP_IN_API(dev.init)    = input_bsp_init;
    BSP_IN_API(dev.deinit)  = input_bsp_deinit;
    BSP_IN_API(dev.conf)    = dev_conf_stub;
    BSP_IN_API(dev.info)    = dev_info_stub;
    BSP_IN_API(dev.priv)    = dev_priv_stub;
    BSP_IN_API(soft_init)   = input_soft_init;
    BSP_IN_API(bind_extra)  = input_bind_extra;
    BSP_IN_API(tickle)      = input_tickle;
    BSP_IN_API(proc_keys)   = input_proc_keys;
    BSP_IN_API(post_key)    = NULL;
    BSP_IN_API(touch_present) = input_is_touch_avail;

    BSP_MOD_API(dev.init)   = dev_init_stub;
    BSP_MOD_API(dev.deinit) = dev_deinit_stub;
    BSP_MOD_API(dev.conf)   = dev_conf_stub;
    BSP_MOD_API(dev.info)   = dev_info_stub;
    BSP_MOD_API(dev.priv)   = dev_priv_stub;
    BSP_MOD_API(insert)    =  bspmod_insert;
    BSP_MOD_API(remove)     = bspmod_remove;
    BSP_MOD_API(probe)      = bspmod_probe;
    BSP_MOD_API(register_api) = bspmod_register_api;
    BSP_MOD_API(get_api)    = bspmod_get_api;

    return &api->api;
#else
    return NULL;
#endif
}

#else /*BSP_INDIR_API*/

bspapi_t *bsp_api_attach (void)

{
    arch_word_t *ptr, size;
    bsp_api_int_t *api;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    api = (bsp_api_int_t *)ptr;
    assert(api->size == sizeof(*api));
    return &api->api;
}

#endif /*BSP_INDIR_API*/

void bsp_tickle (void)
{
    audio_update();
    input_tickle();
    serial_tickle();
    profiler_reset();
    cmd_tickle();
}

#define MAX_ARGC 16

static inline tlv_t *__get_tlv (void *_tlv)
{
    tlv_t *tlv = (tlv_t *)_tlv;
    return tlv;
}

static inline tlv_t *__get_next_tlv (void *_tlv)
{
    tlv_t *tlv = (tlv_t *)_tlv;
    tlv = (tlv_t *)((uint32_t)tlv + tlv->size);
    return tlv;
}

static inline tlv_t *__set_next_tlv (void *_tlv, uint32_t size)
{
    tlv_t *tlv = (tlv_t *)_tlv;
    tlv->size = size + sizeof(tlv->size);
    tlv = (tlv_t *)((uint32_t)tlv + tlv->size);
    return tlv;
}

static const char **__argv = NULL;
static int __argc = 0;

const char **bsp_argc_argv_get (int *argc)
{
    arch_word_t *ptr, size;
    tlv_t *tlv;

    arch_get_shared(&ptr, &size);

    tlv = __get_next_tlv(ptr);
    tlv = __get_next_tlv(tlv);
    *argc = *(int *)&tlv->data[0];
    __argv = (const char **)&tlv->data[1];
    __argc = *argc;
    return __argv;
}

int bsp_argv_check (const char *name)
{
    int i;
    for (i = 0; i < __argc; i++) {
        if (strcmp(__argv[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

const char *bsp_argv_get (int num)
{
    if (num >= __argc) {
        return "";
    }
    return __argv[num];
}

#if defined(BSP_DRIVER)

int bsp_argc_argv_check (const char *arg)
{
    const char *argvbuf[MAX_ARGC], **argv = &argvbuf[0];
    const char **__argv;
    char charbuf[256];
    int __argc, argc, i, fails = 0, size;

    size = snprintf(charbuf, sizeof(charbuf), "%s ", arg);
    if (size >= sizeof(charbuf) - 1) {
        dprintf("too large cmd line\n");
        return -1;
    } 
    argc = d_wstrtok(argv, MAX_ARGC, charbuf);

    __argv = bsp_argc_argv_get(&__argc);
    if (argc != __argc) {
        dprintf("%s() : fail - argc != __argc\n", __func__);
        return -1;
    }
    for (i = 0; i < argc; i++) {
        if (strcmp(__argv[i], argv[i])) {
            fails++;
            dprintf("mismatch : \'%s\' != \'%s\'\n", __argv[i], argv[i]);
        }
    }
    if (fails) {
        dprintf("%s() : failed : %u fails\n", __func__, fails);
        return -fails;
    }
    return 0;
}

void bsp_argc_argv_set (const char *arg)
{
    arch_word_t *ptr, maxsize;
    int size, tmp;
    const char **_argv;
    char *charptr, *tempptr;
    tlv_t *tlv;

    arch_get_shared(&ptr, &maxsize);

    tlv = __get_tlv(ptr);
    maxsize = maxsize - tlv->size;
    tlv = __get_next_tlv(ptr);

    tempptr = (char *)&tlv->data[0];
    size = maxsize;
    charptr = tempptr;

    size = snprintf(charptr, size, "%s ", arg);
    charptr += tmp;
    maxsize -= tmp;

    tlv = __set_next_tlv(tlv, size);

    dprintf("user args : [%s], size : <%u>\n", (char *)charptr, tlv->size);

    size = MAX_ARGC * sizeof(char *) + sizeof(uint32_t) + sizeof(uint32_t);
    maxsize -= size;
    assert(maxsize > 0);

    _argv = (const char **)(&tlv->data[1]);
    tlv->data[0] = d_wstrtok(_argv, MAX_ARGC, tempptr);

    __set_next_tlv(tlv, size);
}

#if defined(BOOT)
void *sys_user_alloc (int size)
{
    if (!user_api.attached) {
        return NULL;
    }
    return user_api.api.heap.malloc(size);
}

void sys_user_free (void *p)
{
    if (!user_api.attached) {
        return;
    }
    user_api.api.heap.free(p);
}

int sys_user_attach (bsp_user_api_t *api)
{
    assert(EXEC_REGION_APP());
    memcpy(&user_api.api, api, sizeof(user_api.api));
    user_api.attached = d_true;
    return 0;
}
#endif /*BOOT*/

#endif /*BSP_DRIVER*/



/*Common for both:*/

int dvprintf (const char *fmt, va_list argptr)
{
    char string[1024];
    vsnprintf(string, sizeof(string), fmt, argptr);
    return dprintf(string);
}

int d_rlimit_wrap (uint32_t *tsf, uint32_t period)
{
    if (*tsf && *tsf > d_time()) {
        return 0;
    }
    *tsf = d_time() + period;
    return 1;
}

void d_sleep (uint32_t ms)
{
    ms = d_time() + ms;
    while (ms > d_time()) {}
}

