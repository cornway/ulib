#ifndef _SERIAL_DEBUG_H_
#define _SERIAL_DEBUG_H_

#include <stddef.h>
#include <stdarg.h>
#include <dev_conf.h>
#include <stdint.h>
#include <bsp_api.h>

#define __func__ __FUNCTION__
#define PRINTF __attribute__((format(printf, 1, 2)))
#ifndef PRINTF_ATTR
#define PRINTF_ATTR(a, b) __attribute__((format(printf, a, b)))
#endif

typedef int (*serial_rx_clbk_t) (int, char **);
typedef int (*inout_clbk_t) (const char *, int, char);

extern inout_clbk_t inout_early_clbk;

typedef struct bsp_debug_api_s {
    bspdev_t dev;
    void (*putc) (char);
    char (*getc) (void);
    int (*send) (char *, size_t);
    void (*flush) (void);
    void (*reg_clbk) (int (*) (int , const char **));
    void (*unreg_clbk) (int (*) (int , const char **));
    void (*tickle) (void);
    int (*dprintf) (const char *, ...);
} bsp_debug_api_t;

#ifndef DEBUG_SERIAL
#define DEBUG_SERIAL 1
#endif

#if DEBUG_SERIAL

#define PRINTF_SERIAL  1

#ifndef SERIAL_TSF
#define SERIAL_TSF 1
#endif

#define BSP_DBG_API(func) ((bsp_debug_api_t *)(g_bspapi->dbg))->func

#if BSP_INDIR_API

#define serial_init             BSP_DBG_API(dev.init)
#define serial_deinit          BSP_DBG_API(dev.deinit)
#define serial_conf            BSP_DBG_API(dev.conf)
#define serial_info            BSP_DBG_API(dev.info)
#define serial_priv            BSP_DBG_API(dev.priv)
#define serial_putc             BSP_DBG_API(putc)
#define serial_getc             BSP_DBG_API(getc)
#define bsp_serial_send         BSP_DBG_API(send)
#define serial_flush            BSP_DBG_API(flush)
#define bsp_stdin_register_if   BSP_DBG_API(reg_clbk)
#define bsp_stdin_unreg_if    BSP_DBG_API(unreg_clbk)
#define serial_tickle           BSP_DBG_API(tickle)
#define dprintf                 BSP_DBG_API(dprintf)

#else /*BSP_INDIR_API*/
int serial_init (void);
void serial_deinit (void);
void serial_putc (char c);
char serial_getc (void);
int bsp_serial_send (char *data, size_t cnt);
void serial_flush (void);
void bsp_stdin_register_if (int (*) (int , const char **));
void bsp_stdin_unreg_if (int (*) (int , const  char **));
void serial_tickle (void);
int dprintf (const char *fmt, ...) PRINTF;
int aprint (const char *str, int size);

extern int32_t g_serial_rx_eof;

#endif /*BSP_INDIR_API*/

int dvprintf (const char *fmt, va_list argptr);

#else /*DEBUG_SERIAL*/

static inline void serial_init (void) {}
static inline void serial_putc (char c) {}
static inline char serial_getc (void) {return 0;}
static inline void serial_send_buf (const void *data, size_t cnt){}
static inline void serial_flush (void){}

static inline int dprintf (const char *fmt, ...){return 0;}
static inline int dvprintf (const char *fmt, va_list argptr) {return 0;}

#endif /*DEBUG_SERIAL*/

#endif /*_SERIAL_DEBUG_H_*/
