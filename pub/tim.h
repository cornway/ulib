#ifndef __TIM_H__
#define __TIM_H__

#ifdef __cplusplus
    extern "C" {
#endif

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

int hal_timer_init (timer_desc_t *desc, void *hw, irqn_t irqn);
int hal_hires_timer_init (timer_desc_t *desc);
int hal_timer_deinit (timer_desc_t *desc);
uint32_t hal_timer_value (timer_desc_t *desc);
void hal_timer_irq_handler (void *hw);


#ifdef __cplusplus
    }
#endif

#endif /*__TIM_H__*/
