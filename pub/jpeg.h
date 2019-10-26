#ifndef __JPEG_H__
#define __JPEG_H__

#include <stdint.h>

typedef struct {
    uint16_t w, h;
    uint8_t colormode;
    uint8_t flags;
} jpeg_info_t;

int jpeg_init (const char *conf);
void *jpeg_cache (const char *path, uint32_t *size);
void jpeg_release (void *p);
int jpeg_decode (jpeg_info_t *info, void *tempbuf, void *data, uint32_t size);
void *jpeg_2_rawpic (const char *path, void *tmpbuf, uint32_t bufsize);

#endif /*__JPEG_H__*/

