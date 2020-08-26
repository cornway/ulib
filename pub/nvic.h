#ifndef __NVIC_H__
#define __NVIC_H__

#ifdef __cplusplus
    extern "C" {
#endif

#include <stdint.h>

#if (NVIC_IRQ_MAX > 32)
#error "uint32_t -> uint64_t"
#else
typedef uint32_t irqmask_t;
typedef int32_t irqn_t;
#define NVIC_IRQ_MASK (0xffffffff)
#endif

void irq_save (irqmask_t *flags);
void irq_restore (irqmask_t flags);
void irq_bmap (irqmask_t *flags);
void NVIC_dump (void);
void irq_destroy (void);

#ifdef __cplusplus
    }
#endif

#endif /*__NVIC_H__*/
