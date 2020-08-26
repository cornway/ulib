#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <tim.h>
#include <nvic.h>
#include <debug.h>
#include <misc_utils.h>
#include <dev_io.h>
#include <heap.h>
#include <bsp_cmd.h>

#define P_RECORDS_MAX 128
#define P_MAX_DEEPTH 36

int g_profile_deep_level = -1;

enum {
    PFLAG_ENTER = (1 << 0),
    PFLAG_EXIT  = (1 << 1),
};

typedef V_PREPACK struct {
    char const      *func;
    uint32_t        timestamp;
    int8_t          next;
    int8_t          caller;
    uint8_t         levelsdeep;
    uint8_t         flags;
} V_POSTPACK record_t;

typedef struct {
    int16_t top, bottom;
} rhead_t;

static int profiler_print_dvar (void *p1, void *p2);

static record_t *records_pool = NULL;
static uint16_t last_alloced_record = 0;

static rhead_t record_levels[P_MAX_DEEPTH];
static int16_t profile_deepth = 0;
static int caller = -1;
timer_desc_t profile_timer_desc;

static inline uint32_t time_diff (uint32_t a, uint32_t b)
{
    uint32_t diff;
    if (a > b) {
        diff = (uint32_t)(-1)- b + a;
    } else {
        diff = b - a;
    }
    return diff;
}

static inline record_t *prof_alloc_rec (void)
{
    if (last_alloced_record >= P_RECORDS_MAX) {
        return NULL;
    }
    if (NULL == records_pool) {
        records_pool = (record_t *)heap_malloc(P_RECORDS_MAX * sizeof(record_t));
    }
    if (NULL == records_pool) {
        dprintf("No memory for profiler\n");
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
    if (profile_deepth <= g_profile_deep_level) {
        rec = prof_alloc_rec();
    }
    if (!rec) {
        return;
    }
    profile_deepth++;

    rec->func = func;
    rec->levelsdeep = profile_deepth - 1;
    rec->flags |= PFLAG_ENTER;
    rec->timestamp = hal_timer_value(&profile_timer_desc);
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
    if (!rec) {
        return;
    }
    profile_deepth--;

    rec->func = func;
    rec->levelsdeep = profile_deepth;
    rec->flags |= PFLAG_EXIT;
    rec->timestamp = hal_timer_value(&profile_timer_desc);

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

static void profiler_timer_init (void)
{

    profile_timer_desc.flags = TIM_RUNREG;
    profile_timer_desc.period = 0xffffffff;
    profile_timer_desc.presc = 1000000;
    profile_timer_desc.handler = NULL;
    profile_timer_desc.init = NULL;
    profile_timer_desc.deinit = NULL;

    if (hal_hires_timer_init(&profile_timer_desc) != 0) {
        dprintf("%s() : fail\n", __func__);
    }
}

static void profiler_timer_deinit (void)
{
    hal_timer_deinit(&profile_timer_desc);
}

void profiler_init (void)
{
    cmdvar_t dvar;

    profiler_reset();
    profiler_timer_init();

    dvar.ptr = (void *)profiler_print_dvar;
    dvar.ptrsize = sizeof(&profiler_print_dvar);
    dvar.type = DVAR_FUNC;
    cmd_register_var(&dvar, "profile");
    cmd_register_i32(&g_profile_deep_level, "proflvl");
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
                delta_tick = time_diff(records_pool[prev].timestamp, records_pool[cur].timestamp);
                delta_us = delta_tick;
                dprintf("\'%s\' %u.%03u ms; ",
                    records_pool[cur].func, delta_us / 1000, delta_us % 1000);
                caller = records_pool[prev].caller;
                if (caller >= 0) {
                    dprintf("Caller : \'%s\'", records_pool[caller].func);
                }
                dprintf("\n");
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
