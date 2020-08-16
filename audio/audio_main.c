#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <nvic.h>
#include <misc_utils.h>
#include <bsp_cmd.h>
#include <debug.h>
#include <term.h>
#include <heap.h>

#include <audio_main.h>
#include "../../common/int/audio_int.h"

#if AUDIO_MODULE_PRESENT

#define CHANNEL_NUM_VALID(num) \
    (((num) < AUDIO_MAX_VOICES) || ((num) == AUDIO_MUS_CHAN_START))

extern isr_status_e g_audio_isr_status;
extern int g_audio_isr_pend[A_ISR_MAX];
extern d_bool g_audio_proc_isr;

audio_t *audio;

void error_handle (void)
{
    assert(0);
}

static inline void
a_chan_reset (a_channel_t *desc)
{
    d_memset(desc, 0, sizeof(*desc));
}

static void
a_chanlist_empty_clbk (struct a_channel_head_s *head)
{
    if (cd_playing())
        return;
    audio->force_stop = d_true;
}

static void
a_chanlist_first_node_clbk (struct a_channel_head_s *head)
{
    if (cd_playing())
        return;
}

void
a_chanlist_node_remove_clbk (struct a_channel_head_s *head, a_channel_t *rem)
{
    a_chan_reset(rem);
}

static audio_channel_t *
a_channel_insert (audio_t *audio, Mix_Chunk *chunk, int channel)
{
    a_channel_t *desc = &audio->pool[channel];
    if (desc->inst.is_playing == 0) {
        desc->inst.is_playing = 1;
        desc->inst.chunk = *chunk;
        a_channel_link(&audio->head, desc, 0);
    } else {
        error_handle();
    }
    desc->inst.chunk     = *chunk;
    a_chunk_len(desc) = AUDIO_BYTES_2_SAMPLES(a_chunk_len(desc));
    desc->inst.id        = channel;
    desc->loopsize =     a_chunk_len(desc);
    desc->bufposition =  a_chunk_data(desc);
    desc->volume =       a_chunk_vol(desc);
    desc->priority = 0;
    return &desc->inst;
}

void
a_channel_remove (audio_t *audio, a_channel_t *desc)
{
    a_channel_unlink(&audio->head, desc);
}

void
a_paint_buff_helper (audio_t *audio, a_buf_t *abuf)
{
    int compratio = audio->head.size + 2;
    d_bool dirty = d_false;

    if (audio->amixer_callback) {
        a_clear_abuf(abuf);
        audio->amixer_callback(-1, abuf->buf, abuf->samples * sizeof(abuf->buf[0]), NULL);
        dirty = d_true;
    }
    if (a_chanlist_try_reject_all(audio, &audio->head) == 0) {
        if (!dirty) {
            a_clear_abuf(abuf);
        }
        return;
    }
    a_paint_buffer(audio, abuf, compratio);
    *abuf->dirty |= dirty;
}

void a_paint_all (audio_t *audio, d_bool force, int *pend)
{
    a_buf_t master;
    int id, bufidx = 0;

    for (id = A_ISR_HALF; id < A_ISR_MAX; id++) {
        if (pend[id] || force) {
            a_get_master4idx(&master, bufidx);
            a_paint_buff_helper(audio, &master);
        }
        bufidx++;
    }
}


static void
a_parse_config (a_intcfg_t *cfg, const char *str)
{
    char *tok;
    int fails = 0;

    assert(str);

    dprintf("Audio config+ : [%s]\n", str);
    tok = "samplerate";
    if (str_parse_tok(str, tok, &cfg->samplerate) <= 0) {
        cfg->samplerate = AUDIO_RATE_DEFAULT;
        fails++;
    } else {
        dprintf("%s :ok\n", tok);
    }
    tok = "volume";
    if (str_parse_tok(str, tok, &cfg->volume) <= 0) {
        cfg->volume = AUDIO_VOLUME_DEFAULT;
        fails++;
    } else {
        dprintf("%s :ok\n", tok);
    }
    tok = "samplebits";
    if (str_parse_tok(str, tok, &cfg->samplebits) <= 0) {
        cfg->samplebits = AUDIO_SAMPLEBITS_DEFAULT;
        fails++;
    } else {
        dprintf("%s :ok\n", tok);
    }
    tok = "cnannels";
    if (str_parse_tok(str, tok, &cfg->channels) <= 0) {
        cfg->channels = AUDIO_CHANNELS_NUM_DEFAULT;
        fails++;
    } else {
        dprintf("%s :ok\n", tok);
    }
    a_hal_check_cfg(cfg);
    dprintf("Audio config-\n");
}

static void
a_isr_clear_all (void)
{
    g_audio_isr_status = A_ISR_NONE;
    g_audio_isr_pend[A_ISR_HALF] = 0;
    g_audio_isr_pend[A_ISR_COMP] = 0;
}

void audio_update_isr (audio_t *audio)
{
    irqmask_t irq_flags = audio->irq_mask;

    irq_save(&irq_flags);
    if (audio->force_stop) {
        a_clear_master();
        audio->force_stop = d_false;
    }
    cd_tickle();
    irq_restore(irq_flags);
}

static void audio_update_dsr (audio_t *audio)
{
    irqmask_t irq_flags = audio->irq_mask;

    irq_save(&irq_flags);
    if (g_audio_isr_status) {
        a_paint_all(audio, d_false, g_audio_isr_pend);
    }
    a_isr_clear_all();
    irq_restore(irq_flags);

    if (audio->force_stop) {
        a_clear_master();
        audio->force_stop = d_false;
    }
    cd_tickle();
}

void audio_update (void)
{
    if (g_audio_proc_isr) {
        audio_update_isr(audio);
    } else {
        audio_update_dsr(audio);
    }
}

void a_dsr_hung_fuse (audio_t *audio, isr_status_e status)
{
    if (audio->head.size == 0) {
        return;
    }
    if ((g_audio_isr_pend[status] & 0xff) == 0xff) {
        dprintf("audio_main.c, __DMA_on_tx_complete : g_audio_isr_pend[ %s ]= %d\n",
                status == A_ISR_HALF ? "A_ISR_HALF" :
                status == A_ISR_COMP ? "A_ISR_COMP" : "?", g_audio_isr_pend[status]);
    } else if (g_audio_isr_pend[status] == 2) {
        a_clear_master();
    }
}

void audio_irq_save (irqmask_t *flags)
{
    *flags = audio->irq_mask;
    irq_save(flags);
}

void audio_irq_restore (irqmask_t flags)
{
    irq_restore(flags);
}

int audio_init (void)
{
    audio = (audio_t *)heap_malloc(sizeof(*audio));
    assert(audio);

    d_memzero(audio, sizeof(*audio));

    audio->head.empty_handle = a_chanlist_empty_clbk;
    audio->head.first_link_handle = a_chanlist_first_node_clbk;
    audio->head.remove_handle = a_chanlist_node_remove_clbk;

    a_mem_init();
    cd_init();
#if (USE_REVERB)
    a_rev_init();
#endif
    return 0;
}

int audio_conf (const char *str)
{
    a_parse_config(&audio->config, str);
    a_hal_configure(audio);
    return 0;
}

a_intcfg_t *audio_current_config (void)
{
    return &audio->config;
}

void audio_deinit (void)
{
    dprintf("%s() :\n", __func__);
    audio_stop_all();
    a_hal_deinit();
    /*TODO : use deinit..*/
    cd_init();
    a_mem_deinit();
    heap_free(audio);
    audio = NULL;
}

audio_channel_t *audio_play_channel (Mix_Chunk *chunk, int channel)
{
    irqmask_t irq_flags = audio->irq_mask;
    audio_channel_t *ch = NULL;

    irq_save(&irq_flags);
    if (!CHANNEL_NUM_VALID(channel)) {
        irq_restore(irq_flags);
        return NULL;
    }
    ch = a_channel_insert(audio, chunk, channel);
    irq_restore(irq_flags);
    return ch;
}

void audio_stop_all (void)
{
    a_channel_t *cur, *next;
    irqmask_t irq_flags = audio->irq_mask;
    irq_save(&irq_flags);
    a_chan_foreach_safe(&audio->head, cur, next) {
        a_channel_remove(audio, cur);
    }
    irq_restore(irq_flags);
    a_clear_master();
}

void audio_mixer_ext (void (*mixer_callback) (int, void *, int, void *))
{
    irqmask_t irq_flags = audio->irq_mask;
    irq_save(&irq_flags);
    audio->amixer_callback = mixer_callback;
    irq_restore(irq_flags);
}

void audio_stop_channel (int channel)
{
    irqmask_t irq_flags = audio->irq_mask;
    irq_save(&irq_flags);
    audio_pause(channel);
    irq_restore(irq_flags);
}

void audio_pause (int channel)
{
    irqmask_t irq_flags = audio->irq_mask;
    irq_save(&irq_flags);

    if (!CHANNEL_NUM_VALID(channel)) {
        irq_restore(irq_flags);
        return;
    }
    if (a_chn_play(&audio->pool[channel])) {
        a_channel_remove(audio, &audio->pool[channel]);
    }
    irq_restore(irq_flags);
}

void audio_resume (void)
{}

int audio_is_playing (int handle)
{
    if (!CHANNEL_NUM_VALID(handle)) {
        return 0;
    }
    return a_chn_play(&audio->pool[handle]);
}

int audio_chk_priority (int priority)
{
    a_channel_t *cur, *next;
    int id = 0;
    irqmask_t irq_flags = audio->irq_mask;
    irq_save(&irq_flags);

    a_chan_foreach_safe(&audio->head, cur, next) {

        if (!a_chn_play(cur)) {
            irq_restore(irq_flags);
            return id;
        }
        if (priority >= 0 && (!priority || cur->priority >= priority)) {
            irq_restore(irq_flags);
            return id;
        }
        id++;
    }
    irq_restore(irq_flags);
    return -1;
}


void audio_set_pan (int handle, int l, int r)
{
    irqmask_t irq_flags = audio->irq_mask;
    irq_save(&irq_flags);

    if (!CHANNEL_NUM_VALID(handle)) {
        irq_restore(irq_flags);
        return;
    }
    if (a_chn_play(&audio->pool[handle])) {
#if USE_STEREO
        a_chunk_vol(&channels[handle]) = ((l + r)) & MAX_VOL;
        channels[handle].left = (uint8_t)l << 1;
        channels[handle].right = (uint8_t)r << 1;
        channels[handle].volume = a_chunk_vol(&channels[handle]);
#else
        a_chunk_vol(&audio->pool[handle]) = ((l + r)) & MAX_VOL;
        audio->pool[handle].volume = a_chunk_vol(&audio->pool[handle]);
#endif
    }
    irq_restore(irq_flags);
}

void audio_sample_vol (audio_channel_t *achannel, uint8_t volume)
{
    irqmask_t irq_flags = audio->irq_mask;

    irq_save(&irq_flags);
    a_channel_t *desc = container_of(achannel, a_channel_t, inst);
    desc->volume = volume & MAX_VOL;
    irq_restore(irq_flags);
}

#else /*AUDIO_MODULE_PRESENT*/

void audio_init (void)
{}

void audio_deinit (void)
{}

void audio_mixer_ext (void (*mixer_callback) (int, void *, int, void *))
{
    audio->amixer_callback = mixer_callback;
}

audio_channel_t *
audio_play_channel (Mix_Chunk *chunk, int channel)
{
    return NULL;
}

void audio_stop_all (void)
{}

audio_channel_t *
audio_stop_channel (int channel)
{
    return NULL;
}

void
audio_pause (int channel)
{}

int
audio_is_playing (int handle)
{
    return 0;
}

int
audio_chk_priority (int handle)
{}


void
audio_set_pan (int handle, int l, int r)
{

}

void
audio_update (void)
{

}

#endif /*AUDIO_MODULE_PRESENT*/
