#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <heap.h>
#include <debug.h>

#define MGAP (16)
#define MMINSZ (MGAP + sizeof(mchunk_t))
#define MMAGIC (0x7A)
#define MPOISON_USE_AFTER_FREE (0x6b)
#define MALLIGN (sizeof(arch_word_t))

struct mpool_s;
struct mlist_s;

typedef struct mchunk_s {
    struct mchunk_s *next;
    struct mchunk_s *next_frag;
    struct mlist_s *mlist;
    size_t size;
    uint8_t data[];
} mchunk_t;

typedef struct mlist_s {
    mchunk_t *head, *tail;
    uint32_t size, cnt;
} mlist_t;

typedef struct mpool_s {
    struct mpool_s *next;
    mlist_t freelist, usedlist;
    void *start; void *end;
    uint8_t pool_id;
    uint8_t data[];
} mpool_t;

typedef struct {
    mpool_t *arena;
    int last_id;
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
    d_memset(&mchunk->data[mchunk->size - MGAP - sizeof(mchunk_t)], MMAGIC, MGAP);
}

static int mpool_check_magic (mchunk_t *mchunk)
{
    uint8_t magic[MGAP];

    d_memset(magic, MMAGIC, MGAP);
    return !d_memcmp(&mchunk->data[mchunk->size - MGAP - sizeof(mchunk_t)], magic, MGAP);
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

    while (head) {

        if (head == mchunk) {
            if (prev) {
                prev->next = mchunk->next;
            } else {
                mlist->head = mchunk->next;
            }
            if (mlist->tail == mchunk) {
                mlist->tail = prev;
            }
            break;
        }

        prev = head;
        head = head->next;
    }
    assert(mchunk);
    mlist->size -= mchunk->size;
    mlist->cnt--;
}

static mchunk_t *__mchunk_new_fragment (mchunk_t *chunk, size_t size)
{
    size_t newsize = chunk->size - size;
    mchunk_t *newchunk = (mchunk_t *)((uint8_t *)chunk + newsize);

    chunk->size = newsize;
    chunk->mlist->size -= size;
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
    while (head) {
        if (head->size >= size) {
            break;
        }
        head = head->next;
    }
    return head;
}

static void *mpool_alloc (mpool_t *mpool, size_t size)
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
        chunk = __mchunk_new_fragment(chunk, memsize);
    } else {
        __mchunk_unlink(&mpool->freelist, chunk);
    }
    __mchunk_link(&mpool->usedlist, chunk);

    mpool_set_magic(chunk);
    return chunk->data;
}

static void mpool_free (mpool_t *mpool,  mchunk_t *mchunk)
{
    assert(mpool_check_magic(mchunk));
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

    return mpool;
}

void m_init (void)
{
    dmem.last_id = 0;
    dmem.arena = NULL;
}

void *m_malloc (void *mpool, uint32_t size)
{
    return mpool_alloc(mpool, size);
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

