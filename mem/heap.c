#include <arch.h>
#include <stdlib.h>
#include <mpu.h>
#include <misc_utils.h>
#include <debug.h>
#include <bsp_sys.h>

#define MALLOC_MAGIC       0x75738910

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

static inline void *
__heap_malloc (size_t      size, int freeable)
{
    mchunk_t *p;

    size = size + sizeof(mchunk_t);
    size = ROUND_UP(size, sizeof(arch_word_t));
    p = (mchunk_t *)malloc(size);
    if (!p) {
        return NULL;
    }
    heap_size_total -= size;
    p->magic = MALLOC_MAGIC;
    p->freeable = freeable;
    p->size = size;
    return (void *)(p + 1);

}

static inline void
__heap_free (void *_p)
{
extern void m_free (void *);
extern void *m_exist (void *);
    mchunk_t *p = (mchunk_t *)_p;
    assert(_p);
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

static inline void *
__heap_realloc (void *x, size_t size)
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
    __heap_free(x);
    return __heap_malloc(size, 1);
}

void heap_dump (void)
{
    arch_word_t heap_mem, heap_size, heap_size_left;

    dprintf("%s() :\n", __func__);
    arch_get_heap(&heap_mem, &heap_size);

    heap_size_left = heap_size - MPU_CACHELINE * 2 - heap_size_total;
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
    heap_size_total = heap_size - MPU_CACHELINE * 2;
#ifdef BOOT
extern void m_init (void *pool, uint32_t size);
extern void __arch_user_heap (void *mem, void *size);

    __arch_user_heap(&heap_user_mem_ptr, &heap_user_size);
    dprintf("user heap : <0x%p> + %u bytes\n", (void *)heap_user_mem_ptr, heap_user_size);
    m_init(heap_user_mem_ptr, heap_user_size);
#endif /*BOOT*/
}

void heap_deinit (void)
{
    heap_dump();
}

#ifdef BOOT

void *heap_alloc_shared (size_t size)
{
extern void *m_malloc (uint32_t size);
    return m_malloc(size);
}

#else /*BOOT*/

void *heap_alloc_shared (size_t size)
{
    __heap_check_margin(size);
    return __heap_malloc(size, 1);
}

#endif /*BOOT*/

int heap_avail (void)
{
    return (heap_size_total - sizeof(mchunk_t));
}

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

