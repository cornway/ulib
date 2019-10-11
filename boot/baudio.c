
#include <debug.h>
#include "int/boot_int.h"
#include <misc_utils.h>
#include <audio_main.h>
#include <heap.h>

#define BSFX_POOLMAX 16

typedef struct bsfx_s {
    void *cache;
    int channel;
    int wavenum;
    uint16_t id;
} bsfx_t;

bsfx_t *bsfx_pool[BSFX_POOLMAX] = {NULL};

static inline int
__alloc_sfx (void)
{
    int i;
    for (i = 0; i < BSFX_POOLMAX; i++) {
        if (bsfx_pool[i] == NULL) {
            return i;
        }
    }
    return -1;
}

static inline void
__set_sfx (int i, bsfx_t *sfx)
{
    assert(i < BSFX_POOLMAX);
    bsfx_pool[i] = sfx;
    sfx->id = i;
}

static inline bsfx_t *
__get_sfx (int i)
{
    assert(i < BSFX_POOLMAX);
    return bsfx_pool[i];
}

static bsfx_t *__boot_check_sfx_exist (int num)
{
    int i;
    for (i = 0; i < BSFX_POOLMAX; i++) {
        if (bsfx_pool[i] && bsfx_pool[i]->wavenum == num) {
            return bsfx_pool[i];
        }
    }
    return NULL;
}

int bsp_open_wave_sfx (const char *name)
{
    int cachenum, cachesize, sfxidx;
    bsfx_t *sfx;

    cachenum = audio_wave_open(name, -1);
    if (cachenum < 0) {
        return -1;
    }
    sfx = __boot_check_sfx_exist(cachenum);
    if (sfx) {
        return sfx->id;
    }
    cachesize = audio_wave_size(cachenum);
    if (cachesize < 0) {
        return -1;
    }
    sfxidx = __alloc_sfx();
    if (sfxidx < 0) {
        return -1;
    }
    sfx = heap_alloc_shared(sizeof(*sfx) + cachesize + 1);
    if (!sfx) {
        return -1;
    }
    sfx->cache = sfx + 1;
    sfx->wavenum = cachenum;
    sfx->channel = -1;
    __set_sfx(sfxidx, sfx);
    audio_wave_cache(cachenum, (uint8_t *)sfx->cache, cachesize);

    return sfx->id;
}

int bsp_play_wave_sfx (int hdl, uint8_t volume)
{
    int chanum;
    Mix_Chunk chunk;
    bsfx_t *sfx = __get_sfx(hdl);

    if (!sfx) {
        return -1;
    }
    chanum = audio_chk_priority(-1);
    if (chanum < 0) {
        chanum = audio_chk_priority(0);
        assert(chanum >= 0);
    }
    if (audio_is_playing(chanum)) {
        audio_stop_channel(chanum);
    }
    chunk.abuf = (snd_sample_t *)sfx->cache;
    chunk.alen = audio_wave_size(sfx->wavenum);
    chunk.allocated = 1;
    chunk.cache = (void **)&sfx->cache;
    chunk.loopstart = -1;
    chunk.volume = volume;
    if (!audio_play_channel(&chunk, chanum)) {
        return -1;
    }
    sfx->channel = chanum;

    return 0;
}

int bsp_stop_wave_sfx (int hdl)
{
    bsfx_t *sfx = __get_sfx(hdl);
    if (!audio_is_playing(sfx->channel)) {
        return -1;
    }
    audio_stop_channel(hdl);
    return 0;
}

void bsp_release_wave_sfx (int hdl)
{
    bsfx_t *sfx = __get_sfx(hdl);

    bsp_stop_wave_sfx(hdl);
    __set_sfx(hdl, NULL);
    audio_wave_close(sfx->wavenum);
    heap_free(sfx);
}

