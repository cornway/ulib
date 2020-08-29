#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <heap.h>
#include <debug.h>
#include <term.h>
#include <bsp_sys.h>

#define MGAP (16)
#define MMINSZ (MGAP + sizeof(mchunk_t))
#define MMAGIC (0x7A)
#define MPROTECT (MMAGIC ^ 0xff)
#define MPOISON_USE_AFTER_FREE (0x6b)
#define MALLIGN (sizeof(arch_word_t))
#define MPROTECT_SIZE (MALLIGN)

struct mpool_s;
struct mlist_s;

typedef struct mchunk_s {
    uint8_t protect[MPROTECT_SIZE];
    struct mchunk_s *next;
    struct mchunk_s *next_frag;
    struct mlist_s *mlist;
    const char *name;
    size_t size;
    uint8_t *data;
} mchunk_t;

typedef struct mlist_s {
    mchunk_t *head, *tail;
    uint32_t size, cnt;
} mlist_t;

typedef struct mpool_s {
    struct mpool_s *next;
    mlist_t freelist, usedlist;
    mchunk_t *frag_start;
    void *start; void *end;
    uint8_t *magic;
    size_t frag_size;
    size_t size;
    uint8_t pool_id;
} mpool_t;

typedef struct {
    mpool_t *arena;
    int last_id;
    uint8_t magic[MPROTECT_SIZE];
} dmem_t;

A_COMPILE_TIME_ASSERT(memory_allign, (sizeof(mchunk_t) % MALLIGN) == 0);

static dmem_t dmem = {0};

#ifdef DMEM_POISON
static void __mpoison (void *dst, size_t size)
{
    d_memset(dst, MPOISON_USE_AFTER_FREE, size);
}
#else
#define __mpoison(dst, size)
#endif /* DMEM_POISON */


static void mpool_set_magic (mchunk_t *mchunk)
{
    d_memset(&mchunk->protect[0], MPROTECT, arrlen(mchunk->protect));
}

int d_memcmp (uint8_t *p1, uint8_t *p2, size_t size);

static int mpool_check_magic (mpool_t *mpool, mchunk_t *mchunk)
{
    uint8_t *magic = mpool->magic;
    return d_memcmp(&mchunk->protect[0], magic, arrlen(mchunk->protect));
}

size_t mchunk_size (void *_mchunk)
{
    mchunk_t *mchunk = (mchunk_t *)_mchunk - 1;
    return mchunk->size;
}

static mchunk_t *mchunk_init (mchunk_t *mchunk, size_t size)
{
    mchunk->data = NULL;
    mchunk->mlist = NULL;
    mchunk->name = NULL;
    mchunk->next = NULL;
    mchunk->next_frag = NULL;
    mchunk->size = size;
    return mchunk;
}

static void __mchunk_link (mlist_t *mlist, mchunk_t *mchunk)
{
    mchunk->mlist = mlist;
    mchunk->next = mlist->head;
    if (mlist->head == NULL) {
        mlist->tail = mchunk;
    }
    mlist->head = mchunk;
    mlist->size += mchunk->size;
    mlist->cnt++;
}

static void __mchunk_unlink (mlist_t *mlist, mchunk_t *mchunk)
{
    mchunk_t *head = mlist->head;
    mchunk_t *prev = NULL;

    while (head && (head != mchunk)) {
        prev = head;
        head = head->next;
    }
    if (prev) {
        prev->next = mchunk->next;
    } else {
        mlist->head = mchunk->next;
    }
    if (mlist->tail == mchunk) {
        mlist->tail = prev;
    }
    mlist->size -= mchunk->size;
    mlist->cnt--;
}

static inline mchunk_t *__mchunk_alloc_ptr (mchunk_t *chunk, size_t off)
{
    return (mchunk_t *)((uint8_t *)chunk + off);
}

static inline void mchunk_add_frag (mpool_t *mpool, mchunk_t *chunk, mchunk_t *frag)
{
    frag->next_frag = chunk->next_frag;
    chunk->next_frag = frag;
    if (!mpool->frag_start) {
        mpool->frag_start = chunk;
    }
}

static mchunk_t *mchunk_fragment_top (mpool_t *mpool, mchunk_t *chunk, size_t new_frag_size)
{
    chunk->size = chunk->size - new_frag_size;
    mchunk_t *newchunk = __mchunk_alloc_ptr(chunk, chunk->size);

    if (chunk->mlist) {
        chunk->mlist->size -= new_frag_size;
    }

    newchunk = mchunk_init(newchunk, new_frag_size);

    mchunk_add_frag(mpool, chunk, newchunk);

    return newchunk;
}

static int mchunk_frag_allowed (mpool_t *mpool, mchunk_t *chunk, size_t size)
{
    assert(size <= chunk->size);
    if (mpool->frag_size <= MMINSZ) {
        if ((size + MMINSZ) <= chunk->size) {
            return 1;
        } else {
            return 0;
        }
    }

    if (size + MMINSZ < mpool->frag_size) {
        return 0;
    }
    size = chunk->size - size;
    if (size + MMINSZ < mpool->frag_size) {
        return 0;
    }
    return 1;
}

static inline void mchunk_concat (mchunk_t *frag, mchunk_t *next_frag)
{
    frag->size = frag->size + next_frag->size;
    frag->next_frag = next_frag->next_frag;
}

static inline int mchunk_concat_allowed (mpool_t *mpool, mchunk_t *frag, mchunk_t *next_frag)
{
    assert(next_frag == (mchunk_t *)((uint8_t *)frag + frag->size));
    if (&mpool->freelist == frag->mlist &&
        frag->mlist == next_frag->mlist) {
        return 1;
    }
    return 0;
}

static void mlist_defrag_all (mpool_t *mpool)
{
    mchunk_t *frag = mpool->frag_start, *next_frag = mpool->frag_start->next_frag, *tmp_frag;

    while (next_frag) {

        tmp_frag = next_frag->next_frag;

        if (mchunk_concat_allowed(mpool, frag, next_frag)) {
            __mchunk_unlink(&mpool->freelist, next_frag);
            mpool->freelist.size += next_frag->size;
            mchunk_concat(frag, next_frag);
        } else {
            frag = next_frag;
        }

        next_frag = tmp_frag;
    }
}

static inline mchunk_t *mpool_best_fit (mchunk_t *head, size_t size)
{
    size_t best_dist = (size_t)-1;
    mchunk_t *chunk = NULL;
    for (; head; head = head->next) {
        if (head->size >= size) {
            if ((head->size - size) < best_dist) {
                best_dist = head->size - size;
                chunk = head;
            }
        }
    }
    return chunk;
}

mchunk_t *mpool_try_realloc (mpool_t *mpool, mchunk_t *mchunk, size_t size)
{
    mchunk_t *oldchunk = mchunk;

    assert(mchunk->size >= size);
    if (mchunk->size <= size + MMINSZ) {
        return mchunk;
    }
    mchunk = mchunk_fragment_top(mpool, mchunk, mchunk->size - size);
    __mchunk_link(&mpool->freelist, mchunk);
    mlist_defrag_all(mpool);
    return oldchunk;
}

static void *mpool_alloc (mpool_t *mpool, size_t size, const char *caller_name)
{
    const size_t memsize = ROUND_UP((size + sizeof(mchunk_t) + MGAP), MALLIGN);
    mchunk_t *chunk;

    if (mpool->freelist.size < memsize) {
        return NULL;
    }

    chunk = mpool_best_fit(mpool->freelist.head, memsize);
    if (!chunk) {
        return NULL;
    }
    if (mchunk_frag_allowed(mpool, chunk, memsize)) {
        __mchunk_link(&mpool->freelist, mchunk_fragment_top(mpool, chunk, chunk->size - memsize));
    }
    __mchunk_unlink(&mpool->freelist, chunk);
    __mchunk_link(&mpool->usedlist, chunk);

    chunk->data = (uint8_t *)(chunk + 1);
    mpool_set_magic(chunk);
    chunk->name = caller_name;
    return chunk->data;
}

static void *mpool_alloc_align (mpool_t *mpool, size_t size, size_t align)
{
    const size_t memsize = ROUND_UP((size + sizeof(mchunk_t) + MGAP + align), MALLIGN);
    size_t offset = 0, pad;
    mchunk_t *chunk, *chunk_before, *chunk_after;

    if (mpool->freelist.size < memsize) {
        return NULL;
    }

    chunk_before = mpool_best_fit(mpool->freelist.head, memsize);
    if (!chunk_before) {
        return NULL;
    }
    if (chunk_before->size >= MMINSZ + memsize) {
        if (!mpool->frag_start) {
            mpool->frag_start = chunk_before;
        }
        chunk_before = mchunk_fragment_top(mpool, chunk_before, memsize);
    } else {
        __mchunk_unlink(&mpool->freelist, chunk_before);
    }

    pad = GET_PAD((arch_word_t)chunk_before, align);
    while (pad < sizeof(mchunk_t)) {
        pad += align;
    }
    offset = pad - sizeof(mchunk_t);

    chunk = mchunk_fragment_top(mpool, chunk_before, memsize - offset);
    chunk_after = mchunk_fragment_top(mpool, chunk, size);

    __mchunk_link(&mpool->freelist, chunk_before);
    __mchunk_link(&mpool->freelist, chunk_after);
    __mchunk_link(&mpool->usedlist, chunk);

    chunk->data = (uint8_t *)(chunk + 1);
    mpool_set_magic(chunk);
    return chunk->data;
}


static void mpool_free (mpool_t *mpool,  mchunk_t *mchunk)
{
    int check = mpool_check_magic(mpool, mchunk);
    if (check) {
        dprintf("%s(): Corrupted memory, err %d, Dump:\n", __func__, check);
        dprintf("red zone: \n");
        hexdump(mchunk->protect, 8, MPROTECT_SIZE, 4);
        assert(0);
    }
    __mpoison(mchunk->data, mchunk->size - sizeof(mchunk_t));

    __mchunk_unlink(&mpool->usedlist, mchunk);
    __mchunk_link(&mpool->freelist, mchunk);

    mlist_defrag_all(mpool);
}

static void mpool_init (mpool_t *mpool, void *pool, uint32_t poolsize)
{
    mchunk_t *chunk = (mchunk_t *)pool;

    d_memzero(mpool, sizeof(*mpool));
    mchunk_init(chunk, poolsize);
    __mchunk_link(&mpool->freelist, chunk);
    mpool->start = pool;
    mpool->end = (void *)((size_t)pool + poolsize);
    mpool->size = poolsize;
}

size_t mlist_size (mlist_t *mlist)
{
    size_t size = 0;
    mchunk_t *head = mlist->head;
    while (head) {

        size += head->size;
        head = head->next;
    }
    return size;
}

size_t mfrag_size (mpool_t *mpool)
{
    size_t size = 0;
    mchunk_t *head = mpool->frag_start;
    while (head) {

        size += head->size;
        head = head->next_frag;
    }
    return size;
}

static int mpool_verify (mpool_t *mpool)
{
    int err = 0;
    size_t size = mpool->size - (mpool->freelist.size + mpool->usedlist.size);

    if (size) {
        err--;
        goto bad;
    }
    size = mlist_size(&mpool->freelist);
    if (size != mpool->freelist.size) {
        err--;
        goto bad;
    }
    size = mlist_size(&mpool->usedlist);
    if (size != mpool->usedlist.size) {
        err--;
        goto bad;
    }
    size = mfrag_size(mpool);
    if (size != mpool->size) {
        err--;
        goto bad;
    }
    return 0;
bad:
    dprintf("%s(): Fail, memory lost or corrupted, err %d\n", __func__, err);
    assert(0);
    return -1;
}

/* public API */

void *_m_pool_init (void *pool, size_t size, int link, size_t frag_size)
{
    mpool_t *mpool = (mpool_t *)pool;

    mpool_init(mpool, mpool + 1, size - sizeof(mpool_t));
    mpool->pool_id = dmem.last_id;

    if (link) {
        dmem.last_id++;
        mpool->next = dmem.arena;
        dmem.arena = mpool;
    }
    mpool->magic = dmem.magic;
    mpool->frag_size = frag_size;
    return mpool;
}

void *m_pool_init (void *pool, size_t size, int link)
{
    return _m_pool_init(pool, size, link, 0);
}

void *m_pool_init_ext (void *pool, size_t size, int link, size_t frag_size)
{
    return _m_pool_init(pool, size, link, frag_size);
}

void m_init (void)
{
    dmem.last_id = 0;
    dmem.arena = NULL;

    d_memset(dmem.magic, MPROTECT, MPROTECT_SIZE);
}

void *m_malloc (void *pool, uint32_t size, const char *caller_name)
{
    mpool_t *mpool = (mpool_t *)pool;
    return mpool_alloc(mpool, size, caller_name);
}

void *m_malloc_align (void *pool, uint32_t size, uint32_t align)
{
    mpool_t *mpool = (mpool_t *)pool;
    return mpool_alloc_align(mpool, size, align);
}

void m_free (void *p)
{
    mchunk_t *mchunk = (mchunk_t *)p;
    mpool_t *mpool;

    mchunk = mchunk - 1;
    mpool = container_of(mchunk->mlist, mpool_t, usedlist);

    if (mchunk->mlist != &mpool->usedlist) {
        assert(0);
    }
    mpool_free(mpool, mchunk);
}

void *m_realloc (void *ptr, size_t size, const char *caller_func)
{
    mchunk_t *mchunk = (mchunk_t *)ptr - 1;
    mpool_t *mpool = container_of(mchunk->mlist, mpool_t, usedlist);

    size = ROUND_UP(size, MALLIGN);

    if (mpool_try_realloc(mpool, mchunk, size)) {
        return ptr;
    }
    dprintf("%s() from %s(): Failed to realloc at 0x%p %u bytes\n",
        __func__, caller_func, ptr, size);
    return ptr;
}

size_t m_avail (void *pool)
{
    mpool_t *mpool = (mpool_t *)pool;
    return mpool->freelist.size;
}

int m_verify (void *pool)
{
    mpool_t *mpool = (mpool_t *)pool;
    return mpool_verify(mpool);
}

void m_stat (void)
{
    mpool_t *pool = dmem.arena;

    dprintf("%s() +\n", __func__);

    while (pool) {
        dprintf("%p; id=%d, size=%u bytes, used=%u bytes\n",
            pool, pool->pool_id, pool->size, pool->usedlist.size);
        pool = pool->next;
    }

    dprintf("%s() -\n", __func__);
}

void mpool_frag_stat (void *_mpool)
{
    mpool_t *mpool = (mpool_t *)_mpool;
    mchunk_t *frag = mpool->frag_start;

    dprintf("%s(): id=%d, cnt=%d------------\n",
        __func__, mpool->pool_id, mpool->freelist.cnt + mpool->usedlist.cnt);
    while (frag) {
        if (frag->mlist == &mpool->freelist) {
            dprintf("***");
        }
        dprintf("0x%p : 0x%08x;\n", frag, frag->size);
        frag = frag->next_frag;
    }
    dprintf("------------\n");
}

