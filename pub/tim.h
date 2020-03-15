#ifndef __TIM_H__
#define __TIM_H__

#include <nvic.h>
 
typedef enum {
    TIM_NONE,
    TIM_RUNIT,
    TIM_RUNREG,
} timflags_t;

typedef struct timer_desc_s {
    void *parent;
    irqn_t irq;
    irqmask_t irqmask;
    void (*handler) (struct timer_desc_s *);
    void (*init) (struct timer_desc_s *);
    void (*deinit) (struct timer_desc_s *);
    uint32_t period;
    uint32_t presc;
    timflags_t flags;
} timer_desc_t;

void *tim_hal_alloc_hw (uint32_t flags, irqn_t *irq);
int hal_tim_init (timer_desc_t *desc, void *hw, irqn_t irqn);
int hal_tim_deinit (timer_desc_t *desc);
uint32_t tim_hal_get_cycles (timer_desc_t *desc);
uint32_t cpu_hal_get_cycles (void);
void cpu_hal_init_cycles (void);


#endif /*__TIM_H__*/
