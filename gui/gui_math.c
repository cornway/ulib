
#include <gui.h>

void dim_extend (dim_t *dest, dim_t *ref)
{
    int box_dest[4] = {dest->x, dest->y, dest->x + dest->w, dest->y + dest->h};
    int box_ref[4] = {ref->x, ref->y, ref->x + ref->w, ref->y + ref->h};
    int durty = 0;

    if (box_ref[0] < box_dest[0]) {
        box_dest[2] += box_dest[0] - box_ref[0];
        box_dest[0] = box_ref[0];
        durty = 1;
    }
    if (box_ref[1] < box_dest[1]) {
        box_dest[2] += box_dest[1] - box_ref[1];
        box_dest[1] = box_ref[1];
        durty = 1;
    }
    if (box_ref[2] > box_dest[2]) {
        box_dest[2] = box_ref[2];
        durty = 1;
    }
    if (box_ref[3] > box_dest[3]) {
        box_dest[3] = box_ref[3];
        durty = 1;
    }
    if (!durty) {
        return;
    }
    dest->x = box_dest[0];
    dest->y = box_dest[1];
    dest->w = box_dest[2] - dest->x;
    dest->h = box_dest[3] - dest->y;
}

d_bool dim_check_intersect (dim_t *top, dim_t *dim)
{
    int tmp,
        max_x = top->x + top->w,
        max_y = top->y + top->h;

    if (!max_x || !max_y) {
        return d_false;
    }
    if (dim->x > max_x ||
        dim->y > max_y) {
        return d_false;
    }
    tmp = (dim->x + dim->w) - top->x;
    if (tmp < 0) {
        return d_false;
    }
    tmp = (dim->y + dim->h) - top->y;
    if (tmp < 0) {
        return d_false;
    }
    return d_true;
}

d_bool dim_check (const dim_t *d, const point_t *p)
{
    int x0 = p->x - d->x;
    if (x0 < 0) {
        return d_false;
    }
    if (x0 > d->w) {
        return d_false;
    }
    int y0 = p->y - d->y;
    if (y0 < 0) {
        return d_false;
    }
    if (y0 > d->h) {
        return d_false;
    }
    return d_true;
}

d_bool dim_check_overlap (const dim_t *d, const dim_t *mask)
{
    if (mask->x > d->x) {
        return d_false;
    }
    if (mask->w + mask->x < d->w + d->x) {
        return d_false;
    }
    if (mask->y > d->y) {
        return d_false;
    }
    if (mask->h + mask->y < d->h + d->y) {
        return d_false;
    }
    return d_true;
}

void dim_trunc (dim_t *d, const dim_t *s)
{
    if (d->x > s->w) {
          d->x = s->w;
    }
    if (d->x + d->w > s->w) {
          d->w = s->w - d->x;
    }
    if (d->y > s->h) {
          d->y = s->h;
    }
    if (d->y + d->h > s->h) {
          d->h = s->h - d->y;
    }
}

void dim_place (dim_t *d, const dim_t *s)
{
    dim_t tmp = {0, 0, s->w, s->h};
    dim_trunc(d, &tmp);
    d->x = d->x + s->x;
    d->y = d->y + s->y;
}

void dim_tolocal (dim_t *d, const dim_t *s)
{
    d->x = d->x - s->x;
    d->y = d->y - s->y;
    dim_trunc(d, s);
}

void dim_tolocal_p (point_t *d, const dim_t *s)
{
    d->x = d->x - s->x;
    d->y = d->y - s->y;
}

void dim_get_origin (point_t *d, const dim_t *s)
{
    d->x = s->x + (s->w / 2);
    d->y = s->y + (s->h / 2);
}

void dim_set_origin (dim_t *d, const point_t *s)
{
    d->x = s->x - d->w / 2;
    d->y = s->y - d->h / 2;
}

void dim_set_right (dim_t *d, dim_t *s)
{
    int x = s->x - d->w;
    int y = s->y + (s->h / 2);

    d->x = x;
    d->y = y - (d->h / 2);
}

void dim_set_left (dim_t *d, dim_t *s)
{
    int x = s->x + s->w;
    int y = s->y + (s->h / 2);

    d->x = x;
    d->y = y - (d->h / 2);
}

void dim_set_top (dim_t *d, dim_t *s)
{
    int x = s->x + (s->w / 2);
    int y = s->y + s->h;

    d->x = x - (d->w / 2);
    d->y = y;
}

void dim_set_bottom (dim_t *d, dim_t *s)
{
    int x = s->x + (s->w / 2);
    int y = s->y - d->h;

    d->x = x - (d->w / 2);
    d->y = y;
}


