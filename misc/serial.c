#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <bsp_api.h>
#include <nvic.h>
#include <tim.h>
#include <misc_utils.h>
#include <debug.h>
#include <heap.h>
#include <dev_io.h>
#include <serial.h>
#include <bsp_cmd.h>
#include <term.h>

#define TX_FLUSH_TIMEOUT 200 /*MS*/

#if DEBUG_SERIAL_USE_DMA
#define SERIAL_TX_BUFFERIZED 1
#else
#define SERIAL_TX_BUFFERIZED 0
#endif

#define str_replace_2_ascii(str) d_stoalpha(str)

static tty_txbuf_t *_serial_tty_alloc_txbuf (serial_tty_t *tty);
static tty_txbuf_t *_serial_tty_free_txbuf (serial_tty_t *tty);


static char __look_for_LF (const char *str, int size)
{
    str = str + size - 1;
    while (*str && size > 0) {
        if (*str == '\n') {
            break;
        }
        str--; size--;
    }
    return *str;
}
static inline int __set_tstamp (serial_tty_t *tty, char *buf, int buflen)
{
    int size = 0;
    if (__tty_is_crlf_char(tty->last_tx_char)) {
        uint32_t msec = d_time();

        size = snprintf(buf, buflen, "[%4d.%03d] ", msec / 1000, msec % 1000);
    }
    return size;
}

static void tty_txbuf_append_data (tty_txbuf_t *stbuf, const void *data, size_t size)
{
    d_memcpy(stbuf->data + stbuf->data_cnt, data, size);
    if (stbuf->data_cnt == 0) {
        stbuf->timestamp = d_time() + TX_FLUSH_TIMEOUT;
    }
    stbuf->data_cnt += size;
}

static int serial_tty_flush (serial_tty_t *tty)
{
    tty_txbuf_t *txbuf;
    int size = 0;

    txbuf = _serial_tty_free_txbuf(tty);
    if (txbuf && txbuf->data_cnt) {

        if (tty->tx_start(tty, txbuf) < 0) {
            fatal_error("%s() : fail\n", __func__);
        }
        size = txbuf->data_cnt;
        txbuf->data_cnt = 0;
        txbuf->timestamp = 0;
    }
    return size;
}

int serial_tty_append_txbuf (serial_tty_t *tty, const void *data, size_t size)
{
    tty_txbuf_t *txbuf = tty->txbuf_list.head;

    if (!txbuf) {
        txbuf = _serial_tty_alloc_txbuf(tty);
        assert(txbuf);
    }
    if (txbuf == tty->txbuf_pending) {
        return -1;
    }
    if (tty->tx_bufsize && (size > tty->tx_bufsize)) {
        size = tty->tx_bufsize;
    }

    tty->last_tx_char = __look_for_LF((const char *)data, size);

    if (size >= (tty->tx_bufsize - txbuf->data_cnt)) {
        txbuf = _serial_tty_alloc_txbuf(tty);
        serial_tty_flush(tty);
    }
    if (txbuf && size) {
        tty_txbuf_append_data(txbuf, data, size);
    }
    return size;
}

int bsp_serial_send (char *buf, size_t cnt)
{
    serial_tty_t *tty = hal_tty_get_vcom_port();
    irqmask_t irq_flags = tty->irqmask;
    int ret = 0;

    tty->inout_hook(buf, cnt, '>');

    irq_save(&irq_flags);
    ret = serial_tty_append_txbuf(tty, buf, cnt);
    irq_restore(irq_flags);
    return ret;
}

void serial_flush (void)
{
    hal_tty_flush_any();
}

int serial_tty_tx_flush_proc (serial_tty_t *tty, d_bool flush)
{
    int size = 0;

    if (flush) {
        while (tty->txbuf_list.head) {
            size += serial_tty_flush(tty);
        }
    } else {
        tty_txbuf_t *a = tty->txbuf_list.head;
        if (d_time() > a->timestamp) {
            size = serial_tty_flush(tty);
        }
    }
    return size;
}

void serial_safe_mode (int safe)
{
    serial_tty_t *tty = hal_tty_get_vcom_port();
    if (safe) {
        tty->tx_bufsize = 0;
    } else {
        tty->tx_bufsize = 512;
    }
}

void serial_putc (char c)
{
    dprintf("%c", c);
}

char serial_getc (void)
{
    return 0;
}

int __dvprintf (const char *fmt, va_list argptr)
{
    serial_tty_t *tty = hal_tty_get_vcom_port();
    char            string[1024];
    int size =      0;

    size = __set_tstamp(tty, string, sizeof(string));

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

int aprint (const char *str, int size)
{
    char            string[1024];
    d_memcpy(string, str, size);
    str_replace_2_ascii(string);
    bsp_serial_send(string, size);
    return size;
}

void serial_tickle (void)
{
    char buf[512];
    int cnt = sizeof(buf);
    serial_tty_t *tty = hal_tty_get_vcom_port();

    tty->rx_poll(tty, buf, &cnt);
    if (cnt > 0) {
        tty->inout_hook(buf, cnt, '<');
    }
}

void _serial_tty_preinit (serial_tty_t *tty)
{
    tty_txbuf_t *txbuf;
    int i;

    tty->txbuf_list.head = NULL;
    tty->txbuf_list.tail = NULL;
    tty->txbuf_rdy.head = NULL;
    tty->txbuf_rdy.tail = NULL;

    for (i = 0; i < 2; i++) {
        txbuf = (tty_txbuf_t *)dma_alloc(sizeof(tty_txbuf_t));
        if (!txbuf) {
            break;
        }
        txbuf->next = tty->txbuf_rdy.head;
        tty->txbuf_rdy.head = txbuf;
        if (!tty->txbuf_rdy.tail) {
            tty->txbuf_rdy.tail = txbuf;
        }
    }

    tty->txbuf_pending =  NULL;
}

static tty_txbuf_t *_serial_tty_alloc_txbuf (serial_tty_t *tty)
{
    irqmask_t irq_flags = tty->irqmask;
    tty_txbuf_t *txbuf;

    irq_save(&irq_flags);
    txbuf = tty->txbuf_rdy.head;
    if (!tty->txbuf_rdy.head) {
        tty->txbuf_rdy.tail = NULL;
    }

    if (!txbuf) {
        irq_restore(irq_flags);
        return NULL;
    }
    tty->txbuf_rdy.head = txbuf->next;

    if (!tty->txbuf_list.head) {
        tty->txbuf_list.head = txbuf;
    } else {
        tty->txbuf_list.tail->next = txbuf;
    }
    tty->txbuf_list.tail = txbuf;
    txbuf->next = NULL;
    irq_restore(irq_flags);
    
    txbuf->data_cnt = 0;
    return txbuf;
}

static tty_txbuf_t *_serial_tty_free_txbuf (serial_tty_t *tty)
{
    irqmask_t irq_flags = tty->irqmask;
    tty_txbuf_t *txbuf;

    irq_save(&irq_flags);
    if (!tty->txbuf_list.tail) {
        irq_restore(irq_flags);
        tty->txbuf_list.tail = NULL;
    }

    txbuf = tty->txbuf_list.head;
    tty->txbuf_list.head = txbuf->next;
    if (!tty->txbuf_list.head) {
        tty->txbuf_list.tail = NULL;
    }

    if (!tty->txbuf_rdy.head) {
        tty->txbuf_rdy.head = txbuf;
    } else {
        tty->txbuf_rdy.tail->next = txbuf;
    }
    tty->txbuf_rdy.tail = txbuf;
    txbuf->next = NULL;

    irq_restore(irq_flags);
    return txbuf;
}



