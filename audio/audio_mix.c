#if defined(BSP_DRIVER)

#include <stdint.h>
#include <stddef.h>
#include "int/audio_int.h"
#include "audio_main.h"
#include <heap.h>

#define GAIN(x, vol, comp) (((int16_t)x * vol) / comp)
#define GAIN_FLOAT(x, vol) (int16_t)((float)(x) * (float)vol)

static void a_paint_buf_ex (a_channel_head_t *chanlist, a_buf_t *abuf, int compratio);

#if USE_REVERB

#define REVERB_END_BUFFER (0x800)
#define REVERB_END_BUFFER_M (REVERB_END_BUFFER - 1)

#define REVERB2_END_BUFFER (0x100)
#define REVERB2_END_BUFFER_M (REVERB2_END_BUFFER - 1)

static snd_sample_t reverb_raw_buf[REVERB_END_BUFFER];
static snd_sample_t reverb2_raw_buf[REVERB2_END_BUFFER];


static uint16_t  rev_rd_idx = 0, rev_wr_idx = 0;
static uint16_t  rev2_rd_idx = 0, rev2_wr_idx = 0;


static void a_rev_push (snd_sample_t s)
{
    reverb_raw_buf[(rev_rd_idx++) & REVERB_END_BUFFER_M] = s;
}

static snd_sample_t a_rev_pop (void)
{
    return reverb_raw_buf[(rev_wr_idx++) & REVERB_END_BUFFER_M];
}

static void a_rev2_push (snd_sample_t s)
{
    reverb2_raw_buf[(rev2_rd_idx++) & REVERB2_END_BUFFER_M] = s;
}

static snd_sample_t a_rev2_pop (void)
{
    return reverb2_raw_buf[(rev2_wr_idx++) & REVERB2_END_BUFFER_M];
}

void a_rev_init (void)
{
    int i;
    for (i = 0; i < REVERB_END_BUFFER; i++) {
        a_rev_push(0);
    }
    for (i = 0; i < REVERB2_END_BUFFER; i++) {
        a_rev2_push(0);
    }
}

static void a_rev_proc (a_buf_t *abuf)
{
    int i = 0;
    snd_sample_t s, *buf = abuf->buf;
    int durty = 0;

    for (i = 0; i < abuf->samples; i++) {
        s = a_rev_pop();
        if (s) {
            buf[i] = buf[i] / 2 + s / 2;
            durty++;
        }
        a_rev_push(buf[i]);
        s = a_rev2_pop();
        if (s) {
            buf[i] = buf[i] / 2 + s / 2;
            durty++;
        }
        a_rev2_push(buf[i]);
    }
    if (durty) {
        *abuf->dirty = true;
    }
}
#endif /*USE_REVERB*/


#if !USE_FLOAT
#error "Unsupported"
#endif

IRAMFUNC static void
a_write_single_to_master (snd_sample_t *dest, mixdata_t *mixdata, int compratio);

static void
a_write_single_to_master (snd_sample_t *dest, mixdata_t *mixdata, int compratio)
{
    int16_t *pdest = (int16_t *)dest;
    int16_t *psrc = (int16_t *)mixdata[0].buf;
    uint8_t vol = mixdata[0].volume;

    if (vol == MAX_VOL) {
        for (int i = 0; i < mixdata->size; i++) {
            pdest[i] = psrc[i] / compratio;
        }
    } else {
        int weight = compratio * MAX_VOL;
        float vol_flt = (float)(vol) / (float)(weight);
        for (int i = 0; i < mixdata->size; i++) {
            pdest[i] = GAIN_FLOAT(psrc[i], vol_flt);
        }
    }
}

IRAMFUNC static void
a_mix_single_to_master (snd_sample_t *dest, mixdata_t *mixdata, int compratio);

static void
a_mix_single_to_master (snd_sample_t *dest, mixdata_t *mixdata, int compratio)
{
    int16_t *pdest = (int16_t *)dest;
    int16_t *psrc = (int16_t *)mixdata[0].buf;
    uint8_t vol = mixdata[0].volume;

    if (vol == 0)
        return;

    if (vol == MAX_VOL) {
        for (int i = 0; i < mixdata->size; i++) {
            pdest[i] = psrc[i] / compratio  + pdest[i];
        }
    } else {
        int weight = compratio * MAX_VOL;
        float vol_flt = (float)(vol) / (float)(weight);
        for (int i = 0; i < mixdata->size; i++) {
            pdest[i] = GAIN_FLOAT(psrc[i], vol_flt) + pdest[i];
        }
    }
}

#if (AUDIO_PLAY_SCHEME == 1)

static inline void
mix_to_master_raw2 (snd_sample_t *dest, mixdata_t *mixdata, int compratio)
{
    a_mix_single_to_master(dest, mixdata, compratio);
    a_mix_single_to_master(dest, mixdata + 1, compratio);
}

static inline void
mix_to_master_raw4 (snd_sample_t *dest, mixdata_t *mixdata, int compratio)
{
    mix_to_master_raw2(dest, mixdata, compratio);
    mix_to_master_raw2(dest, mixdata + 2, compratio);
}

static void
chunk_proc_raw_all (snd_sample_t *dest, mixdata_t *mixdata, int cnt, int compratio)
{
    switch (cnt) {

        case 1:
            a_mix_single_to_master(dest, mixdata, compratio);
        break;

        case 2:
            mix_to_master_raw2(dest, mixdata, compratio);
        break;

        case 3:
            mix_to_master_raw2(dest, mixdata, compratio);
            a_mix_single_to_master(dest, mixdata + 2, compratio);
        break;

        case 4:
            mix_to_master_raw4(dest, mixdata, compratio);
        break;

        case 5:
            mix_to_master_raw4(dest, mixdata, compratio);
            a_mix_single_to_master(dest, mixdata + 4, compratio);
        break;

        case 6:
            mix_to_master_raw4(dest, mixdata, compratio);
            mix_to_master_raw2(dest, mixdata + 4, compratio);
        break;

        case 7:
            mix_to_master_raw4(dest, mixdata, compratio);
            mix_to_master_raw2(dest, mixdata + 4, compratio);
            a_mix_single_to_master(dest, mixdata + 6, compratio);
        break;

        case 8:
            mix_to_master_raw4(dest, mixdata, compratio);
            mix_to_master_raw4(dest, mixdata + 4, compratio);
        break;

        default: error_handle();
    }
}

static void
a_grab_mixdata_all (a_channel_head_t *chanlist, a_buf_t *track, mixdata_t *mixdata)
{
    a_channel_t *cur, *next;
    int minsamples;

    minsamples = chanlist->first->loopsize;
    if (minsamples >= track->samples) {
        minsamples = track->samples;
    }

    a_chan_foreach_safe(chanlist, cur, next) {
        a_grab_mixdata(cur, track, mixdata);
        mixdata->size = minsamples;
        mixdata++;
    }
}

void a_paint_buffer (a_channel_head_t *chanlist, a_buf_t *track, int compratio)
{
    snd_sample_t *dest = track->buf;
    mixdata_t mixdata[AUDIO_MUS_CHAN_START + 1];

    /*TODO : fix this*/
    if (chanlist->size) {

        if (chanlist->first->loopsize < track->samples) {
            a_paint_buf_ex(chanlist, track, compratio);
        }

        a_grab_mixdata_all(chanlist, track, mixdata);

        chunk_proc_raw_all(dest, mixdata, chanlist->size, compratio);
    }
}

#else /*AUDIO_PLAY_SCHEME != 1*/

void a_paint_buffer (a_channel_head_t *chanlist, a_buf_t *abuf, int compratio)
{
    a_paint_buf_ex(chanlist, abuf, compratio);
}

#endif /*AUDIO_PLAY_SCHEME*/

static void a_paint_buf_ex (a_channel_head_t *chanlist, a_buf_t *abuf, int compratio)
{
    a_channel_t *cur, *next;
    mixdata_t mixdata;
    int dirty = 0;
    int cnt = 0;

    a_chan_foreach_safe(chanlist, cur, next) {
        if (a_chn_cplt(cur) && a_chn_cplt(cur)((uint8_t *)a_chunk_data(cur), a_chunk_len(cur) * sizeof(snd_sample_t), A_HALF)) {
            dirty++;
            cnt++;
            continue;
        }

        a_grab_mixdata(cur, abuf, &mixdata);

        if (mixdata.size) {
            a_mix_single_to_master(abuf->buf, &mixdata, compratio);
            dirty++;
        }
        cnt++;
    }
#if (USE_REVERB)
    a_rev_proc(abuf);
#endif
    if (dirty) {
        *abuf->dirty = d_true;
    }
}

static inline void
a_move_channel_window (a_channel_t *desc, a_buf_t *abuf, mixdata_t *mixdata)
{
    int size;

    if (a_chn_play(desc)== 0) {
        return;
    }

    mixdata->buf = desc->bufposition;
    size = abuf->samples;

    if (desc->loopsize < abuf->samples)
        size = desc->loopsize;

    desc->bufposition += size;
    desc->loopsize -= size;
    mixdata->size = size;
}

void
a_grab_mixdata (a_channel_t *channel, a_buf_t *track, mixdata_t *mixdata)
{
    a_move_channel_window(channel, track, mixdata);
    mixdata->volume = channel->volume;
}

static inline uint8_t
a_chan_try_reject (a_channel_t *desc)
{
    void **cache = a_chunk_cache(desc);

    if (cache && (*cache == NULL)) {
        goto remove;
    }
    if (desc->loopsize <= 0) {

        if (a_chn_loopstart(desc) >= 0) {

            desc->loopsize = a_chunk_len(desc);
            desc->bufposition = a_chunk_data(desc);
            desc->bufposition += a_chn_loopstart(desc);
            desc->loopsize -= a_chn_loopstart(desc);
            if (desc->loopsize < 0) {
                error_handle();
            }
        }

        if (a_chn_cplt(desc)) {
            if (a_chn_cplt(desc)((uint8_t *)a_chunk_data(desc), a_chunk_len(desc) * sizeof(snd_sample_t), A_FULL)) {
                goto remove;
            } else {
                desc->bufposition = a_chunk_data(desc);
                desc->loopsize = a_chunk_len(desc);
                desc->volume = a_chunk_vol(desc);
            }
        } else {
            goto remove;
        }
    }

    return 0;
remove :
    a_channel_remove(desc);
    return 1;
}

uint8_t a_chanlist_try_reject_all (a_channel_head_t *chanlist)
{
    a_channel_t *cur, *next;
    a_chan_foreach_safe(chanlist, cur, next) {
        if (a_chn_play(cur)) {
            a_chan_try_reject(cur);
        }
    }
    return chanlist->size;
}

#endif
