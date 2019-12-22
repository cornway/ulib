#ifndef _SERIAL_DEBUG_H_
#define _SERIAL_DEBUG_H_

#include <stddef.h>
#include <stdarg.h>
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

#define PRINTF_SERIAL  1

#define BSP_DBG_API(func) ((bsp_debug_api_t *)(g_bspapi->dbg))->func

#if BSP_INDIR_API

#define uart_hal_tty_init             BSP_DBG_API(dev.init)
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
int uart_hal_tty_init (void);
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

int dprintf_safe (const char *fmt, ...) PRINTF;

extern int32_t g_serial_rx_eof;

#endif /*BSP_INDIR_API*/

int dvprintf (const char *fmt, va_list argptr);

#endif /*DEBUG_SERIAL*/

#endif /*_SERIAL_DEBUG_H_*/
