
#include <gfx.h>
#include <gfx2d_mem.h>
#include <misc_utils.h>
#include <heap.h>

typedef uint8_t pix8_t;

void gfx2d_copy (gfx_2d_buf_t *dest2d, gfx_2d_buf_t *src2d)
{
    screen_t dest_s, src_s;
    __gfx2d_to_screen(&dest_s, dest2d);
    __gfx2d_to_screen(&src_s, src2d);
    vid_copy(&dest_s, &src_s);
}

typedef struct {
    pix8_t a[4];
} scanline8_t;

typedef union {
    uint32_t w;
    scanline8_t sl;
} scanline8_u;

#define GFXBUF_BYTES_STEP_8bpp (sizeof(scanline8_t) / sizeof(pix8_t))
#define GFXBUF_NEXT_SCALED_LINE_8bpp(x, w, lines) (((x) + (lines) * ((w) / sizeof(scanline8_t))))

typedef struct {
    scanline8_u a[2];
} pix_outx2_t;

IRAMFUNC static void
__gfx2d_scale2x2_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src);

static void
__gfx2d_scale2x2_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    scanline8_u d_yt0, d_yt1;
    scanline8_t *scanline;
    pix_outx2_t *d_y0;
    pix_outx2_t *d_y1;
    pix_outx2_t pix;
    pix8_t *rawptr;
    int s_y, i;

    rawptr = (pix8_t *)src->buf;

    d_y0 = (pix_outx2_t *)dest->buf;
    d_y1 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y0, src->wtotal, 1);

    for (s_y = 0; s_y < (src->wtotal * src->htotal); s_y += src->wtotal) {

        scanline = (scanline8_t *)&rawptr[s_y];

        for (i = 0; i < src->w; i += GFXBUF_BYTES_STEP_8bpp) {

            d_yt0.sl = *scanline++;
            d_yt1    = d_yt0;

            d_yt0.sl.a[3] = d_yt0.sl.a[1];
            d_yt0.sl.a[2] = d_yt0.sl.a[1];
            d_yt0.sl.a[1] = d_yt0.sl.a[0];

            d_yt1.sl.a[0] = d_yt1.sl.a[2];
            d_yt1.sl.a[1] = d_yt1.sl.a[2];
            d_yt1.sl.a[2] = d_yt1.sl.a[3];

            pix.a[0] = d_yt0;
            pix.a[1] = d_yt1;

            *d_y0++     = pix;
            *d_y1++     = pix;
        }
        d_y0 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y0, src->wtotal, 1);
        d_y1 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y1, src->wtotal, 1);
    }
}

void
gfx2d_scale2x2_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    if (cs_check_symb(__gfx2d_scale2x2_8bpp)) {
        __gfx2d_scale2x2_8bpp(dest, src);
    }
}

IRAMFUNC static void
__gfx2d_scale2x2_8bpp_F (blut8_t *lut, gfx_2d_buf_t *dest, gfx_2d_buf_t *src);

static void
__gfx2d_scale2x2_8bpp_F (blut8_t *lut, gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    scanline8_u d_yt0, d_yt1, d_yref;
    scanline8_t *scanline, *scanline2;
    pix_outx2_t *d_y0, *d_y1;
    pix_outx2_t pix;
    pix8_t *rawptr, p_yref;
    int s_y, i;

    rawptr = (pix8_t *)src->buf;

    d_y0 = (pix_outx2_t *)dest->buf;
    d_y1 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y0, src->wtotal, 1);

    for (s_y = 0; s_y < (src->wtotal * (src->htotal - 1)); s_y += src->wtotal) {

        scanline = (scanline8_t *)&rawptr[s_y];
        scanline2 = (scanline8_t *)&rawptr[s_y + src->wtotal];

        for (i = 0; i < src->w; i += GFXBUF_BYTES_STEP_8bpp) {

            d_yref.sl = *scanline2++;
            d_yt0.sl = *scanline++;
            p_yref = scanline->a[0];
            d_yt1    = d_yt0;

            d_yt0.sl.a[3] = lut->lut[d_yt1.sl.a[1]][d_yt1.sl.a[2]];
            d_yt0.sl.a[2] = d_yt1.sl.a[1];
            d_yt0.sl.a[1] = lut->lut[d_yt1.sl.a[0]][d_yt1.sl.a[1]];

            d_yt1.sl.a[0] = d_yt1.sl.a[2];
            d_yt1.sl.a[1] = lut->lut[d_yt1.sl.a[2]][d_yt1.sl.a[3]];
            d_yt1.sl.a[2] = d_yt1.sl.a[3];
            d_yt1.sl.a[3] = lut->lut[d_yt1.sl.a[3]][p_yref];


            pix.a[0] = d_yt0;
            pix.a[1] = d_yt1;

            *d_y0++     = pix;

            pix.a[0].sl.a[1] = lut->lut[d_yt0.sl.a[0]][d_yref.sl.a[0]];
            pix.a[0].sl.a[3] = lut->lut[d_yt0.sl.a[2]][d_yref.sl.a[1]];

            pix.a[1].sl.a[1] = lut->lut[d_yt1.sl.a[0]][d_yref.sl.a[2]];
            pix.a[1].sl.a[3] = lut->lut[d_yt1.sl.a[2]][d_yref.sl.a[3]];

            pix.a[0].sl.a[0] = pix.a[0].sl.a[1];
            pix.a[0].sl.a[2] = pix.a[0].sl.a[3];
            pix.a[1].sl.a[0] = pix.a[1].sl.a[1];
            pix.a[1].sl.a[2] = pix.a[1].sl.a[3];

            *d_y1++     = pix;
        }
        d_y0 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y0, src->wtotal, 1);
        d_y1 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y1, src->wtotal, 1);
    }
}

void
gfx2d_scale2x2_8bpp_filt (blut8_t *lut, gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    if (cs_check_symb(__gfx2d_scale2x2_8bpp)) {
        __gfx2d_scale2x2_8bpp_F(lut, dest, src);
    }
}

typedef struct {
    scanline8_u a[3];
} pix_outx3_t;

IRAMFUNC static void
__gfx2d_scale3x3_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src);

static void
__gfx2d_scale3x3_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    scanline8_u d_yt0, d_yt1, d_yt2;
    pix_outx3_t *d_y0, *d_y1, *d_y2;
    scanline8_t *scanline;
    pix_outx3_t pix;
    pix8_t *rawptr;
    int s_y, i;

    rawptr = (pix8_t *)src->buf;
    d_y0 = (pix_outx3_t *)dest->buf;
    d_y1 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y0, src->wtotal, 1);
    d_y2 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y1, src->wtotal, 1);

    for (s_y = 0; s_y < (src->wtotal * src->htotal); s_y += src->wtotal) {

        scanline = (scanline8_t *)&rawptr[s_y];

        for (i = 0; i < src->w; i += GFXBUF_BYTES_STEP_8bpp) {

            d_yt0.sl = *scanline++;
            d_yt1    = d_yt0;
            d_yt2    = d_yt1;

            d_yt2.sl.a[2] = d_yt0.sl.a[3];
            d_yt2.sl.a[1] = d_yt0.sl.a[3];

            d_yt2.sl.a[0] = d_yt0.sl.a[2];
            d_yt1.sl.a[3] = d_yt0.sl.a[2];

            d_yt1.sl.a[0] = d_yt0.sl.a[1];
            d_yt0.sl.a[3] = d_yt0.sl.a[1];

            d_yt0.sl.a[2] = d_yt0.sl.a[0];
            d_yt0.sl.a[1] = d_yt0.sl.a[0];
            pix.a[0] = d_yt0;
            pix.a[1] = d_yt1;
            pix.a[2] = d_yt2;

            *d_y0++     = pix;
            *d_y1++     = pix;
            *d_y2++     = pix;
        }

        d_y0 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y0, src->wtotal, 2);
        d_y1 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y1, src->wtotal, 2);
        d_y2 = GFXBUF_NEXT_SCALED_LINE_8bpp(d_y2, src->wtotal, 2);
    }
}

void
gfx2d_scale3x3_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    if (cs_check_symb(__gfx2d_scale3x3_8bpp)) {
        __gfx2d_scale3x3_8bpp(dest, src);
    }
}

