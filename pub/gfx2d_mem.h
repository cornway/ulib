#ifndef __GFX_2D_MEM_H__
#define __GFX_2D_MEM_H__

typedef struct {
    void *buf;
    int x, y;
    int width, height;
    uint8_t colormode;
    uint8_t alpha;
} screen_t;

typedef struct {
    void *buf;
    int x, y;
    int w, h;
    int wtotal, htotal;
} gfx_2d_buf_t;

typedef struct {
    uint8_t lut[256][256];
} blut8_t;

static inline void
__screen_to_gfx2d (gfx_2d_buf_t *g, screen_t *s)
{
    g->buf = s->buf;
    g->x = s->x;
    g->y = s->y;
    g->w = s->width;
    g->wtotal = s->width;
    g->h = s->height;
    g->htotal = s->height;
}

static inline void
__gfx2d_to_screen (screen_t *s, gfx_2d_buf_t *g)
{
    s->buf = g->buf;
    s->x = g->x;
    s->y = g->y;
    s->width = g->wtotal;
    s->height = g->htotal;
}

void
gfx2d_scale2x2_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src);

void
gfx2d_scale3x3_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src);

void gfx2d_copy (gfx_2d_buf_t *dest2d, gfx_2d_buf_t *src2d);

#endif /*__GFX_2D_MEM_H__*/

