#ifndef     __DMA_H__
#define     __DMA_H__


#include "../inc/dma.h"

Dma::Dma(dma_ops_t * ops, void *addr, size_t len, size_t chunk_len, irqn_t irq) :
        irq(irq),
        addr(addr),
        bytes_len(len),
        bytes_left(len),
        bytes_per_chunk(chunk_len),
        ops(*ops)
{
}

int Dma::Start(void) {
    this->ops.init(&this->ops, this->irq);
    return this->ops.start();
}

int Dma::Stop(void) {

}

int Dma::It(void) {

}

DmaFactory::DmaFactory(void) {

}

Dma *DmaFactory::AllocDma(dma_ops_t * ops, irqn_t irq) {

}

int DmaFactory::StartDma(irqn_t irq) {

}

int DmaFactory::StopDma(irqn_t irq) {

}

int DmaFactory::ItDma(irqn_t irq) {

}


#endif      /*__DMA_H__*/

