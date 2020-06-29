#ifndef __SERIAL_H__
#define __SERIAL_H__

#ifdef __cplusplus
    extern "C" {
#endif

#define STREAM_BUFCNT 2
#define STREAM_BUFCNT_MS (STREAM_BUFCNT - 1)

#define DMA_RX_SIZE (1 << 1)
#define DMA_RX_FIFO_SIZE (1 << 8)

typedef struct {
    uint16_t fifoidx;
    uint16_t fifordidx;
    uint16_t eof;
    char dmabuf[DMA_RX_SIZE];
    char fifo[DMA_RX_FIFO_SIZE];
} tty_rxbuf_t;

typedef struct tty_txbuf_s {
    struct tty_txbuf_s *next;
    uint16_t data_cnt;
    uint32_t timestamp;
    char data[512];
} tty_txbuf_t;

typedef struct {
    tty_txbuf_t *head, *tail;
} txbuf_list_t;

typedef struct serial_tty_s {
    int (*tx_start) (struct serial_tty_s *tty, tty_txbuf_t *txbuf);
    int (*rx_poll) (struct serial_tty_s *tty, char *dest, int *pcnt);
    int (*inout_hook) (char *buf, int size, char dir);

    txbuf_list_t txbuf_rdy;
    txbuf_list_t txbuf_list;
    tty_txbuf_t *txbuf_pending;
    int         tx_bufsize;
    char        last_tx_char;

    tty_rxbuf_t rxbuf;

    irqn_t      irqmask;
    unsigned int initialized: 1;
} serial_tty_t;

serial_tty_t *hal_tty_get_vcom_port (void);
void hal_tty_flush_any (void);
int hal_tty_vcom_attach (void);
void hal_tty_destroy_any (void);

void serial_safe_mode (int safe);
void _serial_tty_preinit (serial_tty_t *tty);

#ifdef __cplusplus
    }
#endif

#endif /* __SERIAL_H__ */
