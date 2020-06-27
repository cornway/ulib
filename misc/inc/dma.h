#ifndef     __DMA_H__
#define     __DMA_H__


#include "stdint.h"
#include "abstract.h"
#include "iterable.h"
#include <nvic.h>

typedef struct dma_ops_s {
    int (*init) (irqn_t irq);
    int (*de_init) (void);
    int (*it) (irqn_t irq);
    int (*start) (void *addr, size_t len);
    int (*stop) (void);
} dma_ops_t;

class DmaFactory;

class Dma : public Link<Dma> {
    private:
        dma_ops_t ops;
        void *addr;
        size_t bytes_len;
        size_t bytes_left;
        size_t bytes_per_chunk;
        irqn_t irq;

        Dma (dma_ops_t *ops, void *addr, size_t len, size_t chunk_len, irqn_t irq);
        int It (void);
        int Start (void);
        int Stop (void);

    friend class DmaFactory;
    public :
};


class DmaFactory {
    private :
        vector::Vector dma;

    public :
        DmaFactory (void);
        Dma *AllocDma (dma_ops_t *ops, irqn_t irq);
        int StartDma (irqn_t irq);
        int StopDma (irqn_t irq);
        int ItDma (irqn_t irq);
};

#endif      /*__DMA_H__*/

