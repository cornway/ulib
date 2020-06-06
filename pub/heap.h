#ifndef __HEAP_H__
#define __HEAP_H__

#ifdef __cplusplus
    extern "C" {
#endif

#define ALIGN(x) __attribute__((aligned(x)))

#define SDRAM __attribute__ ((section ("dram")))
#define DTCM __attribute__ ((section ("dtcm")))
#define IRAM __attribute__ ((section ("iram")))
#define IRAM2 __attribute__ ((section ("iram2")))
#if defined(HAVE_CODESWAP)
#define IRAMFUNC __attribute__ ((section ("ramcode")))
#else
#define IRAMFUNC
#endif

int cs_load_code (void *unused1, void *unused2, int unused3);
int cs_check_symb (void *symb);

#define PTR_ALIGNED(p, a) ((a) && ((arch_word_t)(p) % (a) == 0))

void heap_init (void);
void heap_deinit (void);
size_t heap_avail (void);

#if HEAP_TRACE

typedef struct bsp_heap_api_s {
     void *(*malloc) (uint32_t size, const char *func);
     void (*free) (void *p, const char *func);
} bsp_heap_api_t;

void *_heap_alloc_shared (size_t size, const char *func);
void *_heap_malloc (size_t size, const char *func);
void *_heap_realloc (void *x, size_t size, const char *func);
void *_heap_calloc (size_t size, const char *func);
void _heap_free (void *p, const char *func);

#define heap_alloc_shared(size) _heap_alloc_shared(size, __func__)
#define heap_malloc(size) _heap_malloc(size, __func__)
#define heap_realloc(x, size) _heap_realloc(x, size, __func__)
#define heap_calloc(size) _heap_calloc(size, __func__)
#define heap_free(p) _heap_free(p, __func__)

#define heap_alloc_shared_ptr _heap_alloc_shared
#define heap_malloc_ptr _heap_malloc
#define heap_realloc_ptr _heap_realloc
#define heap_calloc_ptr _heap_calloc
#define heap_free_ptr _heap_free

#define heap_api_malloc(api, size) (api)->malloc(size, __func__)
#define heap_api_free(api, p) (api)->free(p, __func__)

#else /*HEAP_TRACE*/

typedef struct bsp_heap_api_s {
     void *(*malloc) (uint32_t size);
     void (*free) (void *p);
} bsp_heap_api_t;

void *heap_alloc_shared (size_t size);
void *heap_malloc (size_t size);
void *heap_realloc (void *x, size_t size);
void *heap_calloc (size_t size);
void heap_free (void *p);

#define heap_alloc_shared_ptr heap_alloc_shared
#define heap_malloc_ptr heap_malloc
#define heap_realloc_ptr heap_realloc
#define heap_calloc_ptr heap_calloc
#define heap_free_ptr heap_free

#define heap_api_malloc(api, size) (api)->malloc(size)
#define heap_api_free(api, p) (api)->free(p)

#endif /*#HEAP_TRACE*/

#define heap_set_api_shared(api)            \
({                                          \
     (api)->malloc = heap_alloc_shared_ptr; \
     (api)->free = heap_free_ptr;           \
})

#ifdef __cplusplus
    }
#endif

#endif /*__HEAP_H__*/
