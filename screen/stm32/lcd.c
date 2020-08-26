/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <debug.h>
#include <heap.h>

#include <gfx.h>
#include <gfx2d_mem.h>
#include <gui.h>
#include <lcd_main.h>
#include "../../gui/colors.h"
#include <../../../common/int/mpu.h>

#include <bsp_sys.h>
#include <bsp_cmd.h>
#include <dev_io.h>
#include <smp.h>
#include "../../../common/int/lcd_int.h"

static void screen_copy_1x1_SW (screen_t *in);
static void screen_copy_1x1_HW (screen_t *in);
static void screen_copy_2x2_HW (screen_t *in);
static void screen_copy_2x2_8bpp (screen_t *in);
static void screen_copy_2x2_8bpp_filt (screen_t *in);
static void screen_copy_3x3_8bpp (screen_t *in);

uint32_t bsp_lcd_width = (uint32_t)-1;
uint32_t bsp_lcd_height = (uint32_t)-1;

static lcd_t lcd_def_cfg;
lcd_t *g_lcd_inst = NULL;

static const char *screen_mode2txt_map[] =
{
    "***INVALID***",
    "*CLUT L8*",
    "*RGB565*",
    "*ARGB8888*",
};

const uint32_t screen_mode2pixdeep[GFX_COLOR_MODE_MAX] =
{
    0, 1, 2, 4,
};

static int vid_gfx_filter_scale = 0;

int vid_init (void)
{
    cmd_register_i32(&vid_gfx_filter_scale, "vidfilt");
    return screen_hal_init(1);
}

static void vid_mpu_release (lcd_t *lcd)
{
    if (lcd->fb.buf) {
        mpu_unlock((arch_word_t)lcd->fb.buf, lcd->fb.bytes_total);
    }
}

static void vid_mpu_create (lcd_t *lcd, size_t *fb_size)
{
    int i;
    const char *mpu_conf = NULL;

    switch (lcd->config.cachealgo) {
        case VID_CACHE_NONE:
            /*Non-cacheable*/
            mpu_conf = "-c";
        break;
        case VID_CACHE_WTWA:
            /*Write through, no write allocate*/
            mpu_conf = "c";
        break;
        case VID_CACHE_WBNWA:
            /*Write-back, no write allocate*/
            mpu_conf = "bc";
        break;
    }

    i = *fb_size;
    if (mpu_conf && mpu_lock((arch_word_t)lcd->fb.buf, fb_size, mpu_conf) < 0) {
        dprintf("%s() : MPU region config failed\n", __func__);
    } else if (i != *fb_size) {

        assert(i < *fb_size);
        i = *fb_size - i;
        dprintf("%s() : MPU region requires extra space(padding) + %d bytes\n",
                __func__, i);
    }
}

static void vid_release (lcd_t *lcd)
{
    if (NULL == lcd) {
        return;
    }
    if (lcd->fb.buf) {
        d_memzero(lcd->fb.buf, lcd->fb.bytes_total);
        heap_free(lcd->fb.buf);
    }
    vid_mpu_release(lcd);
    d_memzero(lcd, sizeof(*lcd));
}

void vid_deinit (void)
{
    lcd_t *lcd = LCD();

    dprintf("%s() :\n", __func__);
    screen_hal_init(0);
    vid_release(lcd);
    g_lcd_inst = NULL;
}

int vid_set_keying (uint32_t color)
{
    lcd_t *lcd = LCD();

    screen_hal_set_keying(lcd, color);
    return 0;
}

void vid_wh (screen_t *s)
{
    lcd_t *lcd = LCD();

    if (!lcd) {
        s->width = bsp_lcd_width;
        s->height = bsp_lcd_height;
    } else {
        s->width = lcd->fb.w;
        s->height = lcd->fb.h;
    }
}

static void *
vid_create_framebuffer (screen_alloc_t *alloc, lcd_t *lcd,
                       uint32_t w, uint32_t h, uint32_t pixel_deep)
{
    const uint32_t lay_mem_align = 64;
    const uint32_t lay_mem_size = (w * h * pixel_deep) + lay_mem_align;
    uint32_t lay_size;
    uint32_t fb_size;
    uint8_t *fb_mem;
    int ext_frame = hal_smp_present();

    lay_size += lay_mem_size + lay_mem_align;
    if (ext_frame) {
        fb_size = lay_size * (2);
    } else {
        fb_size = lay_size;
    }
    if (lcd->fb.buf) {
        assert(0);
    }

    fb_size = mpu_roundup(fb_size);

    fb_mem = (uint8_t *)alloc->malloc(fb_size);
    if (!fb_mem) {
        dprintf("%s() : failed to allocate %u bytes\n", __func__, fb_size);
        return NULL;
    }
    /*To prevent lcd panel 'in-burning', or 'ghosting' etc.. -
      lets fill fb with white color
    */
    d_memset(fb_mem, COLOR_WHITE, fb_size);

    lcd->fb.buf = fb_mem;
    lcd->fb.bytes_total = fb_size;
    lcd->fb.w = w;
    lcd->fb.h = h;

    lcd->fb.frame_ext = NULL;

    lcd->fb.frame = (uint8_t *)(ROUND_UP((arch_word_t)fb_mem, lay_mem_align));
    fb_mem = fb_mem + lay_size;

    if (ext_frame) {
        lcd->fb.frame_ext = fb_mem;
    }
    vid_mpu_create(lcd, &fb_size);
    return lcd->fb.buf;
}

void vid_ptr_align (int *x, int *y)
{
}

static screen_update_handler_t vid_get_scaler (int scale, uint8_t colormode)
{
    lcd_t *lcd = LCD();
    screen_update_handler_t h = NULL;
    screen_conf_t *conf = &lcd->config;

    if (colormode == GFX_COLOR_MODE_CLUT) {
        switch (scale) {
            case 1:
                if (conf->hwaccel) {
                    h = screen_copy_1x1_HW;
                } else {
                    h = screen_copy_1x1_SW;
                }
            case 2:
                if (conf->hwaccel) {
                    h = screen_copy_2x2_HW;
                } else {
                    if (conf->use_clut) {
                        h = screen_copy_2x2_8bpp_filt;
                    } else {
                        h = screen_copy_2x2_8bpp;
                    }
                }
            break;
            case 3:
                h = screen_copy_3x3_8bpp;
            break;

            default:
                fatal_error("%s() : Scale not supported yet!\n", __func__);
            break;
        }
    } else {
        switch (scale) {
            default:
                h = screen_copy_1x1_SW;
            break;
        }
    }
    return h;
}

static lcd_t *vid_new_winconfig (lcd_t *cfg)
{
    if (!cfg) {
        cfg = &lcd_def_cfg;
        d_memset(&lcd_def_cfg, 0, sizeof(lcd_def_cfg));
    }
    return cfg;
}

static int vid_set_win_size (int screen_w, int screen_h, int lcd_w, int lcd_h, int *x, int *y, int *w, int *h)
{
    int scale = -1;
    int win_w = screen_w > 0 ? screen_w : lcd_w,
        win_h = screen_h > 0 ? screen_h : lcd_h;

    int sw = lcd_w / win_w;
    int sh = lcd_h / win_h;

    if (sw < sh) {
        sh = sw;
    } else {
        sw = sh;
    }
    scale = sw;
    if (scale > LCD_MAX_SCALE) {
        scale = LCD_MAX_SCALE;
    }

    *w = win_w * scale;
    *h = win_h * scale;
    *x = (lcd_w - *w) / scale;
    *y = (lcd_h - *h) / scale;
    return scale;
}

int vid_config (screen_conf_t *conf)
{
    lcd_t *lcd = LCD();

    uint32_t scale;
    int x, y, w, h;

    lcd = vid_new_winconfig(lcd);
    if ((lcd_t *)lcd == g_lcd_inst) {
        return 0;
    }
    d_memzero(lcd, sizeof(*lcd));
    g_lcd_inst = lcd;

    lcd->config = *conf;
    scale = vid_set_win_size(conf->res_x, conf->res_y, bsp_lcd_width, bsp_lcd_height, &x, &y, &w, &h);

    lcd->scaler = vid_get_scaler(scale, conf->colormode);

    lcd_x_size_var = w;
    lcd_y_size_var = h;

    if (conf->laynum) {
        dprintf("%s(): Deprecated conf->laynum!\n", __func__);
    }
    assert(screen_mode2pixdeep[conf->colormode]);
    if (!vid_create_framebuffer(&conf->alloc, lcd, w, h, screen_mode2pixdeep[conf->colormode])) {
        return -1;
    }

    if (NULL == screen_hal_set_config(lcd, x, y, w, h, conf->colormode)) {
        return -1;
    }

    cmd_register_i32(&lcd->cvar_have_smp, "vid_have_smp");
    lcd->cvar_have_smp = 0;

    return 0;
}

uint32_t vid_mem_avail (void)
{
    lcd_t *lcd = LCD();

    assert(lcd);
    return lcd->fb.bytes_total;
}

void vid_vsync (int unused)
{
    lcd_t *lcd = LCD();

    profiler_enter();
    screen_hal_sync(lcd);
    profiler_exit();
}

void vid_get_screen (screen_t *screen, int unused)
{
    lcd_t *lcd = LCD();

    screen->width = lcd->fb.w;
    screen->height = lcd->fb.h;
    screen->buf = lcd->fb.frame;
    screen->x = 0;
    screen->y = 0;
    screen->colormode = lcd->config.colormode;
    screen->alpha = 0xff;
}

void vid_get_ready_screen (screen_t *screen)
{
    vid_get_screen(screen, 0);
}

static int
vid_get_pal8_idx (rgba_t *pal, int palnum, int r, int g, int b)
{
    uint32_t best, best_diff, diff;
    int i;
    uint8_t pr, pg, pb;

    best = 0;
    best_diff = ~0;

    for (i = 0; i < palnum; ++i)
    {
        pr = GFX_ARGB8888_R(pal[i]);
        pg = GFX_ARGB8888_G(pal[i]);
        pb = GFX_ARGB8888_B(pal[i]);
        diff = (r - pr) * (r - pr)
             + (g - pg) * (g - pg)
             + (b - pb) * (b - pb);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}


static inline
rgba_t vid_blend (rgba_t fg, rgba_t bg, uint8_t a)
{
#define __blend(f, b, a) (uint8_t)(((uint16_t)(f * a) + (uint16_t)(b * (255 - a))) / 255)
    rgba_t ret;

    uint8_t fg_r = GFX_ARGB8888_R(fg);
    uint8_t fg_g = GFX_ARGB8888_G(fg);
    uint8_t fg_b = GFX_ARGB8888_B(fg);

    uint8_t bg_r = GFX_ARGB8888_R(bg);
    uint8_t bg_g = GFX_ARGB8888_G(bg);
    uint8_t bg_b = GFX_ARGB8888_B(bg);

    uint8_t r = __blend(fg_r, bg_r, a);
    uint8_t g = __blend(fg_g, bg_g, a);
    uint8_t b = __blend(fg_b, bg_b, a);

    ret = GFX_ARGB8888(r, g, b, 0xff);
    return ret;
}

static inline uint8_t
vid_blend8 (rgba_t *pal, int palnum, uint8_t _fg, uint8_t _bg, uint8_t a)
{
    rgba_t fg = pal[_fg];
    rgba_t bg = pal[_bg];
    rgba_t pix = vid_blend(fg, bg, a);

    return vid_get_pal8_idx(pal, palnum, GFX_ARGB8888_R(pix), GFX_ARGB8888_G(pix), GFX_ARGB8888_B(pix));
}

static size_t vid_gen_blut8 (lcd_t *cfg, void *palette, int numentries)
{
    int i, j;
    blut8_t *blut;
    rgba_t clut[256];

    assert(arrlen(clut) >= numentries);

    cfg->blut = cfg->config.alloc.malloc(sizeof(blut8_t));
    if (NULL == cfg->blut) {
        return 0;
    }
    blut = (blut8_t *)((arch_word_t)cfg->blut);

    d_memcpy(clut, palette, numentries * sizeof(rgba_t));

    for (i = 0; i < numentries; i++) {
        for (j = 0; j < numentries; j++) {
            if (i == j) {
                blut->lut[i][j] = i;
            } else {
                blut->lut[i][j] = vid_blend8(clut, numentries, i, j, 128);
            }
        }
    }
    return sizeof(blut8_t);
}

void vid_set_clut (void *palette, uint32_t clut_num_entries)
{
    lcd_t *lcd = LCD();

    assert(lcd);
    screen_hal_sync(lcd);
    if (lcd->config.use_clut && NULL == lcd->blut) {
        vid_gen_blut8(lcd, palette, clut_num_entries);
    }
    screen_hal_set_clut (lcd, palette, clut_num_entries);
}

static void vid_setup_filter (lcd_t *lcd)
{
    if (lcd->scaler == screen_copy_2x2_8bpp &&
        vid_gfx_filter_scale) {

        lcd->scaler = screen_copy_2x2_8bpp_filt;
    } else if (lcd->scaler == screen_copy_2x2_8bpp_filt &&
        !vid_gfx_filter_scale) {

        lcd->scaler = screen_copy_2x2_8bpp;
    }
}

void vid_update (screen_t *in)
{
    lcd_t *lcd = LCD();

    if (in == NULL) {
        vid_vsync(1);
    } else {
        in->colormode = lcd->config.colormode;
        in->alpha = 0xff;
        vid_setup_filter(lcd);
        lcd->scaler(in);
    }
}

void vid_direct_copy (gfx_2d_buf_t *dest2d, gfx_2d_buf_t *src2d)
{
    screen_t dest_s, src_s;
    __gfx2d_to_screen(&dest_s, dest2d);
    __gfx2d_to_screen(&src_s, src2d);
    vid_copy(&dest_s, &src_s);
}

void vid_direct (int x, int y, screen_t *s, int laynum)
{
    screen_t screen;

    vid_vsync(1);
    if (laynum < 0) {
        vid_get_ready_screen(&screen);
    } else {
        vid_get_screen(&screen, laynum);
    }
    screen.x = x;
    screen.y = y;
    screen.alpha = 0xff;
    screen.colormode = s->colormode;
    vid_copy(&screen, s);
}

void vid_print_info (void)
{
    lcd_t *lcd = LCD();

    assert(lcd);

    dprintf("\n");
    dprintf("Video+ :\n");
    dprintf("width=%4.3u height=%4.3u\n", lcd->fb.w, lcd->fb.h);
    dprintf("layers = %u, color mode = %s \n",
             lcd->config.laynum, screen_mode2txt_map[lcd->config.colormode]);
    dprintf("framebuffer = <0x%p> 0x%08x bytes\n", lcd->fb.buf, lcd->fb.bytes_total);
    dprintf("Video-\n");
}

void vid_refresh_direct (void)
{
}

static int
vid_copy_HW (screen_t *dest, screen_t *src)
{
    lcd_t *lcd = LCD();
    copybuf_t copybuf = {NULL, *dest, *src};
    uint8_t colormode = lcd->config.colormode;

    vid_vsync(0);
    assert(screen_mode2pixdeep[colormode]);
    return screen_hal_copy_m2m(lcd, &copybuf, screen_mode2pixdeep[colormode]);
}

int vid_copy (screen_t *dest, screen_t *src)
{
    return vid_copy_HW(dest, src);
}

int vid_copy_line_8b (void *dest, void *src, int w)
{
    lcd_t *lcd = LCD();
    return screen_gfx8_copy_line(lcd, dest, src, w);
}

int vid_gfx2d_direct (int x, int y, gfx_2d_buf_t *src, int laynum)
{
    lcd_t *lcd = LCD();
    gfx_2d_buf_t dest;
    screen_t screen;

    vid_vsync(1);
    vid_get_ready_screen(&screen);

    screen.x = x;
    screen.y = y;
    __screen_to_gfx2d(&dest, &screen);
    return screen_gfx8888_copy(lcd, &dest, src);
}

static void
screen_copy_1x1_SW (screen_t *in)
{
    screen_t screen;

    vid_vsync(1);
    vid_get_ready_screen(&screen);
    vid_copy(&screen, in);
}

static void
screen_copy_1x1_HW (screen_t *in)
{
    lcd_t *lcd = LCD();

    screen_t screen;
    copybuf_t copybuf = {NULL};

    vid_get_ready_screen(&screen);
    vid_vsync(1);
    copybuf.dest = screen;
    copybuf.src = *in;

    screen_hal_copy_m2m(lcd, &copybuf, 1);
}

static void
screen_copy_2x2_HW (screen_t *in)
{
    lcd_t *lcd = LCD();
    screen_t screen;
    copybuf_t copybuf;

    vid_get_ready_screen(&screen);
    vid_vsync(1);

    copybuf.dest = screen;
    copybuf.src = *in;

    copybuf.dest.colormode = lcd->config.colormode;
    copybuf.src.colormode = lcd->config.colormode;
    screen_hal_scale_h8_2x2(lcd, &copybuf, lcd->config.hwaccel > 1);
}

static hal_smp_task_t *task = NULL;

static void
screen_copy_2x2_8bpp_task (void *arg)
{
    gfx_2d_buf_pair_t *buf = (gfx_2d_buf_pair_t *)arg;
    gfx2d_scale2x2_8bpp(&buf->dest, &buf->src);
}

static void
screen_copy_2x2_8bpp (screen_t *in)
{
    lcd_t *lcd = LCD();
    screen_t screen;
    gfx_2d_buf_t dest, src;

    vid_vsync(1);
    vid_get_ready_screen(&screen);
    __screen_to_gfx2d(&dest, &screen);
    __screen_to_gfx2d(&src, in);

    if (!lcd->cvar_have_smp) {
        gfx2d_scale2x2_8bpp(&dest, &src);
    } else {
        gfx_2d_buf_pair_t arg = {dest, src};
        int hsem = hal_smp_hsem_alloc("hsem_task");

        assert(lcd->fb.frame_ext);

        if (task) {
            hal_smp_sync_task(task);
            hal_smp_free_task(task);
        }

        d_memcpy(lcd->fb.frame_ext, in->buf, in->width * in->height);
        arg.src.buf = lcd->fb.frame_ext;

        hal_smp_hsem_spinlock(hsem);
        task = hal_smp_sched_task(screen_copy_2x2_8bpp_task, &arg, sizeof(arg));
        hal_smp_hsem_release(hsem);
    }
}

static void
screen_copy_2x2_8bpp_filt (screen_t *in)
{
    lcd_t *lcd = LCD();
    screen_t screen;
    gfx_2d_buf_t dest, src;

    if (NULL == lcd->blut) {
        screen_copy_2x2_8bpp(in);
        return;
    }
    vid_get_ready_screen(&screen);
    vid_vsync(1);
    __screen_to_gfx2d(&dest, &screen);
    __screen_to_gfx2d(&src, in);
    if (lcd->bilinear) {
        gfx2d_scale2x2_8bpp_filt_Bi((blut8_t *)lcd->blut, &dest, &src);
    } else {
        gfx2d_scale2x2_8bpp_filt((blut8_t *)lcd->blut, &dest, &src);
    }
}


static void
screen_copy_3x3_8bpp(screen_t *in)
{
    screen_t screen;
    gfx_2d_buf_t dest, src;

    vid_vsync(1);
    vid_get_ready_screen(&screen);

    __screen_to_gfx2d(&dest, &screen);
    __screen_to_gfx2d(&src, in);
    gfx2d_scale3x3_8bpp(&dest, &src);
}

int vid_priv_ctl (int c, void *v)
{
    lcd_t *lcd = LCD();

    switch (c) {
        case LCD_PRIV_GET_TRANSP_LUT:
            if (lcd->blut) {
                blut8_t *blut = (blut8_t *)lcd->blut;
                *(blut8_t **)v = blut;
            } else {
                return -CMDERR_NOCORE;
            }
        break;
        default :
            return -CMDERR_NOCORE;
    }
    return 0;
}

