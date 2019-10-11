#include <stdint.h>
#include <stddef.h>
#include "heap.h"
#include "audio_main.h"
#include "int/audio_int.h"

#if AUDIO_MODULE_PRESENT

#if defined(BSP_DRIVER)

typedef struct {
    snd_sample_t *buf;
    int samples;
    d_bool dirty;
} a_master_t;

static snd_sample_t master_buf_raw[2][AUDIO_OUT_BUFFER_SIZE] ALIGN(8);
a_master_t master_track[2];
snd_sample_t *master_base_raw;
size_t master_base_samples;

int
a_channel_link (a_channel_head_t *head, a_channel_t *link, uint8_t sort)
{
    a_channel_t *cur, *next;

    head->size++;
    if (head->first == NULL) {
        head->first = link;
        head->last = link;
        link->next = NULL;
        link->prev = NULL;
        if (head->first_link_handle)
            head->first_link_handle(head);
        return head->size;
    }
    a_chan_foreach_safe(head, cur, next) {
        if ((a_chunk_len(link) < a_chunk_len(cur))) {
            if (cur->prev != NULL) {
                cur->prev->next = link;
                link->next = cur;
                link->prev = cur->prev;
                cur->prev = link;
                return head->size;
            }
            link->next = cur;
            link->prev = NULL;
            cur->prev = link;
            head->first = link;
            return head->size;
         }
    }
    link->prev = head->last;
    link->next = NULL;
    head->last->next = link;
    head->last = link;
    return head->size;
}

int
a_channel_unlink (a_channel_head_t *head, a_channel_t *node)
{
    if (!head->size) {
        return 0;
    }
    head->size--;
    a_channel_t *prev = node->prev, *next = node->next;
    if (!head->size) {
        head->first = NULL;
        head->last = NULL;

        if (head->remove_handle)
            head->remove_handle(head, node);

        if (head->empty_handle)
            head->empty_handle(head);
        return 0;
    }
    if (!prev) {
        head->first = next;
        next->prev = NULL;
    } else if (next) {
        prev->next = next;
        next->prev = prev;
    } else if (!next) {
        prev->next = NULL;
        head->last = prev;
    }
    if (head->remove_handle)
            head->remove_handle(head, node);

    return head->size;
}

void
a_mem_init (void)
{
    master_track[0].buf = master_buf_raw[0];
    master_track[0].samples = AUDIO_OUT_BUFFER_SIZE;
    master_track[0].dirty = d_false;

    master_track[1].buf = master_buf_raw[1];
    master_track[1].samples = AUDIO_OUT_BUFFER_SIZE;
    master_track[1].dirty = d_false;

    master_base_raw = master_buf_raw[0];
    master_base_samples = AUDIO_OUT_BUFFER_SIZE * 2;
}

void
a_get_master_base (a_buf_t *master)
{
    master->buf = master_base_raw;
    master->samples = master_base_samples;
}

void
a_get_master4idx (a_buf_t *master, int idx)
{
    master->buf = master_track[idx].buf;
    master->samples = master_track[idx].samples;
    master->dirty = &master_track[idx].dirty;
}

void a_clear_abuf (a_buf_t *abuf)
{
    uint64_t *p_buf = (uint64_t *)abuf->buf;
    if (*abuf->dirty == d_false)
        return;

    for (int i = 0; i < AUDIO_SAMPLES_2_DWORDS(abuf->samples); i++) {
        p_buf[i] = 0;
    }
    *abuf->dirty = d_false;
}

void a_clear_master (void)
{
    uint64_t *p_buf = (uint64_t *)master_base_raw;

    for (int i = 0; i < AUDIO_SAMPLES_2_DWORDS(master_base_samples); i++) {
        p_buf[i] = 0;
    }
    master_track[0].dirty = d_false;
    master_track[1].dirty = d_false;
}
#endif

#endif /*AUDIO_MODULE_PRESENT*/
