#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <debug.h>
#include <heap.h>
#include <bsp_sys.h>

#ifdef BOOT
#include "../../common/int/mpu.h"
#endif

void *m_pool_init (void *pool, size_t size);
void m_init (void);
void *m_malloc (void *pool, size_t size);
void *m_malloc_align (void *pool, uint32_t size, uint32_t align);
void m_free (void *p);
size_t m_avail (void *pool);

static void *heap_shared_pool;
static void *heap_pool;
static void *dma_pool;

#if HEAP_TRACE
#define heap_dbg(args...) \
    dprintf("[HEAP DEBUG] " args)
#else
#define caller_func __func__
#define heap_dbg(args...)
#endif /* HEAP_TRACE */

#if HEAP_TRACE
static inline void *
__heap_malloc (size_t size, const char *caller_func)
#else
static inline void *
__heap_malloc (size_t size)
#endif /* HEAP_TRACE */
{
    void *p;
    size = ROUND_UP(size, sizeof(arch_word_t));
    p = m_malloc(heap_pool, size);
    if (!p) {
        heap_dbg("[%s] Failed to allocate [%u] bytes\n", caller_func, size);
    }
    return p;
}

static inline void
#if HEAP_TRACE
__heap_free (void *p, const char *caller_func)
#else
__heap_free (void *p)
#endif
{
    if (!p) {
        heap_dbg("%s() : NULL\n", caller_func);
        return;
    }
    m_free(p);
}

#if HEAP_TRACE
static inline void *
__heap_realloc (void *x, size_t size, const char *caller_func)
#else
static inline void *
__heap_realloc (void *x, size_t size)
#endif
{
    if (m_avail(heap_pool) < size) {
        fatal_error("%s() : Failed; size= %d, avail= %d\n",
            __func__, size, m_avail(heap_pool));
    }
#if HEAP_TRACE
    __heap_free(x, caller_func);
    return __heap_malloc(size, caller_func);
#else
    __heap_free(x);
    return __heap_malloc(size);
#endif /* HEAP_TRACE */
}

void heap_dump (void)
{
    arch_word_t heap_mem, heap_size, heap_size_left;

    __arch_get_heap(&heap_mem, &heap_size);

    heap_size_left = heap_size - m_avail(heap_pool);
    assert(heap_size_left <= heap_size);
    if (heap_size_left) {
        dprintf("%s() : Unfreed left : %u bytes\n", __func__, heap_size_left);
    }
}

void heap_stat (void)
{
    arch_word_t heap_mem, heap_size;
    arch_word_t sp_mem, sp_size;

    arch_get_stack(&sp_mem, &sp_size);
    arch_get_heap(&heap_mem, &heap_size);

    dprintf("Memory :\n");
    dprintf("stack : <0x%p> + %u bytes\n", (void *)sp_mem, sp_size);
    dprintf("heap : <0x%p> + %u bytes\n", (void *)heap_mem, heap_size);
#ifdef BOOT
    arch_get_usr_heap(&heap_mem, &heap_size);
    dprintf("user heap : <0x%p> + %u bytes\n", (void *)heap_mem, heap_size);
#endif
}

void heap_init (void)
{
    arch_word_t heap_mem, heap_size;
    void *dma_pool_buf = NULL;
    size_t dma_pool_size = 0x8000;

    arch_get_heap(&heap_mem, &heap_size);

    m_init();
    heap_pool = m_pool_init((void *)heap_mem, heap_size);

#ifdef BOOT
    arch_get_usr_heap(&heap_mem, &heap_size);
    heap_shared_pool = m_pool_init((void *)heap_mem, heap_size);
    dma_pool_buf = m_malloc_align(heap_pool, dma_pool_size, dma_pool_size);
    assert(dma_pool_buf);
    dma_pool = m_pool_init(dma_pool_buf, dma_pool_size);
    if (mpu_lock((arch_word_t)dma_pool_buf, &dma_pool_size, "c") < 0) {
        assert(0);
    }
#else
    heap_shared_pool = heap_pool;
    dma_pool = NULL;
#endif /*BOOT*/
}

void heap_deinit (void)
{
    heap_dump();
}

#ifdef BOOT

#if HEAP_TRACE
void *_heap_alloc_shared (size_t size, const char *caller_func)
{
    void *p = m_malloc(heap_shared_pool, size);
    if (NULL == p) {
        heap_dbg("[%s] : Failed to allocate [%u] bytes\n", caller_func, size);
    }
    return p;
}

#else /* HEAP_TRACE */

void *heap_alloc_shared (size_t size)
{
    return m_malloc(heap_shared_pool, size);
}

#endif /* HEAP_TRACE */

#else /*BOOT*/

#if HEAP_TRACE
void *_heap_alloc_shared (size_t size, const char *caller_func)
{
    return __heap_malloc(size, 1, caller_func);
}
#else /* HEAP_TRACE */
void *_heap_alloc_shared (size_t size)
{
    return __heap_malloc(size, 1);
}

#endif /*HEAP_TRACE*/

#endif /*BOOT*/

size_t heap_avail (void)
{
    return m_avail(heap_shared_pool);
}

#if HEAP_TRACE

void *_heap_malloc (size_t  size, const char *caller_func)
{
    return __heap_malloc(size, 1, caller_func);
}

void *_heap_realloc (void *x, size_t size, const char *caller_func)
{
    return __heap_realloc(x, size, caller_func);
}

void *_heap_calloc (size_t size, const char *caller_func)
{
    void *p = __heap_malloc(size, 1, caller_func);
    if (p) {
        d_memzero(p, size);
    }
    return p;
}

void _heap_free (void *p, const char *caller_func)
{
    __heap_free(p, caller_func);
}

#else /*HEAP_TRACE*/

void *heap_malloc (size_t  size)
{
    return __heap_malloc(size);
}

void *heap_realloc (void *x, size_t size)
{
    return __heap_realloc(x, size);
}

void *heap_calloc (size_t size)
{
    void *p = __heap_malloc(size);
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

void *dma_alloc (size_t size)
{
    return m_malloc(dma_pool, size);
}

void dma_free (void *p)
{
    heap_free(p);
}

void *heap_alloc_shared_align (size_t size, size_t align)
{
    return m_malloc_align(heap_shared_pool, size, align);
}



