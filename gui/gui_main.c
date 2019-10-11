#include <string.h>

#include <gui.h>
#include <jpeg.h>
#include <misc_utils.h>
#include <lcd_main.h>
#include <dev_io.h>
#include <heap.h>
#include <bsp_cmd.h>

extern void gui_rect_fill_HAL (dim_t *dest, dim_t *rect, rgba_t color);
extern void gui_com_fill_HAL (component_t *com, rgba_t color);
extern int gui_draw_string_HAL (component_t *com, int line,
                                rgba_t textcolor, const char *str, int txtmode);
extern void gui_get_font_prop_HAL (fontprop_t *prop, const void *_font);
extern const void *gui_get_font_4_size_HAL (gui_t *gui, int size, int bestmatch);

void gui_get_font_prop (fontprop_t *prop, const void *font)
{
    gui_get_font_prop_HAL(prop, font);
}

const void *gui_get_font_4_size (gui_t *gui, int size, int bestmatch)
{
    return gui_get_font_4_size_HAL(gui, size, bestmatch);
}

#define gui_error(fmt, args ...) \
    if (gui->dbglvl >= DBG_ERR) {dprintf("%s() [fatal] : "fmt, __func__, args);}

#define gui_warn(args ...) \
    if (gui->dbglvl >= DBG_WARN) {dprintf(args);}

#define gui_info(fmt, args ...) \
    if (gui->dbglvl >= DBG_INFO) {dprintf("%s() : "fmt, __func__, args);}

#define G_TEXT_MAX 256

typedef struct {
    dim_t activebox;
    dim_t dirtybox;
    uint8_t dirty;
} gui_int_t; /*internal usage*/

static d_bool __gui_draw_allowed (gui_t *gui);

static gui_int_t gui_int = {{0}};

#define GUI_LINK(parent, item)     \
do  {                             \
    (item)->next = (parent)->head; \
    (parent)->head = (item);       \
} while (0)

component_t *
gui_iterate_all (gui_t *gui, void *user, int (*h) (component_t *com, void *user))
{
    if (h == NULL) {
        return NULL;
    }
    pane_t *pane = gui->head;
    component_t *com;

    if (!pane) {
        return NULL;
    }
    while (pane) {
        com = pane->head;
        while (com) {
            if (h(com, user) > 0) {
                return com;
            }
            com = com->next;
        }
        pane = pane->next;
    }
    return NULL;
}

static inline void
gui_set_dirty (gui_t *gui, dim_t *dim)
{
    gui_int_t *gui_int = gui->ctxt;
    assert(gui_int);

    dim_extend(&gui_int->dirtybox, dim);
    gui_int->dirty = 1;
}

static inline d_bool
gui_is_com_durty (gui_t *gui, component_t *com)
{
    gui_int_t *gui_int = gui->ctxt;
    assert(gui_int);

    return dim_check_intersect(&gui_int->activebox, &com->dim);
}

static inline void
gui_mark_dirty (gui_t *gui)
{
    gui_int_t *gui_int = gui->ctxt;
    assert(gui_int);

    if (!gui_int->dirty) {
        return;
    }

    d_memcpy(&gui_int->activebox, &gui_int->dirtybox, sizeof(gui_int->activebox));
    d_memzero(&gui_int->dirtybox, sizeof(gui_int->dirtybox));
    gui_int->dirty = 0;
}

void gui_com_set_dirty (gui_t *gui, component_t *com)
{
    gui_set_dirty(gui, &com->dim);
}

void gui_pane_set_dirty (gui_t *gui, pane_t *pane)
{
    gui_set_dirty(gui, &pane->dim);
}

static inline void
gui_post_draw (gui_t *gui)
{
    gui_int_t *gui_int = gui->ctxt;
    assert(gui_int);

    d_memzero(&gui_int->activebox, sizeof(gui_int->activebox));
}

void gui_init (gui_t *gui, const char *name, uint8_t framerate,
                dim_t *dim, gui_bsp_api_t *bspapi)
{
    char temp[GUI_MAX_NAME];

    d_memzero(gui, sizeof(*gui));

    gui->font = gui_get_font_4_size_HAL(gui, 20, 1);

    gui->dim.x = dim->x;
    gui->dim.y = dim->y;
    gui->dim.w = dim->w;
    gui->dim.h = dim->h;
    gui->framerate = framerate;
    gui->dbglvl = DBG_WARN;
    snprintf(gui->name, sizeof(gui->name), "%s", name);

    snprintf(temp, sizeof(temp), "%s_%s", name, "fps");
    cmd_register_i32(&gui->framerate, temp);
    
    snprintf(temp, sizeof(temp), "%s_%s", name, "dbglvl");
    cmd_register_i32(&gui->dbglvl, temp);

    d_memcpy(&gui->bspapi, bspapi, sizeof(gui->bspapi));
    gui->ctxt = &gui_int;
    gui_set_dirty(gui, &gui->dim);
    gui_mark_dirty(gui);
}

void gui_destroy (gui_t *gui)
{
    pane_t *pane = gui->head;
    component_t *com;

    while (pane) {
        com = pane->head;
        while (com) {
            heap_free(com);
            com = com->next;
        }
        gui_rect_fill(pane->parent, &pane->dim, &pane->dim, COLOR_BLACK);
        heap_free(pane);
        pane = pane->next;
    }
    gui->destroy = 0;
}

pane_t *gui_create_pane (gui_t *gui, const char *name, int extra)
{
    int namelen = strlen(name);
    int allocsize;
    pane_t *pane = NULL;

    allocsize = sizeof(*pane) + namelen + 1;
    if (extra > 0) {
        allocsize += extra;
    }
    allocsize = ROUND_UP(allocsize, 4);
    pane = gui_bsp_alloc(gui, allocsize);
    assert(pane);
    d_memzero(pane, allocsize);
    snprintf(pane->name, namelen + 1, "%s", name);
    return pane;
}

static void __gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h)
{
    GUI_LINK(gui, pane);
    pane->dim.x = x;
    pane->dim.y = y;
    pane->dim.w = w;
    pane->dim.h = h;
    pane->parent = gui;
    pane->selectable = 1;
}

void gui_set_pane (gui_t *gui, pane_t *pane)
{
    __gui_set_panexy(gui, pane, gui->dim.x, gui->dim.y, gui->dim.w, gui->dim.h);
}

void gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h)
{
    __gui_set_panexy(gui, pane, x, y, w, h);
    dim_place(&pane->dim, &gui->dim);
}

void gui_set_child (pane_t *parent, pane_t *child)
{
    child->child = parent->child;
    parent->child = child;
}

void gui_set_pic (component_t *com, rawpic_t *pic, int top)
{
    pic->sprite = 0;
    if (!pic->alpha) {
        pic->alpha = 0xff;
    }
    com->pic = pic;
    com->pictop = top;
    if (pic->w > com->dim.w) {
        pic->w = com->dim.w;
    }
    if (pic->h > com->dim.h) {
        pic->h = com->dim.h;
    }
    gui_set_dirty(com->parent->parent, &com->dim);
}

component_t *gui_create_comp (gui_t *gui, const char *name, const char *text)
{
    int namelen = strlen(name);
    int textlen = text ? strlen(text) : G_TEXT_MAX;
    int allocsize;
    component_t *com;

    allocsize = sizeof(*com) + namelen + 1 + textlen + 1;
    allocsize = ROUND_UP(allocsize, sizeof(uint32_t));
    com = gui_bsp_alloc(gui, allocsize);
    assert(com && name);
    d_memzero(com, allocsize);
    snprintf(com->name, sizeof(com->name), "%s", name);
    com->text_size = textlen + 1;
    if (text && text[0]) {
        gui_print(com, "%s", text);
    }
    return com;

}

void gui_set_comp (pane_t *pane, component_t *c, int x, int y, int w, int h)
{
    GUI_LINK(pane, c);

    c->dim.x = (x > 0) ? x : 0;
    c->dim.y = (y > 0) ? y : 0;
    c->dim.w = (w > 0) ? w : pane->dim.w;
    c->dim.h = (h > 0) ? h : pane->dim.h;
    c->parent = pane;
    c->font = pane->parent->font;
    dim_place(&c->dim, &pane->dim);
}

void gui_set_prop (component_t *c, prop_t *prop)
{
    c->fcolor = prop->fcolor;
    c->bcolor = prop->bcolor;
    c->ispad = prop->ispad;
    c->userdraw = prop->user_draw;
    if (prop->fontprop.font) {
        c->font = prop->fontprop.font;
    }
}

static inline rgba_t
gui_com_select_color (component_t *com)
{
#define APPLY_GLOW(color, glow) \
    ((color) | (((glow) << 24) | ((glow) << 16) | ((glow) << 8) | ((glow) << 0)))

    rgba_t color = com->bcolor;

    if (com->glow && com == com->parent->onfocus) {
        color = APPLY_GLOW(color, com->glow);
    }
    return color;
#undef APPLY_GLOW
}

void gui_com_clear (component_t *com)
{
    gui_com_fill(com, com->bcolor);
}

void gui_com_fill (component_t *com, rgba_t color)
{
    gui_set_dirty(com->parent->parent, &com->dim);
    gui_com_fill_HAL(com, color);
}

void gui_set_text (component_t *com, const char *text, int x, int y)
{
    snprintf(com->text, com->text_size, "%s", text);
}

void gui_printxy (component_t *com, int x, int y, const char *fmt, ...)
{
    va_list         argptr;

    va_start (argptr, fmt);
    com->text_index = vsnprintf (com->text, com->text_size, fmt, argptr);
    va_end (argptr);
}

int gui_apendxy (component_t *com, int x, int y, const char *fmt, ...)
{
    va_list         argptr;
    uint16_t        offset = com->text_index - 1;

    if (com->text_index >= com->text_size - 1) {
        return -1;
    }

    va_start (argptr, fmt);

    com->text_index +=
        vsnprintf (com->text + offset, com->text_size - com->text_index, fmt, argptr);
    va_end (argptr);
    return com->text_size - com->text_index;
}

int gui_draw_string (component_t *com, int line, rgba_t textcolor, const char *str)
{
    return gui_draw_string_HAL(com, line, textcolor, str, GUI_LEFT_ALIGN);
}

int gui_draw_string_c (component_t *com, int line, rgba_t textcolor, const char *str)
{
    return gui_draw_string_HAL(com, line, textcolor, str, GUI_CENTER_ALIGN);
}

void gui_rect_fill (gui_t *gui, dim_t *dest, dim_t *rect, rgba_t color)
{
    gui_rect_fill_HAL(dest, rect, color);
    gui_set_dirty(gui, rect);
}

static int
gui_rawpic_draw (component_t *com, rawpic_t *pic)
{
    screen_t src = {0};
    point_t p;
    dim_t d = {0, 0, pic->w, pic->h};

    dim_get_origin(&p, &com->dim);
    dim_set_origin(&d, &p);

    src.x = 0;
    src.y = 0;
    src.width = pic->w;
    src.height = pic->h;
    src.buf = pic->data;
    src.colormode = GFX_COLOR_MODE_AUTO;
    src.alpha = pic->alpha;

    if (pic->sprite) {
        /*second plane for chroma keyed*/
        vid_direct(d.x, d.y, &src, 1);
    } else {
        vid_direct(d.x, d.y, &src, -1);
    }
    return 0;
}

static inline void
gui_com_draw_default (pane_t *pane, component_t *com)
{
    int len = 0, tmp;
    int line = 0;
    char *text;

    if (com->draw) {
        com->draw(pane, com, NULL);
    }
    if (com->showname) {
        gui_draw_string_c(com, line, com->fcolor, com->name);
        line++;
    }
    len = com->text_index;
    text = com->text;
    tmp = len;
    while (len > 0 && tmp > 0) {
        tmp = gui_draw_string_c(com, line, com->fcolor, text);
        len = len - tmp;
        text += tmp;
        line++;
    }
}

static void
gui_com_draw (pane_t *pane, component_t *com)
{
    rgba_t color;

    if (com->ispad) {
        gui_com_clear(com);
        return;
    }
    color = gui_com_select_color(com);
    gui_com_fill(com, color);

    if (com->pic && !com->pictop) {
        gui_rawpic_draw(com, com->pic);
    }
    if (com->draw) {
        com->draw(pane, com, NULL);
    }
    if (!com->userdraw) {
        gui_com_draw_default(pane, com);
    }
    if (com->pic && com->pictop) {
        gui_rawpic_draw(com, com->pic);
    }
}

static int gui_pane_draw (gui_t *gui, pane_t *pane)
{
    component_t *com = pane->head;
    int repaint = 0;

    if (!pane->repaint) {
        return repaint;
    }
    if (pane->child) {
        repaint = gui_pane_draw(gui, pane->child);
    }
    vid_vsync(0);
    while (com) {
        if (gui_is_com_durty(gui, com)) {
            gui_com_draw(pane, com);
        }
        com = com->next;
    }
    pane->repaint = 0;
    return repaint;
}

void gui_draw (gui_t *gui, int force)
{
    pane_t *selected = gui->selected_head;

    if (!__gui_draw_allowed(gui)) {
        return;
    }
    assert(selected);
    gui_mark_dirty(gui);
    if (gui_pane_draw(gui, selected)) {
        gui_post_draw(gui);
    }
}

void gui_set_repaint (pane_t *pane, int repaint)
{
    while (pane) {
        pane->repaint = repaint;
        pane = pane->child;
    }
}

static component_t *
gui_get_com_4_xy (pane_t *pane, const gevt_t *evt, point_t *pplocal)
{
    component_t *com;
    point_t plocal = evt->p;

    vid_ptr_align(&plocal.x, &plocal.y);
    dim_tolocal_p(&plocal, &pane->dim);
    com = pane->onfocus;

    if (com) {
        if (evt->e == GUIRELEASE) {
            gui_set_focus(pane, NULL, com);
        }
    } else {
        com = pane->head;
    }
    while (com) {
        if (dim_check(&com->dim, &evt->p)) {
            break;
        }
        com = com->next;
    }
    if (com) {
        pplocal->x = plocal.x;
        pplocal->y = plocal.y;
        dim_tolocal_p(pplocal, &com->dim);
        if (evt->e == GUIACT) {
            gui_set_focus(pane, com, NULL);
        }
    }
    return com;
}

static int
gui_com_input_direct (pane_t *pane, component_t *com, gevt_t *evt)
{
    comp_handler_t h;

    if (evt->e == GUIRELEASE) {
        h = com->release;
    } else {
        h = com->act;
    }
    if (h && h(pane, com, evt) > 0) {
        return 1;
    }
    return 0;
}

static int
gui_com_input_xy (pane_t *pane, const gevt_t *evt)
{
    point_t plocal;
    comp_handler_t h;
    d_bool isrelease = d_false;
    component_t *com = gui_get_com_4_xy(pane, evt, &plocal);
    gevt_t comevt;

    if (!com) {
        return 0;
    }

    if (isrelease) {
        h = com->release;
    } else {
        h = com->act;
    }
    if (!h) {
        return 0;
    }

    comevt.e = evt->e;
    comevt.p = plocal;
    comevt.sym = evt->sym;
    comevt.symbolic = evt->symbolic;

    if (h(pane, com, &comevt) > 0) {
        return 1;
    }
    return 0;
}

int gui_send_event (gui_t *gui, gevt_t *evt)
{
    component_t *com = NULL;
    pane_t *pane = gui->selected_head;
    int handled = 0;

    assert(pane);
    if (evt->symbolic) {
        com = pane->onfocus;
    }
    if (com) {
        if (pane == gui->selected_head) {
            handled = gui_com_input_direct(pane, com, evt);
        }
    } else {
        handled = gui_com_input_xy(pane, evt);
    }
    if (handled) {
        pane = gui->selected_head;
        gui_set_repaint(pane, 1);
        gui_mark_dirty(gui);
    }
    return handled;
}

static int
gui_contain_child (pane_t *parent, pane_t *child)
{
    while (parent) {
        if (parent == child) {
            return 1;
        }
        parent = parent->child;
    }
    return 0;
}

void gui_wakeup_com (gui_t *gui, component_t *com)
{
    pane_t *pane = com->parent;

    if (pane != gui->selected_head &&
        !gui_contain_child(gui->selected_head, pane)) {
        return;
    }
    com->parent->repaint = 1;

}

pane_t *gui_get_pane_4_name (gui_t *gui, const char *name)
{
    pane_t *pane = gui->head;

    while (pane) {
        if (strcmp(pane->name, name) == 0) {
            return pane;
        }
        pane = pane->next;
    }
    return NULL;
}

void gui_select_pane (gui_t *gui, pane_t *pane)
{
    pane->selected_next = gui->selected_head;
    if (!gui->selected_tail) {
        gui->selected_tail = pane;
    }
    gui->selected_head = pane;
    gui_mark_dirty(gui);
    gui_set_repaint(pane, 1);
}

pane_t *gui_release_pane (gui_t *gui)
{
    pane_t *head = gui->selected_head,
           *tail = gui->selected_tail;

    if (!head) {
        return NULL;
    }
    if (head == tail) {
        gui_mark_dirty(gui);
        gui_set_repaint(head, 1);
        head = NULL;
        gui->selected_tail = NULL;
    } else {
        head = head->selected_next;
    }
    gui->selected_head = head;
    return head;
}

static inline pane_t *
__gui_iter_next_pane (gui_t *gui, pane_t *pane)
{
    assert(pane);
    pane = pane->next;

    if (!pane) {
        pane = gui->head;
    }
    return pane;
}

pane_t *
gui_select_next_pane (gui_t *gui)
{
    pane_t *prevpane = gui->selected_head;
    pane_t *pane = __gui_iter_next_pane(gui, prevpane);

    while (prevpane != pane && !pane->selectable) {
        pane = __gui_iter_next_pane(gui, pane);
    }
    if (prevpane != pane) {
        gui_release_pane(gui);
        gui_select_pane(gui, pane);
    }
    return pane;
}

static inline component_t *
__gui_iter_next_focus (pane_t *pane, component_t *com)
{
    com = com->next;
    if (!com) {
        com = pane->head;
    }
    return com;
}

component_t *
gui_set_focus (pane_t *pane, component_t *com, component_t *prev)
{
    gui_t *gui = pane->parent;

    assert(com || prev);
    pane->onfocus = com;
    if (com) {
        gui_com_set_dirty(gui, com);
        com->glow = 127;
    }
    if (prev) {
        gui_com_set_dirty(gui, prev);
        prev->glow = 0;
    }

    gui_set_repaint(pane, 1);
    gui_mark_dirty(gui);
    return com;
}

component_t *gui_set_next_focus (gui_t *gui)
{
    pane_t *pane = gui->selected_head;
    component_t *com, *prev;

    if (!pane || !pane->onfocus) {
        return NULL;
    }
    prev = pane->onfocus;
    com = __gui_iter_next_focus(pane, prev);

    while (com != prev && !com->selectable) {
        com = __gui_iter_next_focus(pane, com);
    }
    if (com == prev) {
        return NULL;
    }

    return gui_set_focus(pane, com, prev);
}

static d_bool __gui_draw_allowed (gui_t *gui)
{
    if (0 == d_rlimit_wrap(&gui->repainttsf, 1000 / gui->framerate)) {
        return d_false;
    }
    return d_true;
}

