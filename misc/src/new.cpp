#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <heap.h>
#include <debug.h>



void *operator new (size_t size)
{
    return heap_malloc(size);
}

void operator delete (void *p)
{
    heap_free(p);
}

