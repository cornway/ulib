#ifndef __JPEG_H__
#define __JPEG_H__

#ifdef __cplusplus
    extern "C" {
#endif

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t w, h;
    uint8_t colormode;
    uint8_t flags;
} jpeg_info_t;

int jpeg_init (const char *conf);
void *jpeg_cache (const char *path, size_t *size);
void jpeg_release (void *p);
int jpeg_decode (jpeg_info_t *info, void *tempbuf, void *data, uint32_t size);
void *jpeg_2_rawpic (const char *path, void *tmpbuf, uint32_t bufsize);

#ifdef __cplusplus
    }
#endif

#endif /*__JPEG_H__*/

