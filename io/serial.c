#include "stdint.h"
#include "string.h"
#include "stdarg.h"

#include <stm32f7xx_it.h>

#include "../int/tim_int.h"
#include "../int/term_int.h"

#include <misc_utils.h>
#include <debug.h>
#include <main.h>
#include <dev_conf.h>
#include "../int/nvic.h"
#include <heap.h>
#include <dev_io.h>

#if defined(BSP_DRIVER)

#if DEBUG_SERIAL

#define TX_FLUSH_TIMEOUT 200 /*MS*/

static void serial_fatal (void)
{
    for (;;) {}
}

#if !DEBUG_SERIAL_USE_DMA

#if DEBUG_SERIAL_BUFERIZED
#warning "DEBUG_SERIAL_BUFERIZED==true while DEBUG_SERIAL_USE_DMA==false"
#undef DEBUG_SERIAL_BUFERIZED
#define DEBUG_SERIAL_BUFERIZED 0
#endif

#ifdef DEBUG_SERIAL_USE_RX
#error "DEBUG_SERIAL_USE_RX only with DEBUG_SERIAL_USE_DMA==1"
#endif

#else

#ifndef DEBUG_SERIAL_USE_RX
#warning "DEBUG_SERIAL_USE_RX undefined, using TRUE"
#define DEBUG_SERIAL_USE_RX 1
#endif /*!DEBUG_SERIAL_USE_DMA*/

#endif /*!DEBUG_SERIAL_USE_DMA*/

#ifndef USE_STM32F769I_DISCO
#error "Not supported"
#endif

#include "../../common/int/uart_int.h"

extern void serial_led_on (void);
extern void serial_led_off (void);


static streambuf_t streambuf[STREAM_BUFCNT];

#endif /*DEBUG_SERIAL_BUFERIZED*/

#if DEBUG_SERIAL_USE_RX

static timer_desc_t serial_timer;

static rxstream_t rxstream;

#endif /*DEBUG_SERIAL_USE_RX*/

int32_t g_serial_rx_eof = '\n';

static void serial_flush_handler (int force);

static uart_desc_t *
debug_port (void)
{
    int i;

    for (i = 0; i < uart_desc_cnt; i++) {

        if (uart_desc_pool[i]->initialized &&
            (uart_desc_pool[i]->type == SERIAL_DEBUG)) {

            return uart_desc_pool[i];
        }
    }
    serial_fatal();
    return NULL;
}

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

static void _dbgstream_submit (uart_desc_t *uart_desc, const void *data, size_t cnt)
{
    HAL_StatusTypeDef status;

    status = serial_submit_to_hw(uart_desc, data, cnt);

    if (status != HAL_OK) {
        serial_fatal();
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
        serial_fatal();
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
    uart_desc_t *uart_desc = debug_port();
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
    serial_flush_handler(1);
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

#if DEBUG_SERIAL_USE_DMA

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

static void _serial_rx_cplt (uint8_t full)
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
    irqmask_t irq = dma_rx_irq_mask;
    char buf[DMA_RX_FIFO_SIZE + 1];
    int cnt;

    if (rxstream.eof != 0x3) {
        return;
    }

    irq_save(&irq);
    serial_io_fifo_flush(&rxstream, buf, &cnt);
    irq_restore(irq);

    if (inout_early_clbk) {
        inout_early_clbk(buf, cnt, '<');
    }
    bsp_inout_forward(buf, cnt, '<');
}

#endif


#endif /*DEBUG_SERIAL_USE_DMA*/

#if DEBUG_SERIAL_BUFERIZED

static void serial_flush_handler (int force)
{
    uart_desc_t *uart_desc = debug_port();
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
                dma_tx_waitflush(uart_desc);
            }
        }
    }
}

#endif /*DEBUG_SERIAL_BUFERIZED*/

#endif /*DEBUG_SERIAL*/

#endif
