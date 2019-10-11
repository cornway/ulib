#ifndef __HEAP_H__
#define __HEAP_H__

#include <stdint.h>

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

typedef struct bsp_heap_api_s {
     void *(*malloc) (uint32_t size);
     void (*free) (void *p);
} bsp_heap_api_t;

void heap_init (void);
void heap_deinit (void);
void *heap_alloc_shared (size_t size);
int heap_avail (void);
void *heap_malloc (size_t size);
void *heap_realloc (void *x, size_t size);
void *heap_calloc (size_t size);
void heap_free (void *p);

#endif /*__HEAP_H__*/
