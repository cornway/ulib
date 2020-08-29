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

void m_init (void);
void *m_pool_init (void *pool, size_t size, int link);
void *m_pool_init_ext (void *pool, size_t size, int link, size_t frag_size);
void *m_malloc (void *pool, size_t size, const char *caller_name);
void *m_malloc_align (void *pool, uint32_t size, uint32_t align);
void m_free (void *p);
size_t m_avail (void *pool);
void m_stat (void);
size_t mchunk_size (void *_mchunk);
void *m_realloc (void *ptr, size_t size, const char *caller_func);
void mpool_frag_stat (void *_mpool);

typedef struct heap_pool_desc_s {
    void *mpool;
    size_t size, chunk_size, chunk_min_size;
} heap_pool_desc_t;

static heap_pool_desc_t heap_pool_desc[8] = {0};

static void *heap_shared_pool, *dma_pool;

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
    int i;
    void *p = NULL;
    size = ROUND_UP(size, sizeof(arch_word_t));

    for (i = 0; !p && i < arrlen(heap_pool_desc) && heap_pool_desc[i].mpool; i++) {
        if (size <= heap_pool_desc[i].chunk_size && size >= heap_pool_desc[i].chunk_min_size) {
            p = m_malloc(heap_pool_desc[i].mpool, size, caller_func);
        }
    }
    if (!p) {
        dprintf("[%s] Failed to allocate [%u] bytes\n", caller_func, size);
        heap_stat();
        for (i = 0; !p && i < arrlen(heap_pool_desc); i++) {
            dprintf("Mpool total size=%u, size=%u, min size=%u\n",
                heap_pool_desc[i].size, heap_pool_desc[i].chunk_size, heap_pool_desc[i].chunk_min_size);
            mpool_frag_stat(heap_pool_desc[i].mpool);
        }
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
    return m_realloc(x, size, caller_func);
}

void heap_dump (void)
{
    m_stat();
}

void heap_stat (void)
{
    arch_word_t heap_mem, heap_size;
    arch_word_t sp_mem, sp_size;

    arch_get_stack(&sp_mem, &sp_size);
    arch_get_heap(&heap_mem, &heap_size);

#ifdef BOOT
    dprintf("Driver Memory");
#else
    dprintf("User Memory");
#endif
    dprintf("===============\n");
    dprintf("stack : <0x%p> + %u bytes\n", (void *)sp_mem, sp_size);
    dprintf("heap : <0x%p> + %u bytes\n", (void *)heap_mem, heap_size);
#ifdef BOOT
    arch_get_usr_heap(&heap_mem, &heap_size);
    dprintf("user heap : <0x%p> + %u bytes\n", (void *)heap_mem, heap_size);
#endif
    m_stat();
    dprintf("===============\n");
}

#ifdef BOOT

void heap_init (void)
{
    arch_word_t heap_mem, heap_size;
    size_t dma_pool_size = 0x10000;

    m_init();

    arch_get_heap(&heap_mem, &heap_size);
    assert(heap_size > dma_pool_size);

    heap_size -= dma_pool_size;

    heap_pool_desc[0].chunk_size = heap_size;
    heap_pool_desc[0].size = heap_size;
    heap_pool_desc[0].chunk_min_size = 0;

    heap_pool_desc[0].mpool = m_pool_init((void *)heap_mem, heap_size, 1);
    heap_mem += heap_size;
    dma_pool = m_pool_init((void *)heap_mem, dma_pool_size, 1);
    if (mpu_lock(heap_mem, &dma_pool_size, "c") < 0) {
        assert(0);
    }
    heap_shared_pool = NULL;
    if (EXEC_REGION_DRIVER()) {
        arch_get_usr_heap(&heap_mem, &heap_size);
        heap_shared_pool = m_pool_init((void *)heap_mem, heap_size, 1);
    }
}

#else /* BOOT */

static void _heap_init (heap_conf_t *conf)
{
    int i = 0;
    arch_word_t heap_mem, heap_size;

    m_init();

    arch_get_heap(&heap_mem, &heap_size);

    d_memzero(&heap_pool_desc, sizeof(heap_pool_desc));

    if (conf) {
        while (conf->total_size && i < arrlen(heap_pool_desc)) {
            heap_pool_desc[i].size = conf->total_size;
            heap_pool_desc[i].chunk_size = conf->max_frag_size;
            heap_pool_desc[i].chunk_min_size = conf->min_frag_size;
            i++;
            conf++;
        }
        assert(i < arrlen(heap_pool_desc));
    } else {
        heap_pool_desc[0].chunk_size = (size_t)-1;
        heap_pool_desc[0].size = (size_t)-1;
        heap_pool_desc[0].chunk_min_size = 0;
    }

    for (i = 0; i < arrlen(heap_pool_desc) && heap_pool_desc[i].size && heap_size; i++) {
        if (heap_pool_desc[i].size > heap_size) {
            heap_pool_desc[i].size = heap_size;
        }
        heap_pool_desc[i].mpool = m_pool_init_ext((void *)heap_mem, heap_pool_desc[i].size, 1, heap_pool_desc[i].chunk_min_size);
        heap_mem += heap_pool_desc[i].size;
        heap_size -= heap_pool_desc[i].size;
    }
    assert(!heap_size);
    i = i ? i - 1 : 0;
    heap_shared_pool = heap_pool_desc[i].mpool;
    dma_pool = NULL;
}

void heap_init_ext (const heap_conf_t *conf)
{
    _heap_init(conf);
}

void heap_init (void)
{
    _heap_init(NULL);
}


#endif /* BOOT */

void heap_deinit (void)
{
    heap_dump();
}

#ifdef BOOT

#if HEAP_TRACE
void *_heap_alloc_shared (size_t size, const char *caller_func)
{
    void *p = m_malloc(heap_shared_pool, size, caller_func);
    if (NULL == p) {
        heap_dbg("[%s] : Failed to allocate [%u] bytes\n", caller_func, size);
    }
    return p;
}

#else /* HEAP_TRACE */

void *heap_alloc_shared (size_t size)
{
    return m_malloc(heap_shared_pool, size, "NULL");
}

#endif /* HEAP_TRACE */

#else /*BOOT*/

#if HEAP_TRACE
void *_heap_alloc_shared (size_t size, const char *caller_func)
{
    return __heap_malloc(size, caller_func);
}
#else /* HEAP_TRACE */

void *heap_alloc_shared (size_t size)
{
    return m_malloc(heap_shared_pool, size, "NULL");
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
    return __heap_malloc(size, caller_func);
}

void *_heap_realloc (void *x, size_t size, const char *caller_func)
{
    return __heap_realloc(x, size, caller_func);
}

void *_heap_calloc (size_t size, const char *caller_func)
{
    void *p = __heap_malloc(size, caller_func);
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
    return m_malloc(dma_pool, size, NULL);
}

void dma_free (void *p)
{
    heap_free(p);
}

void *heap_alloc_shared_align (size_t size, size_t align)
{
    return m_malloc_align(heap_shared_pool, size, align);
}

