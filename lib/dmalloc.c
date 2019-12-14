
#include <misc_utils.h>
#include <debug.h>

#define MPOOL_TINY_TRESH (128)
#define MPOOL_TINY_SIZE (4096)
#define MGAP (16)

typedef struct mchunk_s {
    struct mchunk_s *next;
    uint32_t size;
    uint32_t pool_id;
} mchunk_t;

typedef struct {
    mchunk_t *head, *tail;
    uint32_t size, cnt;
} mlist_t;

typedef struct {
    mlist_t freelist, usedlist;
    void *start; void *end;
    uint8_t pool_id;
} mpool_t;

typedef struct {
    mpool_t pool;
    mpool_t tiny;
} mem_t;

static mpool_t mpool = {{0}}, mpool_tiny = {{0}};
static mpool_t *mpool_arena[] = {&mpool, &mpool_tiny};

static void __mchunk_link (mlist_t *mlist, mchunk_t *mchunk)
{
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
            mlist->size -= mchunk->size;
            mlist->cnt--;
            break;
        }

        prev = head;
        head = head->next;
    }
}

static void _mchunk_move (mpool_t *mpool, mchunk_t *chunk, int is_free)
{
    mlist_t *from, *to;

    if (is_free) {
        from = &mpool->usedlist;
        to = &mpool->freelist;
    } else {
        from = &mpool->freelist;
        to = &mpool->usedlist;
    }

    __mchunk_unlink(from, chunk);
    __mchunk_link(to, chunk);
}

static mchunk_t *mchunk_split (mlist_t *mlist, mchunk_t *chunk, uint32_t size)
{
    uint32_t oldsize = chunk->size - size;
    mchunk_t *newchunk = (mchunk_t *)((uint8_t *)chunk + oldsize);

    chunk->size = oldsize;
    newchunk->size = size;
    mlist->size -= size;
    return newchunk;
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

static void *mpool_alloc (mpool_t *mpool, uint32_t size)
{
    mlist_t *mlist = &mpool->freelist;
    mchunk_t *head = mlist->head, *newchunk;
    uint32_t memsize = size + sizeof(mchunk_t);

    if (mlist->size < memsize) {
        return NULL;
    }

    while (head) {
        if (head->size >= memsize + MGAP) {
            break;
        }
        head = head->next;
    }
    if (!head) {
        return NULL;
    }
    newchunk = mchunk_split(&mpool->freelist, head, memsize);
    __mchunk_link(&mpool->usedlist, newchunk);
    newchunk->pool_id = mpool->pool_id;
    return (void *)(newchunk + 1);
}

static mchunk_t *mlist_sanitize_once (mlist_t *mlist);

static void mpool_free (mpool_t *mpool, void *p)
{
    mchunk_t *mchunk = (mchunk_t *)p;

    mchunk = mchunk - 1;
    _mchunk_move(mpool, mchunk, 1);
    do {
        mchunk = mlist_sanitize_once(&mpool->freelist);
    } while (mchunk);
}

void *m_init (void *pool, uint32_t size)
{
    mem_t *mem = (mem_t *)pool;
    size -= sizeof(mem_t);
    mpool_init(&mem->pool, (void *)&mem[1], size);
    pool = mpool_alloc(&mem->pool, MPOOL_TINY_SIZE);
    if (!pool) {
        return NULL;
    }
    mpool_init(&mem->tiny, pool, MPOOL_TINY_SIZE);
    mem->pool.pool_id = 0;
    mem->tiny.pool_id = 1;
    return mem;
}

void *m_exist (void *_mem, void *p)
{
    mem_t *mem= _mem;
    if (!mem || p < mem->pool.start || p > mem->pool.end) {
        p = NULL;
    }
    return p;
}

void *m_malloc (void *_mem, uint32_t size)
{
    mem_t *mem = _mem;
    void *ptr = NULL;

    if (!mem) {
        return NULL;
    }
    size = ROUND_UP(size, sizeof(arch_word_t));

    if (size < MPOOL_TINY_TRESH) {
        ptr = mpool_alloc(&mem->tiny, size);
    }
    if (!ptr) {
        ptr = mpool_alloc(&mem->pool, size);
    }
    return ptr;
}

void m_free (void *_mem, void *p)
{
    mem_t *mem = _mem;
    mpool_t *pool = NULL;
    mchunk_t *mchunk = (mchunk_t *)p;

    if (!mem) {
        return;
    }
    mchunk = mchunk - 1;
    if (mchunk->pool_id == mem->pool.pool_id) {
        pool = &mem->pool;
    }
    if (!pool) {
        pool = &mem->tiny;
        return;
    }
    assert(pool);
    mpool_free(pool, p);
}

static void mlist_print (mlist_t *mlist, const char *name)
{
    int i;
    mchunk_t *mchunk;

    d_printf("%s pool : chunks= %u, bytes= %u\n", name, mlist->cnt, mlist->size);
    mchunk = mlist->head;

    while (mchunk) {
        d_printf("mchunk[%i]: <0x%p> of size [0x%08x]\n",
            i, mchunk, mchunk->size);
        mchunk = mchunk->next;
        i++;
    }
    d_printf("***\n");
}

static mchunk_t *mlist_sanitize_once (mlist_t *mlist)
{
    mchunk_t *head = mlist->head, *prev = NULL;

    while (head) {

        if (prev) {
            uint8_t *ptr1 = (uint8_t *)prev + prev->size;

            if (ptr1 == (uint8_t *)head) {
                prev->size += head->size;
                __mchunk_unlink(mlist, head);
                mlist->size += head->size;
                break;
            }
        }
        prev = head;
        head = head->next;
    }
    return head;
}

