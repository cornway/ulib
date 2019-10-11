#include <string.h>
#include <stdio.h>
#include "int/audio_int.h"
#include <bsp_sys.h>
#include <audio_main.h>
#include <dev_io.h>
#include <debug.h>

#if defined(BSP_DRIVER)

#define A_MAX_SLOTNUM 100
#define A_MAXPATH 128
#define A_MAX_REFCNT 255

typedef struct sfx_cache_s {
    struct sfx_cache_s *next;
    int size;
    int16_t dataoff;
    int16_t slotnum;
    uint8_t ref; /*TODO : make atomic*/
    char path[1];
} sfx_cache_t;

#define a_refcnt(sfx) ((sfx)->ref++)
#define a_unrefcnt(sfx) ((sfx)->ref--)
#define a_refcheck(sfx) ((sfx)->ref)

static sfx_cache_t *wave_cache_head = NULL;
static sfx_cache_t *wave_cache_slot[A_MAX_SLOTNUM] = {NULL};
static int wave_cache_slots_used = 0;

static inline sfx_cache_t **
__next_slot_avail (int *idx)
{
    int i;

    *idx = -1;
    for (i = 0; i < A_MAX_SLOTNUM; i++) {
        if (wave_cache_slot[i] == NULL) {
            *idx = i;
            return &wave_cache_slot[i];
        }
    }
    return NULL;
}

static void
__link_desc (sfx_cache_t *sfx)
{
    sfx_cache_t **slot;
    int num;

    sfx->next = wave_cache_head;
    wave_cache_head = sfx;

    slot = __next_slot_avail(&num);
    assert(slot);
    *slot = sfx;
    sfx->slotnum = num;
    wave_cache_slots_used++;
}

static void
__unlink_desc (sfx_cache_t *sfx)
{
    sfx_cache_t *cur = wave_cache_head, *prev = NULL;

    while (cur) {
        if (cur == sfx) {
            if (prev) {
                prev->next = cur->next;
            } else {
                wave_cache_head = cur->next;
            }
            wave_cache_slot[sfx->slotnum] = NULL;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
    assert(0);
}

static sfx_cache_t *
__new_cache_desc (const char *path)
{
    sfx_cache_t *sfx;
    int pathlen = strlen(path) + 1;
    int memsize = sizeof(sfx_cache_t) + pathlen;

    if (wave_cache_slots_used >= A_MAX_SLOTNUM) {
        dprintf("%s() : No more slots available\n", __func__);
        return NULL;
    }

    sfx = (sfx_cache_t *)heap_malloc(memsize);
    if (!sfx) {
        dprintf("%s() : fail\n", __func__);
        return NULL;
    }
    memset(sfx, 0, memsize);
    d_memcpy(sfx->path, path, pathlen);

    __link_desc(sfx);

    return sfx;
}

static sfx_cache_t *
a_wave_cached (const char *path, int num)
{
    sfx_cache_t *sfx = wave_cache_head;

    if (num >= 0 && num < A_MAX_SLOTNUM) {
        if (wave_cache_slot[num] == NULL) {
            dprintf("%s() : unallocated\n", __func__);
        }
        return wave_cache_slot[num];
    }

    if (!path) {
        return NULL;
    }
    while (sfx) {
        if (strcmp(sfx->path, path) == 0) {
            return sfx;
        }
        sfx = sfx->next;
    }
    return NULL;
}

static sfx_cache_t *
a_wave_cache_alloc (const char *path, int *num)
{
    sfx_cache_t *sfx;

    sfx = a_wave_cached(path, *num);
    if (sfx) {
        /*TODO : overflow ???*/
        a_refcnt(sfx);
        return sfx;
    }
    *num = -1;
    sfx = __new_cache_desc(path);
    if (!sfx) {
        return NULL;
    }
    a_refcnt(sfx);
    return sfx;
}

static void
a_wave_cache_free (sfx_cache_t *sfx)
{
    a_unrefcnt(sfx);

    if (a_refcheck(sfx) > 0) {
        return;
    }
    __unlink_desc(sfx);
    heap_free(sfx);
    wave_cache_slots_used--;
}

int
audio_wave_open (const char *path, int num)
{
    int file, size;
    wave_t wave;
    sfx_cache_t *sfx;

    sfx = a_wave_cache_alloc(path, &num);
    if (num >= 0) {
        return num;
    }

    size = d_open(path, &file, "r");

    if (file < 0 || size < 0) {
        a_wave_cache_free(sfx);
        return -1;
    }

    if (d_read(file, &wave, sizeof(wave)) <= 0) {
        goto fail;
    }
    if (!a_wave_supported(&wave)) {
        goto fail;
    }

    sfx->dataoff =  wave.SubChunk1Size;
    sfx->size = size - wave.SubChunk1Size;
    d_close(file);
    return sfx->slotnum;
fail:
    a_wave_cache_free(sfx);
    d_close(file);
    return -1;
}

int
audio_wave_size (int num)
{
    sfx_cache_t *sfx = a_wave_cached(NULL, num);
    if (!sfx) {
        return -1;
    }
    return sfx->size;
}

int
audio_wave_cache (int num, uint8_t *dest, int size)
{
    sfx_cache_t *sfx;
    int file;

    sfx = a_wave_cached(NULL, num);

    assert(sfx);

    d_open(sfx->path, &file, "r");
    if (file < 0) {
        return -1;
    }
    d_seek(file, sfx->dataoff, DSEEK_SET);
    if (d_read(file, dest, size) < 0) {
        return -1;
    }
    d_close(file);
    return size;
}

void
audio_wave_close (int num)
{
    sfx_cache_t *sfx = a_wave_cached(NULL, num);

    assert(sfx);
    a_wave_cache_free(sfx);
}


/*internal*/
d_bool a_wave_supported (wave_t *wave)
{
    a_intcfg_t *cfg = a_get_conf();

    if (readShort(&wave->BitPerSample) != cfg->samplebits) {
        return d_false;
    }
    if (readShort(&wave->NbrChannels) != cfg->channels) {
        return d_false;
    }
    if (readLong(&wave->SampleRate) != cfg->samplerate) {
        return d_false;
    }
    return d_true;
}

#endif
