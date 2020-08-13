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
#define MMAGIC_SIZE (MGAP)

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
    size_t size;
    uint8_t pool_id;
} mpool_t;

typedef struct {
    mpool_t *arena;
    int last_id;
    uint8_t magic[MMAGIC_SIZE + MPROTECT_SIZE];
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
    d_memset(&mchunk->data[mchunk->size - MMAGIC_SIZE - sizeof(mchunk_t)], MMAGIC, MMAGIC_SIZE);
}

int d_memcmp (uint8_t *p1, uint8_t *p2, size_t size);

static int mpool_check_magic (mpool_t *mpool, mchunk_t *mchunk)
{
    uint8_t *magic = mpool->magic;
    int ret;

    ret = d_memcmp(&mchunk->data[mchunk->size - MMAGIC_SIZE - sizeof(mchunk_t)], magic, MMAGIC_SIZE);
    ret += d_memcmp(&mchunk->protect[0], magic + MMAGIC_SIZE, MPROTECT_SIZE);

    return ret;
}

static mchunk_t *mchunk_init (mchunk_t *mchunk, size_t size)
{
    d_memset(mchunk, 0, sizeof(mchunk_t));
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

static mchunk_t *__mchunk_new_fragment (mchunk_t *chunk, size_t size)
{
    const size_t newsize = chunk->size - size;
    mchunk_t *newchunk = __mchunk_alloc_ptr(chunk, newsize);

    chunk->size = newsize;
    chunk->mlist->size -= size;

    newchunk->next_frag = chunk->next_frag;
    chunk->next_frag = newchunk;

    return mchunk_init(newchunk, size);
}

static inline void mchunk_concat (mchunk_t *chunk, mchunk_t *next_chunk)
{
    chunk->size = chunk->size + next_chunk->size;
    chunk->next_frag = next_chunk->next_frag;
    next_chunk->next_frag = NULL;
}

static mchunk_t *mlist_defrag (mlist_t *mlist, mchunk_t *chunk)
{
    mchunk_t *frag = chunk->next_frag, *next_frag;

    while (frag && (frag->mlist == mlist)) {

        next_frag = frag->next_frag;

        mchunk_concat(chunk, frag);
        mlist->size += frag->size;
        __mchunk_unlink(mlist, frag);

        frag = next_frag;
    }
    return chunk->next;
}

static void mlist_defrag_all (mlist_t *mlist)
{
    mchunk_t *chunk = mlist->head;

    while (mlist->cnt && chunk) {
        chunk = mlist_defrag(mlist, chunk);
    }
}

static inline mchunk_t *mpool_best_fit (mchunk_t *head, size_t size)
{
    for (; head && head->size < size; head = head->next) {}
    return head;
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
    if (chunk->size >= MMINSZ + memsize) {
        if (!mpool->frag_start) {
            mpool->frag_start = chunk;
        }
        chunk = __mchunk_new_fragment(chunk, memsize);
    } else {
        __mchunk_unlink(&mpool->freelist, chunk);
    }
    __mchunk_link(&mpool->usedlist, chunk);

    chunk->data = (uint8_t *)(chunk + 1);
    mpool_set_magic(chunk);
    chunk->name = caller_name;
    return chunk->data;
}

static void *mpool_alloc_align (mpool_t *mpool, size_t size, size_t align)
{
    const size_t memsize = ROUND_UP((size + sizeof(mchunk_t) + MGAP), MALLIGN) + align;
    size_t offset = 0, pad;
    mchunk_t *chunk, *chunk_aligned;

    if (mpool->freelist.size < memsize) {
        return NULL;
    }

    chunk = mpool_best_fit(mpool->freelist.head, memsize);
    if (!chunk) {
        return NULL;
    }
    if (chunk->size >= MMINSZ + memsize) {
        if (!mpool->frag_start) {
            mpool->frag_start = chunk;
        }
        chunk = __mchunk_new_fragment(chunk, memsize);
    } else {
        __mchunk_unlink(&mpool->freelist, chunk);
    }

    pad = GET_PAD((arch_word_t)chunk, align);
    while (pad < sizeof(mchunk_t)) {
        pad += align;
    }
    offset = pad - sizeof(mchunk_t) + sizeof(arch_word_t);

    chunk_aligned = __mchunk_new_fragment(chunk, offset);
    __mchunk_link(&mpool->freelist, chunk);
    chunk = __mchunk_new_fragment(chunk_aligned, size);
    __mchunk_link(&mpool->freelist, chunk);
    __mchunk_link(&mpool->usedlist, chunk_aligned);

    mpool_set_magic(chunk);
    chunk_aligned->data = (uint8_t *)(chunk_aligned + 1);
    return chunk_aligned->data;
}


static void mpool_free (mpool_t *mpool,  mchunk_t *mchunk)
{
    int check = mpool_check_magic(mpool, mchunk);
    if (check) {
        dprintf("%s(): Corrupted memory, err %d, Dump:\n", __func__, check);
        dprintf("bottom: \n");
        hexdump(mchunk->protect, 8, MPROTECT_SIZE, 4);
        dprintf("top: \n");
        hexdump(&mchunk->data[mchunk->size - sizeof(mchunk_t) - MMAGIC_SIZE], 8, MMAGIC_SIZE, 4);
        dprintf("***\n");
        assert(0);
    }
    __mpoison(mchunk->data, mchunk->size - sizeof(mchunk_t));

    __mchunk_unlink(&mpool->usedlist, mchunk);
    __mchunk_link(&mpool->freelist, mchunk);

    mlist_defrag_all(&mpool->freelist);
}

static void mpool_init (mpool_t *mpool, void *pool, uint32_t poolsize)
{
    mchunk_t *chunk = (mchunk_t *)pool;

    d_memzero(mpool, sizeof(*mpool));
    chunk->size = poolsize;
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

void *m_pool_init (void *pool, size_t size)
{
    mpool_t *mpool = (mpool_t *)pool;

    mpool_init(mpool, mpool + 1, size - sizeof(mpool_t));
    mpool->pool_id = dmem.last_id;

    dmem.last_id++;
    mpool->next = dmem.arena;
    dmem.arena = mpool;
    mpool->magic = dmem.magic;
    return mpool;
}

void m_init (void)
{
    dmem.last_id = 0;
    dmem.arena = NULL;

    d_memset(dmem.magic, MMAGIC, MMAGIC_SIZE);
    d_memset(dmem.magic + MMAGIC_SIZE, MPROTECT, MPROTECT_SIZE);
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
    mchunk_t *chunk;

    dprintf("%s() +\n", __func__);

    while (pool) {
        dprintf("%p; id=%d, size=%u bytes, used=%u bytes\n",
            pool, pool->pool_id, pool->size, pool->usedlist.size);
        chunk = pool->usedlist.head;
        while (chunk) {
            dprintf("%p : %s; size=%u\n", chunk, chunk->name ? chunk->name : "NULL", chunk->size);
            chunk = chunk->next;
        }
        pool = pool->next;
    }

    dprintf("%s() -\n", __func__);
}

