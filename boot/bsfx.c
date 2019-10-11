#include <string.h>

#include <debug.h>
#include "int/boot_int.h"

#include <misc_utils.h>
#include <dev_io.h>
#include <bconf.h>
#include <audio_main.h>

#define SFX_MAX_NAME 8

typedef struct {
    const char *name;
    char filename[SFX_MAX_NAME];
    int sfx_id;
} sfx_map_t;

static sfx_map_t sfx_map[] =
{
    [SFX_MOVE]         = {"sfxmove.wav", "\0", -1},
    [SFX_SCROLL]       = {"sfxscroll.wav", "\0", -1},
    [SFX_WARNING]      = {"sfxwarn.wav", "\0", -1},
    [SFX_CONFIRM]      = {"sfxaccpt.wav", "\0", -1},
    [SFX_START_APP]    = {"sfxstrt2.wav", "\0", -1},
    [SFX_NOWAY]        = {"sfxnoway.wav", "\0", -1},
};

static cd_track_t boot_cd = {NULL};

static inline sfx_map_t *
bsfx_map_4_name (sfx_map_t *map, int mapsize, const char *name)
{
    int i;

    for (i = 0; i < mapsize; i++) {
        if (!strcmp(map[i].name, name)) {
            return &map[i];
        }
    }
    return NULL;
}

static void bsfx_read_sfx_map (const char *path, sfx_map_t *map, int mapsize)
{
    int f, tokc = 2;
    char buf[SFX_MAX_NAME * 2 + 4], *p;
    const char *tok[2];
    sfx_map_t *sfx;

    d_open(path, &f, "r");
    if (f < 0) {
        return;
    }
    while (!d_eof(f)) {
        p = d_gets(f, buf, sizeof(buf));
        if (!p) {
            continue;
        }
        tokc = d_vstrtok(tok, 2, buf, '=');
        if (tokc < 2) {
            continue;
        }
        sfx = bsfx_map_4_name(map, mapsize, tok[0]);
        if (!sfx) {
            continue;
        }
        snprintf(sfx->filename, sizeof(sfx->filename), "%s", tok[1]);
    }
    d_close(f);
}

void bsfx_sound_precache (void (*statfunc) (const char *, int), int prev_per)
{
    int i;
    char path[BOOT_MAX_PATH];
    const char *name;

    snprintf(path, sizeof(path), "%s/%s", BOOT_SFX_DIR_PATH, "sfxconf.cfg");
    bsfx_read_sfx_map(path, sfx_map, arrlen(sfx_map));

    for (i = 0; i < arrlen(sfx_map); i++) {
        name = (*sfx_map[i].filename) ? sfx_map[i].filename : sfx_map[i].name;

        snprintf(path, sizeof(path), "%s/%s", BOOT_SFX_DIR_PATH, name);
        sfx_map[i].sfx_id = bsp_open_wave_sfx(path);
        if (statfunc) {
            statfunc(path, ++prev_per);
        }
    }
}

void bsfx_sound_free (void)
{
    int i;

    for (i = 0; i < arrlen(sfx_map); i++) {
        bsp_release_wave_sfx(sfx_map[i].sfx_id);
        sfx_map[i].sfx_id = -1;
    }
}

void bsfx_start_sound (int num, uint8_t volume)
{
    bsp_play_wave_sfx(num, volume);
}

void bsfx_title_music (int play, uint8_t volume)
{
    if (!play) {
        cd_stop(&boot_cd);
    } else {
        cd_play_name(&boot_cd, BOOT_STARTUP_MUSIC_PATH);
        cd_volume(&boot_cd, volume);
    }
}

