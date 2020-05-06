#include <stdlib.h>

#include <misc_utils.h>
#include <config.h>
#include <arch.h>
#ifdef BOOT
#include "../../common/int/mpu.h"
#endif
#include <debug.h>
#include <bsp_sys.h>

#define MALLOC_MAGIC       0x75738910

#if HEAP_TRACE

#define heap_dbg(args...) \
    dprintf("[HEAP DEBUG] " args)
#else
#define heap_dbg(args...)
#endif

typedef struct {
    size_t magic;
    size_t size;
    size_t freeable;
} mchunk_t;

static int heap_size_total = -1;

static inline int
__heap_aligned (void *p)
{
    uint8_t pad = sizeof(arch_word_t) - 1;
    if (((arch_word_t)p) & pad) {
        return 0;
    }
    return 1;
}

#if !defined(BOOT)

static inline void
__heap_check_margin (size_t        size)
{
    size = heap_size_total - size - sizeof(mchunk_t);
    if (size < 0) {
        fatal_error("__heap_check_margin : exceeds by %d bytes\n", -size);
    }
}

#else

static uint8_t *heap_user_mem_ptr = NULL;
static arch_word_t heap_user_size = 0;

#endif

#if HEAP_TRACE
static inline void *
__heap_malloc (size_t      size, int freeable, const char *func)
#else
static inline void *
__heap_malloc (size_t      size, int freeable)
#endif
{
    mchunk_t *p;

    size = size + sizeof(mchunk_t);
    size = ROUND_UP(size, sizeof(arch_word_t));
    p = (mchunk_t *)malloc(size);
    if (!p) {
        heap_dbg("[%s] Failed to allocate [%u] bytes\n", func, size);
        return NULL;
    }
    heap_size_total -= size;
    p->magic = MALLOC_MAGIC;
    p->freeable = freeable;
    p->size = size;
    return (void *)(p + 1);
}

#if HEAP_TRACE
static inline void
__heap_free (void *_p, const char *func)
#else
static inline void
__heap_free (void *_p)
#endif
{
extern void m_free (void *);
extern void *m_exist (void *);
    mchunk_t *p = (mchunk_t *)_p;
    if (NULL == _p) {
        dprintf("%s : Failed to free <%p>\n", __func__, _p);
        assert(0);
    }
#if defined(BOOT)
    if (m_exist(p)) {
        m_free(p);
        return;
    }
#endif
    p = p - 1;
    if (p->magic != MALLOC_MAGIC) {
        fatal_error("__heap_free : magic fail, expected= 0x%08x, token= 0x%08x\n",
                    MALLOC_MAGIC, p->magic);
    }
    if (!p->freeable) {
        dbg_eval(DBG_WARN) {
            dprintf("%s() : static memory : <0x%p>\n", __func__, _p);
        }
        return;
    }
    heap_size_total += p->size;
    free(p);
}

#if HEAP_TRACE
static inline void *
__heap_realloc (void *x, size_t size, const char *func)
#else
static inline void *
__heap_realloc (void *x, size_t size)
#endif
{
    mchunk_t *p = (mchunk_t *)x;
    if (!p) {
        return NULL;
    }
    p = p - 1;
    if (p->size >= size) {
        return x;
    }
    if (heap_size_total < size) {
        fatal_error("%s() : size= %d, avail= %d\n",
            __func__, size, heap_size_total);
    }
    assert(p->freeable);
#if HEAP_TRACE
    __heap_free(x, func);
    return __heap_malloc(size, 1, func);
#else
    __heap_free(x);
    return __heap_malloc(size, 1);
#endif
}

void heap_dump (void)
{
    arch_word_t heap_mem, heap_size, heap_size_left;

    dprintf("%s() :\n", __func__);
    __arch_get_heap(&heap_mem, &heap_size);

    heap_size_left = heap_size - 32 * 2 - heap_size_total;
    assert(heap_size_left <= heap_size);
    if (heap_size_left) {
        dprintf("%s() : Unfreed left : %u bytes\n", __func__, heap_size_left);
    }
}

void heap_init (void)
{
    arch_word_t heap_mem, heap_size;
    arch_word_t sp_mem, sp_size;

    arch_get_stack(&sp_mem, &sp_size);
    arch_get_heap(&heap_mem, &heap_size);

    dprintf("Memory :\n");
    dprintf("stack : <0x%p> + %u bytes\n", (void *)sp_mem, sp_size);
    dprintf("heap : <0x%p> + %u bytes\n", (void *)heap_mem, heap_size);
    heap_size_total = heap_size - 32 * 2;
#ifdef BOOT
extern void m_init (void *pool, uint32_t size);

    arch_get_usr_heap(&heap_user_mem_ptr, &heap_user_size);
    dprintf("user heap : <0x%p> + %u bytes\n", (void *)heap_user_mem_ptr, heap_user_size);
    m_init(heap_user_mem_ptr, heap_user_size);
#endif /*BOOT*/
}

void heap_deinit (void)
{
    heap_dump();
}

#ifdef BOOT

#if HEAP_TRACE

void *_heap_alloc_shared (size_t size, const char *func)
{
extern void *m_malloc (uint32_t size);
    void *p = m_malloc(size);
    if (NULL == p) {
        heap_dbg("[%s] : Failed to allocate [%u] bytes\n", func, size);
    }
    return p;
}

#else

void *heap_alloc_shared (size_t size)
{
extern void *m_malloc (uint32_t size);
    return m_malloc(size);
}

#endif

#else /*BOOT*/

#if HEAP_TRACE
void *_heap_alloc_shared (size_t size, const char *func)
{
    __heap_check_margin(size);
    return __heap_malloc(size, 1, func);
}
#else
void *heap_alloc_shared (size_t size)
{
    __heap_check_margin(size);
    return __heap_malloc(size, 1);
}
#endif /*HEAP_TRACE*/

#endif /*BOOT*/

size_t heap_avail (void)
{
    return (heap_size_total - sizeof(mchunk_t));
}

#if HEAP_TRACE

void *_heap_malloc (size_t  size, const char *func)
{
    return __heap_malloc(size, 1, func);
}

void *_heap_realloc (void *x, size_t size, const char *func)
{
    return __heap_realloc(x, size, func);
}

void *_heap_calloc (size_t size, const char *func)
{
    void *p = __heap_malloc(size, 1, func);
    if (p) {
        d_memzero(p, size);
    }
    return p;
}

void _heap_free (void *p, const char *func)
{
    __heap_free(p, func);
}

#else /*HEAP_TRACE*/

void *heap_malloc (size_t  size)
{
    return __heap_malloc(size, 1);
}

void *heap_realloc (void *x, size_t size)
{
    return __heap_realloc(x, size);
}

void *heap_calloc (size_t size)
{
    void *p = __heap_malloc(size, 1);
    if (p) {
        d_memzero(p, size);
    }
    return p;
}

void heap_free (void *p)
{
    __heap_free(p);
}

#endif /*HEAP_TRACE*/
