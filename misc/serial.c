#include "stdint.h"
#include "string.h"
#include "stdarg.h"

#include <nvic.h>
#include <tim.h>
#include <misc_utils.h>
#include <debug.h>
#include <main.h>
#include <dev_conf.h>
#include <heap.h>
#include <dev_io.h>

#define str_replace_2_ascii(str) d_stoalpha(str)

#define TX_FLUSH_TIMEOUT 200 /*MS*/

#if !DEBUG_SERIAL_USE_DMA

#if DEBUG_SERIAL_BUFERIZED
#warning "DEBUG_SERIAL_BUFERIZED==true while DEBUG_SERIAL_USE_DMA==false"
#undef DEBUG_SERIAL_BUFERIZED
#define DEBUG_SERIAL_BUFERIZED 0
#endif

#ifdef DEBUG_SERIAL_USE_RX
#error "DEBUG_SERIAL_USE_RX only with DEBUG_SERIAL_USE_DMA==1"
#endif

#else /*DEBUG_SERIAL_USE_DMA*/

#endif /*!DEBUG_SERIAL_USE_DMA*/

#ifndef DEBUG_SERIAL_USE_RX
#warning "DEBUG_SERIAL_USE_RX undefined, using TRUE"
#define DEBUG_SERIAL_USE_RX 1
#endif /*!DEBUG_SERIAL_USE_DMA*/

#include "../../common/int/uart_int.h"

extern void serial_led_on (void);
extern void serial_led_off (void);

#if DEBUG_SERIAL_BUFERIZED

static streambuf_t streambuf[STREAM_BUFCNT];

#endif /*DEBUG_SERIAL_BUFERIZED*/

#if DEBUG_SERIAL_USE_RX

extern timer_desc_t serial_timer;

static rxstream_t rxstream;

#endif /*DEBUG_SERIAL_USE_RX*/

int32_t g_serial_rx_eof = '\n';

#if SERIAL_TSF

#define MSEC 1000

static int prev_putc = -1;

static void inline
__proc_tsf (const char *str, int size)
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
__insert_tsf (const char *fmt, char *buf, int max)
{
    uint32_t msec, sec;

    if (prev_putc < 0 ||
        prev_putc != '\n') {
        return 0;
    }
    msec = HAL_GetTick();
    sec = msec / MSEC;
    return snprintf(buf, max, "[%4d.%03d] ", sec, msec % MSEC);
}

#endif /*SERIAL_TSF*/

#if DEBUG_SERIAL_BUFERIZED

static inline void _dbgstream_submit (uart_desc_t *uart_desc, const void *data, size_t cnt)
{
    if (serial_submit_to_hw(uart_desc, data, cnt) < 0) {
        fatal_error("%s() : fail\n");
    }
}

static void __dbgstream_send (uart_desc_t *uart_desc, streambuf_t *stbuf)
{
    _dbgstream_submit(uart_desc, stbuf->data, stbuf->bufposition);
    stbuf->bufposition = 0;
    stbuf->timestamp = 0;
}

static void dbgstream_apend_data (streambuf_t *stbuf, const void *data, size_t size)
{
    char *p = stbuf->data + stbuf->bufposition;
    d_memcpy(p, data, size);
    if (stbuf->bufposition == 0) {
        stbuf->timestamp = d_time();
    }
    stbuf->bufposition += size;
}

static inline int
dbgstream_submit (uart_desc_t *uart_desc, const void *data, size_t size, d_bool flush)

{
    streambuf_t *active_stream = &streambuf[uart_desc->active_stream & STREAM_BUFCNT_MS];

    if (size > STREAM_BUFSIZE) {
        size = STREAM_BUFSIZE;
    }

#if SERIAL_TSF
    __proc_tsf((const char *)data, size);
#endif

    if (flush || size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        __dbgstream_send(uart_desc, active_stream);
        active_stream = &streambuf[(++uart_desc->active_stream) & STREAM_BUFCNT_MS];
    }
    if (size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        fatal_error("%s() : fail\n");
    }
    if (size) {
        dbgstream_apend_data(active_stream, data, size);
    }
    return size;
}

#else /*DEBUG_SERIAL_BUFERIZED*/

static inline int
dbgstream_submit (uart_desc_t *uart_desc, const void *data, size_t size, d_bool flush)
{
    UNUSED(uart_desc);
    UNUSED(data);
    UNUSED(size);
}

#endif /*DEBUG_SERIAL_BUFERIZED*/

int bsp_serial_send (char *buf, size_t cnt)
{
    irqmask_t irq_flags = serial_timer.irqmask;
    uart_desc_t *uart_desc = uart_get_stdio_port();
    int ret = 0;

    if (inout_early_clbk) {
        inout_early_clbk(buf, cnt, '>');
    }
    bsp_inout_forward(buf, cnt, '>');

    irq_save(&irq_flags);

    ret = dbgstream_submit(uart_desc, buf, cnt, d_false);
    if (ret <= 0) {
        ret = dbgstream_submit(uart_desc, buf, cnt, d_true);
    }

    irq_restore(irq_flags);

    return ret;
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

void serial_flush (void)
{
    irqmask_t irq_flags = serial_timer.irqmask;

    irq_save(&irq_flags);
    serial_rx_flush_handler(1);
    irq_restore(irq_flags);
}

int __dvprintf (const char *fmt, va_list argptr)
{
    char            string[1024];
    int size = 0;
#if SERIAL_TSF
    size = __insert_tsf(fmt, string, sizeof(string));
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

static uint8_t __check_rx_crlf (char c)
{
    if (!g_serial_rx_eof) {
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

void serial_rx_cplt_handler (uint8_t full)
{
    int dmacplt = sizeof(rxstream.dmabuf) / 2;
    int cnt = dmacplt;
    char *src;

    src = &rxstream.dmabuf[full * dmacplt];

    while (cnt) {
        rxstream.fifo[rxstream.fifoidx++ & (DMA_RX_FIFO_SIZE - 1)] = *src;
        rxstream.eof |= __check_rx_crlf(*src);
        src++; cnt--;
    }
    if (!g_serial_rx_eof) {
        rxstream.eof = 3; /*\r & \n met*/
    }
}

static void serial_io_fifo_flush (rxstream_t *rx, char *dest, int *pcnt)
{
    int16_t cnt = (int16_t)(rx->fifoidx - rx->fifordidx);

    if (cnt < 0) {
        cnt = -cnt;
    }
    *pcnt = cnt;
    while (cnt > 0) {
        *dest = rx->fifo[rx->fifordidx++ & (DMA_RX_FIFO_SIZE - 1)];
        cnt--;
        dest++;
    }
    *dest = 0;
    /*TODO : Move next line after crlf to the begining,
      to handle multiple lines.
    */
    rx->eof = 0;
}

#if DEBUG_SERIAL_USE_RX

void serial_tickle (void)
{
#if DEBUG_SERIAL_USE_DMA
extern irqmask_t dma_rx_irq_mask;
    irqmask_t irq = dma_rx_irq_mask;
#endif
    char buf[DMA_RX_FIFO_SIZE + 1];
    int cnt;

    if (rxstream.eof != 0x3) {
        return;
    }
#if DEBUG_SERIAL_USE_DMA
    irq_save(&irq);
    serial_io_fifo_flush(&rxstream, buf, &cnt);
    irq_restore(irq);
#else
    serial_io_fifo_flush(&rxstream, buf, &cnt);
#endif /*DEBUG_SERIAL_USE_DMA*/
    if (inout_early_clbk) {
        inout_early_clbk(buf, cnt, '<');
    }
    bsp_inout_forward(buf, cnt, '<');
}

#endif

#if DEBUG_SERIAL_BUFERIZED

void serial_rx_flush_handler (int force)
{
    uart_desc_t *uart_desc = uart_get_stdio_port();
    streambuf_t *active_stream = &streambuf[uart_desc->active_stream & STREAM_BUFCNT_MS];
    uint32_t time;

    if (!uart_desc->uart_tx_ready) {
        return;
    }

    if (0 != active_stream->bufposition) {

        time = d_time();

        if ((time - active_stream->timestamp) > TX_FLUSH_TIMEOUT ||
            force) {

            active_stream->timestamp = time;
            active_stream->data[active_stream->bufposition++] = '\n';
            dbgstream_submit(uart_desc, NULL, 0, d_true);
            if (force) {
                uart_hal_sync(uart_desc);
            }
        }
    }
}

#endif /*DEBUG_SERIAL_BUFERIZED*/