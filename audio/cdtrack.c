
#if defined(BSP_DRIVER)

#include "string.h"
#include "int/audio_int.h"
#include "audio_main.h"
#include "dev_io.h"
#include <debug.h>
#include <stdio.h>

#if MUSIC_MODULE_PRESENT

#define N(x) (sizeof(x) / sizeof(x[0]))

#define MUS_BUF_LEN_MS 1000

/*IMPORTANT NOTE : music won't play with small cache buffer (less than 'AUDIO_OUT_BUFFER_SIZE * 5'), -
  frequent access to the sd card (or any other storage)
  in blocking mode (currently used mode) - will hungs up system forever!
*/
/*NOTE 2 : cache size of 'AUDIO_OUT_BUFFER_SIZE * 5' seems to be ok,
  but such size will cause less fps.
*/
#define MUS_RAM_BUF_SIZE AUDIO_OUT_BUFFER_SIZE * 5

static snd_sample_t mus_ram_buf[2][MUS_RAM_BUF_SIZE];

typedef struct track_s track_t;

#define SONG_DESC(cd_track) ((track_t *)(cd_track)->desc)

struct track_s {
    int                 file;
    wave_t              stream;
    audio_channel_t     *channel;
    snd_sample_t        *cache;
    int                 remain;
    char                path[64];
    uint8_t             volume;
};

typedef enum {
    CD_IDLE     = 0x1,
    CD_PRELOAD  = 0x2,
    CD_PLAY     = 0x4,
    CD_REPEAT   = 0x8,
    CD_PAUSE    = 0x10,
    CD_RESUME   = 0x20,
    CD_STOP     = 0x40,
} cd_state_t;

#define CD_IDLE_M (~(CD_PLAY | CD_PRELOAD))

typedef enum {
    REPEAT_NONE,
    REPEAT_LOOP,
} mus_repeat_t;

typedef struct cdaudio_s cdaudio_t;

struct cdaudio_s {
    mus_repeat_t repeat;
    cd_state_t state;
    int last_upd_tsf;
    int8_t rd_idx;
};

track_t cd_track;
cdaudio_t cdaudio;

static void cd_preload_next (cdaudio_t *cdaudio, track_t *song);
static void cd_setup_track (cdaudio_t *cd, track_t *song);
static int a_preload_next (cdaudio_t *cd, track_t *song);


static void cd_reset_track (track_t *song)
{
    memset(song, 0, sizeof(*song));
}

static void cd_reset (cdaudio_t *cd)
{
    cd->rd_idx = 0;
    cd->repeat = REPEAT_LOOP;
    cd->state = CD_IDLE;
}

static int cd_cplt_hdlr (uint8_t *abuf, uint32_t alen, cplt_stat_t complete)
{
    switch (complete) {
        case A_HALF:
            if ((CD_IDLE_M & cdaudio.state) || !cd_track.volume) {
                /*Skip this*/
                return 1;
            }
        break;
        case A_FULL:

            if (cd_track.remain <=  0) {
                if (cdaudio.repeat == REPEAT_LOOP) {
                    cdaudio.state = CD_REPEAT;
                }
                return 1;
            }
            if ((CD_IDLE_M & cdaudio.state) || !cd_track.volume) {
                /*This will cause 'fake channel' - it won't play, but suspend*/
                return 0;
            }
            cd_setup_track(&cdaudio, &cd_track);
        break;
        default:
        break;
    }
    return 0;
}

static int cd_open_stream (track_t *song, const char *path)
{
    song->file = -1;
    strcpy(song->path, path);
    d_open(song->path, &song->file, "r");
    if (song->file < 0)
        return -1;

    if (d_read(song->file, &song->stream, sizeof(song->stream)) < 0) {
        error_handle();
    }
    if (!a_wave_supported(&song->stream)) {
        dprintf("%s() : trying to load unsupported wave\n", __func__);
        error_handle();
    }
    song->remain = song->stream.FileSize / sizeof(snd_sample_t);
    return 0;
}

static void cd_close_stream (cdaudio_t *cd, track_t *song)
{
    d_close(song->file);
    song->cache = NULL;
    cd_reset_track(song);
    cd_reset(cd);
    song->file = -1;
}

static int _cd_play (cdaudio_t *cd, track_t *song, char *path, int repeat)
{
    Mix_Chunk mixchunk;
    if (cdaudio.state != CD_IDLE) {
        cd_close_stream(cd, song);
    }

    if (cd_open_stream(song, path) < 0) {
        return -1;
    }
    cd->state = CD_PLAY;
    cd->repeat = repeat ? REPEAT_LOOP : REPEAT_NONE;
    cd_preload_next(cd, song);
    /*Little hack - force channel reject to update it through callback*/
    memset(&mixchunk, 0, sizeof(mixchunk));
    mixchunk.loopstart = -1;
    song->cache = mus_ram_buf[0];
    mixchunk.cache = (void **)&song->cache;

    if (audio_is_playing(AUDIO_MUS_CHAN_START)) {
        audio_stop_channel(AUDIO_MUS_CHAN_START);
    }
    song->channel = audio_play_channel(&mixchunk, AUDIO_MUS_CHAN_START);
    if (!song->channel) {
        error_handle();
    }
    song->channel->complete = cd_cplt_hdlr;
    return 0;
}

static int
a_preload_next (cdaudio_t *cd, track_t *song)
{
    snd_sample_t *ram;
    audio_channel_t *channel = song->channel;
    Mix_Chunk *chunk = &channel->chunk;
    int32_t rem = song->remain;

    ram = mus_ram_buf[cd->rd_idx];

    if (rem > MUS_RAM_BUF_SIZE) {
        rem = MUS_RAM_BUF_SIZE;
    }
    chunk->abuf = ram;
    chunk->alen = rem;
    chunk->volume = song->volume;
    return rem;
}

static void cd_setup_track (cdaudio_t *cd, track_t *song)
{
    int32_t rem = song->remain;

    if (song->channel) {
        cd->rd_idx ^= 1;
        rem = a_preload_next(cd, song);
        song->remain -= rem;
        cd->state = CD_PRELOAD;
    }
}

static void cd_dorepeat (cdaudio_t *cd, track_t *song)
{
    char path[128];
    uint8_t volume = song->volume;
    strcpy(path, song->path);
    _cd_play(cd, song, path, cd->repeat);
    song->volume = volume;
}

void cd_tickle (void)
{
    switch (cdaudio.state) {
        case CD_PLAY:
        break;
        case CD_PRELOAD:
            cd_preload_next(&cdaudio, &cd_track);
            cdaudio.state = CD_PLAY;
        break;
        case CD_STOP:
            audio_stop_channel(AUDIO_MUS_CHAN_START);
            cd_close_stream(&cdaudio, &cd_track);
            cd_reset(&cdaudio);
        break;
        case CD_REPEAT:
            cd_dorepeat(&cdaudio, &cd_track);
        break;
        default:
        break;
    }
}

static void cd_preload_next (cdaudio_t *cd, track_t *song)
{
    snd_sample_t *dest;
    int32_t rem = song->remain;

    dest = mus_ram_buf[cd->rd_idx ^ 1];

    if (rem > MUS_RAM_BUF_SIZE)
        rem = MUS_RAM_BUF_SIZE;

    if (d_read(song->file, dest, rem * sizeof(snd_sample_t)) < 0) {
        error_handle();
    }
}

int cd_playing (void)
{
    return (    (cdaudio.state == CD_PLAY) || 
                (cdaudio.state == CD_PRELOAD)
            );
}

cd_track_t *cd_play_name (cd_track_t *track, const char *path)
{
    track->desc = &cd_track;
    if (_cd_play(&cdaudio, &cd_track, (char *)path, 1) < 0) {
        return NULL;
    }
    return track;
}

int cd_pause (cd_track_t *track)
{
    cdaudio.state = CD_PAUSE;
    return 1;
}

int cd_resume (cd_track_t *track)
{
    if (cdaudio.state == CD_PAUSE) {
        cdaudio.state = CD_PLAY;
        return 0;
    }
    return 1;
}

int cd_stop (cd_track_t *track)
{
    if (cdaudio.state != CD_IDLE)
        cdaudio.state = CD_STOP;
    return 0;
}

int cd_volume (cd_track_t *track, uint8_t vol)
{
    track_t *s = SONG_DESC(track);
    s->volume = vol;
    return 0;
}

uint8_t cd_getvol (cd_track_t *track)
{
    return SONG_DESC(track)->volume;
}

int cd_init (void)
{
    cd_reset(&cdaudio);
    cd_reset_track(&cd_track);
    cd_track.volume = MAX_VOL / 2;
    return 0;
}

#else

int cd_init (void)
{
    return 0;
}

cd_track_t *cd_play_name (cd_track_t *track, const char *path)
{
    return NULL;
}

int cd_pause (cd_track_t *track)
{
    return 0;
}

int cd_resume (cd_track_t *track)
{
    return 0;
}

int cd_stop (cd_track_t *track)
{
    return 0;
}


int cd_playing (cd_track_t *track)
{
    return 0;
}

void cd_tickle (cd_track_t *track)
{

}

#endif

#endif
