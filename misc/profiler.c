#if defined(BSP_DRIVER)

#include "stdint.h"
#include "string.h"
#include <main.h>
#include "int/tim_int.h"
#include <bsp_cmd.h>
#include <debug.h>
#include <misc_utils.h>
#include <debug.h>
#include <dev_io.h>
#include <heap.h>

#define P_RECORDS_MAX 1024
#define P_MAX_DEEPTH 36

int g_profile_deep_level = 1;
int g_profile_timer_tsf = 1;
static __IO uint32_t *prof_timer_cnt_ptr = NULL;
static uint8_t prof_time_init_ok = 0;

enum {
    PFLAG_ENTER = (1 << 0),
    PFLAG_EXIT  = (1 << 1),
};

typedef struct {
    char const      *func;
    uint32_t        cycles;
    int16_t         next;
    int16_t         caller;
    uint8_t         levelsdeep;
    uint8_t         flags;
} record_t;

typedef struct {
    int16_t top, bottom;
} rhead_t;

static int profiler_print_dvar (void *p1, void *p2);

static record_t *records_pool = NULL;
static uint16_t last_alloced_record = 0;

static rhead_t record_levels[P_MAX_DEEPTH];
static int16_t profile_deepth = 0;
static int caller = -1;

extern uint32_t SystemCoreClock;

static uint32_t clocks_per_us = (0U);

static inline uint32_t _cpu_cycles_to_us (uint32_t cycles)
{
    return cycles / clocks_per_us;
}

static inline uint32_t _cpu_us_to_cycles (uint32_t us)
{
    return us * clocks_per_us;
}

static inline uint32_t _cpu_cycles_diff (uint32_t a, uint32_t b)
{
    uint32_t diff;
    if (a > b) {
        diff = (uint32_t)(-1)- b + a;
    } else {
        diff = b - a;
    }
    return diff;
}

static void delay_us (uint32_t us)
{
    volatile uint32_t cycles = _cpu_us_to_cycles(us);

    while (cycles--)
    {
    }
}

static inline record_t *prof_alloc_rec (void)
{
    if (last_alloced_record >= P_RECORDS_MAX) {
        return NULL;
    }
    if (NULL == records_pool) {
        records_pool = heap_malloc(P_RECORDS_MAX * sizeof(record_t));
    }
    if (NULL == records_pool) {
        return NULL;
    }
    return &records_pool[last_alloced_record++];
}

static void prof_link_rec (record_t *rec)
{
    rhead_t *head = &record_levels[rec->levelsdeep];
    int idx = (int)(rec - records_pool);
    record_t *prev_rec;

    rec->next = -1;

    if (head->top < 0) {
        head->top = idx;
        head->bottom = idx;
        goto linkdone;
    }
    prev_rec = &records_pool[head->top];
    prev_rec->next = idx;
    head->top = idx;
    rec->caller = -1;
linkdone:
    if (rec->flags & PFLAG_ENTER) {
        if (caller >= 0) {
            rec->caller = caller;
        }
        caller = idx;
    } else if (rec->flags & PFLAG_EXIT) {
        caller = rec->caller;
    }
}

void _profiler_enter (const char *func, int line)
{
    record_t *rec = NULL;

    if (profile_deepth >= P_MAX_DEEPTH) {
        return;
    }
    profile_deepth++;

    if (profile_deepth <= g_profile_deep_level) {
        rec = prof_alloc_rec();
    }

    if (!rec) {
        return;
    }
    rec->func = func;
    rec->levelsdeep = profile_deepth - 1;
    rec->flags |= PFLAG_ENTER;
    if (prof_time_init_ok && g_profile_timer_tsf == 1) {
        rec->cycles = *prof_timer_cnt_ptr;
    } else {
        rec->cycles = DWT->CYCCNT;
    }
    prof_link_rec(rec);
}

void _profiler_exit (const char *func, int line)
{
    record_t *rec = NULL;

    if (profile_deepth <= 0) {
        return;
    }
    if (profile_deepth <= g_profile_deep_level) {
        rec = prof_alloc_rec();
    }
    profile_deepth--;
    if (!rec) {
        return;
    }
    rec->func = func;
    rec->levelsdeep = profile_deepth;
    rec->flags |= PFLAG_EXIT;
    if (prof_time_init_ok && g_profile_timer_tsf == 1) {
        rec->cycles = *prof_timer_cnt_ptr;
    } else {
        rec->cycles = DWT->CYCCNT;
    }

    prof_link_rec(rec);
}

void profiler_reset (void)
{
    int i;

    if (NULL == records_pool) {
        return;
    }
    d_memzero(records_pool, P_MAX_DEEPTH * sizeof(record_t));
    heap_free(records_pool);
    records_pool = NULL;

    for (i = 0; i < P_MAX_DEEPTH; i++) {
        record_levels[i].top = -1;
        record_levels[i].bottom = -1;
    }

    last_alloced_record = 0;
    profile_deepth = 0;
}

timer_desc_t profile_timer_desc;

static void profile_timer_msp_init (timer_desc_t *desc)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
}

static void profile_timer_msp_deinit (timer_desc_t *desc)
{
    __HAL_RCC_TIM2_CLK_DISABLE();
}

static void profile_timer_handler (timer_desc_t *desc)
{

}

void TIM2_IRQHandler (void)
{
    HAL_TIM_IRQHandler(&profile_timer_desc.handle);
}

static void profiler_timer_init (void)
{

    profile_timer_desc.flags = TIM_RUNREG;
    profile_timer_desc.period = 0xffffffff;
    profile_timer_desc.presc = 1000000;
    profile_timer_desc.handler = profile_timer_handler;
    profile_timer_desc.init = profile_timer_msp_init;
    profile_timer_desc.deinit = profile_timer_msp_deinit;
    profile_timer_desc.hw = TIM2;
    profile_timer_desc.irq = TIM2_IRQn;
    if (hal_tim_init(&profile_timer_desc) == 0) {
        prof_time_init_ok = 1;
    }
    if (!prof_time_init_ok) {
        dprintf("%s() : fail\n", __func__);
    }
    prof_timer_cnt_ptr = &profile_timer_desc.hw->CNT;
}

static void profiler_timer_deinit (void)
{
    hal_tim_deinit(&profile_timer_desc);
}

void profiler_init (void)
{
    cmdvar_t dvar;
    DWT->CTRL |= 1 ; // enable the counter
    DWT->CYCCNT = 0; // reset the counter
    delay_us(1);
    profiler_reset();

    clocks_per_us = SystemCoreClock / 1000000U;

    if (g_profile_timer_tsf == 1) {
        profiler_timer_init();
    }

    dvar.ptr = profiler_print_dvar;
    dvar.ptrsize = sizeof(&profiler_print_dvar);
    dvar.type = DVAR_FUNC;
    cmd_register_var(&dvar, "profile");
    cmd_register_i32(&g_profile_deep_level, "proflvl");
    cmd_register_i32(&g_profile_timer_tsf, "proftsf");
}

void profiler_deinit (void)
{
    dprintf("%s() :\n", __func__);
    profiler_timer_deinit();
}

void profiler_print (void)
{
    rhead_t *tail = NULL;
    int i, cur, next, prev, entrycnt = 0;
    uint32_t delta_tick, delta_us;
    int caller = -1;

    dprintf("%s() : \n", __func__);
    dprintf("core clock = \'%u\', clocks per us = \'%u\'\n",
        SystemCoreClock, clocks_per_us);


    if (NULL == records_pool) {
        return;
    }

    for (i = 0; i < P_MAX_DEEPTH; i++) {

        tail = &record_levels[i];
        if (tail->bottom < 0) {
            break;
        }
        cur = tail->bottom;//records_pool[head->top].next;
        next = records_pool[cur].next;
        prev = -1;
        do {

            if (entrycnt & 1) {
                delta_tick = _cpu_cycles_diff(records_pool[prev].cycles, records_pool[cur].cycles);
                if (prof_time_init_ok && g_profile_timer_tsf == 1) {
                    delta_us = delta_tick;
                    dprintf("function \'%s\' took %u.%03u ms\n",
                        records_pool[cur].func, delta_us / 1000, delta_us % 1000);
                } else {
                    delta_us = _cpu_cycles_to_us(delta_tick);
                    dprintf("function \'%s\' took %u cycles or %u.%03u ms\n",
                        records_pool[cur].func, delta_tick, delta_us / 1000, delta_us % 1000);
                }
                caller = records_pool[prev].caller;
                if (caller >= 0) {
                    dprintf("Caller : \'%s\'\n\n", records_pool[caller].func);
                }
            }

            prev = cur;
            cur = next;
            next = records_pool[cur].next;
            entrycnt++;
        } while (cur >= 0);
    }
    dprintf("%s() : finish\n", __func__);
}

static int profiler_print_dvar (void *p1, void *p2)
{
    profiler_print();
    return -1;
}

#endif
