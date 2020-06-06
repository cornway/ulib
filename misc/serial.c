#include "stdint.h"
#include "string.h"
#include "stdarg.h"

#include <nvic.h>
#include <tim.h>
#include <misc_utils.h>
#include <debug.h>
#include <main.h>
#include <config.h>
#include <heap.h>
#include <dev_io.h>

#if DEBUG_SERIAL_USE_DMA
#define SERIAL_TX_BUFFERIZED 1
#else
#define SERIAL_TX_BUFFERIZED 0
#endif

#define str_replace_2_ascii(str) d_stoalpha(str)

#include "../../common/int/uart_int.h"

extern void serial_led_on (void);
extern void serial_led_off (void);

#if SERIAL_TX_BUFFERIZED

#define STREAM_BUFSIZE 512
#define STREAM_BUFCNT 2
#define STREAM_BUFCNT_MS (STREAM_BUFCNT - 1)

typedef struct {
    char data[STREAM_BUFSIZE + 1];
    int  bufposition;
    uint32_t timestamp;
} streambuf_t;

static streambuf_t streambuf[STREAM_BUFCNT];
#ifndef SERIAL_TX_TIMESTAMP
#define SERIAL_TX_TIMESTAMP 1
#endif
#endif /*SERIAL_TX_BUFFERIZED*/

int32_t g_serial_rxtx_eol_sens = '\n';

#if SERIAL_TX_TIMESTAMP

static int prev_putc = -1;

static void inline
__scan_last_char (const char *str, int size)
{
    if (!str || !size) {
        return;
    }
    str = str + size - 1;
    while (*str) {
        prev_putc = *str;
        if (*str == '\n') {
            break;
        }
        str--;
    }
}

static inline int
__insert_tx_time_ms (const char *fmt, char *buf, int max)
{
    uint32_t msec, sec;

    if (prev_putc < 0 ||
        prev_putc != '\n') {
        return 0;
    }
    msec = d_time();
    return snprintf(buf, max, "[%4d.%03d] ", msec / 1000, msec % 1000);
}
#endif /*SERIAL_TX_TIMESTAMP*/

#if SERIAL_TX_BUFFERIZED
static void __submit_to_hw (uart_desc_t *uart_desc, streambuf_t *stbuf)
{
    if (uart_hal_submit_tx_data(uart_desc, stbuf->data, stbuf->bufposition) < 0) {
        fatal_error("%s() : fail\n", __func__);
    }
    stbuf->bufposition = 0;
    stbuf->timestamp = 0;
}

static void __buf_append_data (streambuf_t *stbuf, const void *data, size_t size)
{
    char *p = stbuf->data + stbuf->bufposition;
    d_memcpy(p, data, size);
    if (stbuf->bufposition == 0) {
        stbuf->timestamp = d_time();
    }
    stbuf->bufposition += size;
}

int serial_submit_tx_data (uart_desc_t *uart_desc, const void *data, size_t size, d_bool flush)
{
    streambuf_t *active_stream = &streambuf[uart_desc->tx_id & STREAM_BUFCNT_MS];

    if (size > STREAM_BUFSIZE) {
        size = STREAM_BUFSIZE;
    }

#if SERIAL_TX_TIMESTAMP
    __scan_last_char((const char *)data, size);
#endif
    if (uart_desc->tx_direct) {
        __buf_append_data(active_stream, data, size);
        __submit_to_hw(uart_desc, active_stream);
        return size;
    }
    if (flush || size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        __submit_to_hw(uart_desc, active_stream);
        active_stream = &streambuf[(++uart_desc->tx_id) & STREAM_BUFCNT_MS];
    }
    if (size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        fatal_error("%s() : fail\n", __func__);
    }
    if (size) {
        __buf_append_data(active_stream, data, size);
    }
    return size;
}

int bsp_serial_send (char *buf, size_t cnt)
{
extern timer_desc_t uart_hal_wdog_tim;
    irqmask_t irq_flags = uart_hal_wdog_tim.irqmask;
    uart_desc_t *uart_desc = uart_get_stdio_port();
    int ret = 0;

    if (inout_early_clbk) {
        inout_early_clbk(buf, cnt, '>');
    }
    bsp_inout_forward(buf, cnt, '>');

    irq_save(&irq_flags);
    ret = serial_submit_tx_data(uart_desc, buf, cnt, d_false);
    if (ret <= 0) {
        ret = serial_submit_tx_data(uart_desc, buf, cnt, d_true);
    }
    irq_restore(irq_flags);
    return ret;
}

void serial_flush (void)
{
    uart_hal_tx_flush_all();
}

#else /*SERIAL_TX_BUFFERIZED*/

int bsp_serial_send (char *buf, size_t cnt)
{
    uart_desc_t *uart_desc = uart_get_stdio_port();
    
    return uart_hal_submit_tx_data(uart_desc, buf, cnt);
}

void serial_flush (void)
{
}

#endif /*SERIAL_TX_BUFFERIZED*/

void serial_safe_mode (int safe)
{
    uart_desc_t *uart_desc = uart_get_stdio_port();
    uart_hal_set_tx_mode(uart_desc, !safe);
}

void serial_putc (char c)
{
    dprintf("%c", c);
}

char serial_getc (void)
{
    /*TODO : Use Rx from USART */
    return 0;
}

int __dvprintf (const char *fmt, va_list argptr)
{
    char            string[1024];
    int size = 0;
#if SERIAL_TX_TIMESTAMP
    size = __insert_tx_time_ms(fmt, string, sizeof(string));
#endif
    size += vsnprintf(string + size, sizeof(string) - size, fmt, argptr);
    bsp_serial_send(string, size);
    return size;
}

int dprintf (const char *fmt, ...)
{
    va_list         argptr;
    int size;

    va_start (argptr, fmt);
    size = __dvprintf(fmt, argptr);
    va_end (argptr);
    return size;
}

/*prints ascii only(printable?)*/
int aprint (const char *str, int size)
{
    char            string[1024];
    d_memcpy(string, str, size);
    str_replace_2_ascii(string);
    bsp_serial_send(string, size);
    return size;
}

void serial_hal_get_tx_buf (uart_desc_t *uart_desc, uint32_t *tstamp, int *pos)
{
    streambuf_t *a = &streambuf[uart_desc->tx_id & STREAM_BUFCNT_MS];
    *tstamp = a->timestamp;
    *pos = a->bufposition;
}

uint8_t __tty_is_crlf_char (char c)
{
    if (!g_serial_rxtx_eol_sens) {
        return 0;
    }
    if (c == '\r') {
        return 0x1;
    }
    if (c == '\n') {
        return 0x3;
    }
    return 0;
}

void serial_tickle (void)
{
    char buf[512];
    int cnt = sizeof(buf), left;
    uart_desc_t *uart_desc = uart_get_stdio_port();

    left = uart_hal_rx_flush(uart_desc, buf, &cnt);
    if (cnt > 0) {
        if (inout_early_clbk) {
            inout_early_clbk(buf, cnt, '<');
        }
        bsp_inout_forward(buf, cnt, '<');
    }
}

