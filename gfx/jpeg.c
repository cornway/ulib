
#include "int/lcd_int.h"

#include <jpeg.h>
#include <lcd_main.h>
#include <misc_utils.h>
#include <dev_io.h>
#include <heap.h>
#include <gui.h>

int jpeg_init (const char *conf)
{
    return JPEG_UserInit_HAL();
}

void *jpeg_cache (const char *path, uint32_t *size)
{
    int f;
    void *p;

    *size = d_open(path, &f, "r");
    if (f < 0) {
        return NULL;
    }

    p = heap_alloc_shared(*size);
    if (!p) {
        d_close(f);
        return NULL;
    }
    d_read(f, p, *size);
    d_close(f);
    return p;
}

void jpeg_release (void *p)
{
    heap_free(p);
}

int jpeg_decode (jpeg_info_t *info, void *tempbuf, void *data, uint32_t size)
{
    return JPEG_Decode_HAL(info, tempbuf, data, size);
}

void *jpeg_2_rawpic (const char *path, void *tmpbuf, uint32_t bufsize)
{
    void *cache;
    uint32_t size;
    jpeg_info_t info;
    rawpic_t *rawpic;

    cache = jpeg_cache(path, &size);
    if (!cache) {
        return NULL;
    }
    jpeg_decode(&info, tmpbuf, cache, size);

    size = info.w * info.h * 4;
    rawpic = heap_alloc_shared(size + sizeof(*rawpic));
    if (!rawpic) {
        return NULL;
    }
    rawpic->data = (void *)(rawpic + 1);
    rawpic->w = info.w;
    rawpic->h = info.h;

    d_memcpy(rawpic->data, tmpbuf, size);

    jpeg_release(cache);
    return rawpic;
}
