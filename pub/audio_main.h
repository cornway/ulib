#ifndef _AUDIO_MAIN_H
#define _AUDIO_MAIN_H

#include "stdint.h"
#include "dev_conf.h"
#include <nvic.h>
#include <bsp_api.h>

#define AUDIO_SIZE_TO_MS(rate, size) (((long long)(size) * 1000) / (rate))
#define AUDIO_MS_TO_SIZE(rate, ms) (((((rate) << 2) / 1000) * (ms)) >> 2)

#define MAX_VOL (0x7f)

#define AUDIO_SAMPLES_2_BYTES(samples) ((samples) * sizeof(snd_sample_t))
#define AUDIO_SAMPLES_2_WORDS(samples) (AUDIO_SAMPLES_2_BYTES(samples) / sizeof(uint32_t))
#define AUDIO_SAMPLES_2_DWORDS(samples) (AUDIO_SAMPLES_2_BYTES(samples) / sizeof(uint64_t))

#define AUDIO_BYTES_2_SAMPLES(bytes) ((bytes) / sizeof(snd_sample_t))

typedef struct {
  uint32_t ChunkID;       /* 0 */ 
  uint32_t FileSize;      /* 4 */
  uint32_t FileFormat;    /* 8 */
  uint32_t SubChunk1ID;   /* 12 */
  uint32_t SubChunk1Size; /* 16*/  
  uint16_t AudioFormat;   /* 20 */ 
  uint16_t NbrChannels;   /* 22 */   
  uint32_t SampleRate;    /* 24 */
  
  uint32_t ByteRate;      /* 28 */
  uint16_t BlockAlign;    /* 32 */  
  uint16_t BitPerSample;  /* 34 */  
  uint32_t SubChunk2ID;   /* 36 */   
  uint32_t SubChunk2Size; /* 40 */    
} wave_t;


typedef int16_t snd_sample_t;

typedef struct {
    int allocated;
    snd_sample_t *abuf;
    int32_t alen;
    void **cache;
    int loopstart;
    uint8_t volume;     /* Per-sample volume, 0-128 */
} Mix_Chunk;

typedef enum {
    A_NONE,
    A_HALF,
    A_FULL
} cplt_stat_t;

typedef struct {
    Mix_Chunk chunk;
    int8_t is_playing;
    uint8_t id;
    int (*complete) (uint8_t *, uint32_t, cplt_stat_t);
} audio_channel_t;

typedef struct {
    void *desc;
} cd_track_t;

typedef struct bsp_sfx_api_s {
    bspdev_t dev;
    void (*mixer_ext) (void (*) (int, void *, int, void *));
    audio_channel_t *(*play) (Mix_Chunk *, int);
    void (*stop) (int);
    void (*pause) (int);
    void (*stop_all) (void);
    int (*is_play) (int);
    int (*check) (int);
    void (*set_pan) (int, int, int);
    void (*sample_volume) (audio_channel_t *, uint8_t);
    void (*tickle) (void);
    void (*irq_save) (irqmask_t *);
    void (*irq_restore) (irqmask_t);

    int (*wave_open) (const char *, int);
    int (*wave_size) (int);
    int (*wave_cache) (int, uint8_t *, int);
    void (*wave_close) (int);
} bsp_sfx_api_t;

#define BSP_SFX_API(func) ((bsp_sfx_api_t *)(g_bspapi->sfx))->func

typedef struct bsp_cd_api_s {
    bspdev_t dev;
    cd_track_t *(*play) (cd_track_t *, const char *);
    int (*pause) (cd_track_t *);
    int (*resume) (cd_track_t *);
    int (*stop) (cd_track_t *);
    int (*volume) (cd_track_t *, uint8_t);
    uint8_t (*getvol) (cd_track_t *);
    int (*playing) (void);
} bsp_cd_api_t;

#define BSP_CD_API(func) ((bsp_cd_api_t *)(g_bspapi->cd))->func

#if BSP_INDIR_API

#define audio_init              BSP_SFX_API(dev.init)
#define audio_deinit            BSP_SFX_API(dev.deinit)
#define audio_conf              BSP_SFX_API(dev.conf)
#define audio_info              BSP_SFX_API(dev.info)
#define audio_priv              BSP_SFX_API(dev.priv)
#define audio_mixer_ext         BSP_SFX_API(mixer_ext)
#define audio_play_channel      BSP_SFX_API(play)
#define audio_stop_channel      BSP_SFX_API(stop)
#define audio_pause             BSP_SFX_API(pause)
#define audio_stop_all          BSP_SFX_API(stop_all)
#define audio_is_playing        BSP_SFX_API(is_play)
#define audio_chk_priority      BSP_SFX_API(check)
#define audio_set_pan           BSP_SFX_API(set_pan)
#define audio_sample_vol        BSP_SFX_API(sample_volume)
#define audio_update            BSP_SFX_API(tickle)
#define audio_irq_save          BSP_SFX_API(irq_save)
#define audio_irq_restore       BSP_SFX_API(irq_restore)

#define audio_wave_open         BSP_SFX_API(wave_open)
#define audio_wave_size         BSP_SFX_API(wave_size)
#define audio_cache_wave        BSP_SFX_API(wave_cache)
#define audio_wave_close        BSP_SFX_API(wave_close)

#define cd_init                 BSP_CD_API(dev.init)
#define cd_deinit               BSP_CD_API(dev.deinit)
#define cd_conf                 BSP_CD_API(dev.conf)
#define cd_info                 BSP_CD_API(dev.info)
#define cd_priv                 BSP_CD_API(dev.priv)
#define cd_play_name            BSP_CD_API(play)
#define cd_pause                BSP_CD_API(pause)
#define cd_resume               BSP_CD_API(resume)
#define cd_stop                 BSP_CD_API(stop)
#define cd_volume               BSP_CD_API(volume)
#define cd_getvol               BSP_CD_API(getvol)
#define cd_playing              BSP_CD_API(playing)

#else

int audio_init (void);
void audio_deinit (void);
int audio_conf (const char *str);
void audio_mixer_ext (void (*mixer_callback) (int, void *, int, void *));
audio_channel_t *audio_play_channel (Mix_Chunk *chunk, int channel);
void audio_stop_channel (int channel);
void audio_pause (int channel);
void audio_stop_all (void);
int audio_is_playing (int handle);
int audio_chk_priority (int priority);
void audio_set_pan (int handle, int l, int r);
void audio_sample_vol (audio_channel_t *achannel, uint8_t volume);
void audio_update (void);
void audio_irq_save (irqmask_t *flags);
void audio_irq_restore (irqmask_t flags);

int audio_wave_open (const char *name, int num);
int audio_wave_size (int num);
int audio_wave_cache (int num, uint8_t *dest, int size);
void audio_wave_close (int num);

int cd_init (void);
cd_track_t *cd_play_name (cd_track_t *track, const char *path);
int cd_pause (cd_track_t *track);
int cd_resume (cd_track_t *track);
int cd_stop (cd_track_t *track);
int cd_volume (cd_track_t *track, uint8_t vol);
uint8_t cd_getvol (cd_track_t *track);
int cd_playing (void);
void cd_tickle (void);
#endif


#endif /*_AUDIO_MAIN_H*/
