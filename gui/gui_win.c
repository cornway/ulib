#include <string.h>
#include <heap.h>
#include <jpeg.h>
#include <gfx.h>
#include <gui.h>

#if defined(BSP_DRIVER)

#define WIN_ERR(args ...) \
    dprintf("gui err : "args)

typedef enum {
    WINNONE,
    WINHIDE,
    WINCLOSE,
    WINACCEPT,
    WINDECLINE,
    WINSTATEMAX,
} winact_t;

typedef enum {
    WIN_NONE,
    WIN_ALERT,
    WIN_CONSOLE,
    WIN_PROGRESS,
    WIN_JPEG,
    WIN_MAX,
} WIN_TYPE;

typedef struct {
    pane_t *pane;
    win_user_hdlr_t user_clbk;
    WIN_TYPE type;
} win_t;

static component_t *
win_new_border (gui_t *gui)
{
    component_t *com = gui_create_comp(gui, "pad", "");
    com->bcolor = COLOR_BLACK;
    com->fcolor = COLOR_BLACK;
    com->ispad = 1;
    return com;
}

static inline win_t *WIN_HANDLE (void *_pane)
{
    pane_t *pane = _pane;
    win_t *win = (win_t *)(pane + 1);
    assert(win->pane == pane);
    return win;
}

static inline win_t *WIN_HANDLE_CHECK (void *pane, WIN_TYPE type)
{
    win_t *win = WIN_HANDLE(pane);
    assert(win->type == type);
    return win;
}

void win_set_user_clbk (void *pane, win_user_hdlr_t clbk)
{
    win_t *win = WIN_HANDLE(pane);
    win->user_clbk = clbk;
}

static void win_trunc_frame (dim_t *dim, const point_t *border, int x, int y, int w, int h)
{
    dim->x = x + border->x;
    dim->y = y + border->y;

    dim->w = w - border->w * 2;
    dim->h = h - border->h * 2;
}

static void win_setup_frame (pane_t *pane, const point_t *border, dim_t *dim)
{
    typedef void (*dim_set_func_t)(dim_t *, dim_t *);

    gui_t *gui = pane->parent;
    component_t *com;
    int i;
    dim_t wdim = {0, 0, dim->w + border->w * 2, border->y};
    dim_t hdim = {0, 0, border->w, dim->h + border->h * 2};
    dim_set_func_t func[] =
    {
        dim_set_top,
        dim_set_bottom,
        dim_set_left,
        dim_set_right,
    };
    dim_t *dimptr[] = {&wdim, &wdim, &hdim, &hdim};

    for (i = 0; i < arrlen(func); i++) {
        func[i](dimptr[i], dim);
        com = win_new_border(gui);
        gui_set_comp(pane, com, dimptr[i]->x, dimptr[i]->y, dimptr[i]->w, dimptr[i]->h);
    }
}

enum {
    WALERT_ACT_NONE = 0x0,
    WALERT_ACT_ACCEPT = 0x1,
    WALERT_ACT_DECLINE = 0x2,
    WALERT_ACT_MS = 0x3,
};

typedef struct {
    win_t win;
    win_alert_hdlr_t yes, no;
    component_t *com_yes, *com_no;
    component_t *com_msg;
} win_alert_t;

static inline win_alert_t *
WALERT_HANDLE (void *pane)
{
    return (win_alert_t *)WIN_HANDLE_CHECK(pane, WIN_ALERT);
}

static void win_user_handler (win_t *win, gevt_t *evt)
{
    if (NULL == win->user_clbk) {
        return;
    }
    win->user_clbk(evt);
}

static int win_alert_handler (pane_t *pane, component_t *c, void *user)
{
    gevt_t *evt = (gevt_t *)user;
    win_alert_hdlr_t h = NULL;
    win_alert_t *alert = WALERT_HANDLE(pane);
    int needsclose = 1;

    win_user_handler(&alert->win, evt);

    if (evt->sym == GUI_KEY_LEFT ||
        evt->sym == GUI_KEY_RIGHT) {
        gui_set_next_focus(pane->parent);
        return 1;
    }

    if ((c->userflags & WALERT_ACT_MS) == 0 ||
        evt->sym != GUI_KEY_RETURN) {
        return 1;
    }
    switch (c->userflags & WALERT_ACT_MS) {
        case WALERT_ACT_ACCEPT:  h = alert->yes;
        break;
        case WALERT_ACT_DECLINE: h = alert->no;
        break;
        default: needsclose = 0;
        break;
    }
    if (needsclose) {
        win_close_allert(pane);
    }
    if (h) {
        return h(c);
    }
    return 1;
}

static win_alert_t *win_alert_alloc (gui_t *gui)
{
    pane_t *pane;
    win_alert_t *win;
    int memsize = sizeof(win_alert_t);

    pane = gui_create_pane(gui, "allert", memsize);
    win = (win_alert_t *)(pane + 1);
    win->win.pane = pane;
    win->win.type = WIN_ALERT;
    return win;
}

pane_t *win_new_allert (gui_t *gui, int w, int h, const char *msg)
{
    const point_t border = {16, 8};
    const int btnsize = 40;
    dim_t dim;
    point_t p;
    pane_t *pane;
    component_t *com;
    win_alert_t *alert;
    prop_t prop = {0};

    alert = win_alert_alloc(gui);
    pane = alert->win.pane;

    gui_set_panexy(gui, pane, 0, 0, w, h);
    dim_get_origin(&p, &gui->dim);
    dim_set_origin(&pane->dim, &p);

    pane->selectable = 0;

    prop.bcolor = COLOR_RED;
    prop.fcolor = COLOR_BLACK;
    gui_set_prop(com, &prop);

    com = gui_create_comp(gui, "title", msg);
    com->userflags = WALERT_ACT_NONE;
    gui_set_comp(pane, com, 0, 0, w, btnsize);

    prop.bcolor = COLOR_LBLUE;
    prop.fcolor = COLOR_WHITE;
    gui_set_prop(com, &prop);

    com = gui_create_comp(gui, "accept", "YES");
    com->act = win_alert_handler;
    com->userflags = WALERT_ACT_ACCEPT;
    com->glow = 0x1f;
    com->selectable = 1;
    gui_set_comp(pane, com, 0, h - btnsize, w / 2 - border.w / 2, btnsize);
    alert->com_yes = com;

    prop.bcolor = COLOR_DGREY;
    prop.fcolor = COLOR_WHITE;
    gui_set_prop(com, &prop);

    com = gui_create_comp(gui, "decline", "NO");
    com->act = win_alert_handler;
    com->userflags = WALERT_ACT_DECLINE;
    com->glow = 0x1f;
    com->selectable = 1;
    gui_set_comp(pane, com, w / 2 + border.w / 2, h - btnsize, w / 2 - border.w / 2, btnsize);
    pane->onfocus = com;
    alert->com_no = com;

    gui_set_prop(com, &prop);

    com = win_new_border(gui);
    gui_set_comp(pane, com, w / 2 - border.w / 2, h - btnsize, border.w, btnsize);

    win_trunc_frame(&dim, &border, 0, btnsize, w, h - btnsize * 2);
    win_setup_frame(pane, &border, &dim);

    com = gui_create_comp(gui, "message", NULL);
    gui_set_comp(pane, com, dim.x, dim.y, dim.w, dim.h);
    alert->com_msg = com;

    prop.bcolor = COLOR_WHITE;
    prop.fcolor = COLOR_BLACK;
    gui_set_prop(com, &prop);

    return pane;
}

int win_alert (pane_t *pane, const char *text,
                  win_alert_hdlr_t accept, win_alert_hdlr_t decline)
{
    win_alert_t *alert;
    component_t *com;

    if (!pane) {
        return -1;
    }
    alert = WALERT_HANDLE(pane);
    com = alert->com_msg;
    assert(com);
    alert->yes = accept;
    alert->no = decline;
    gui_print(com, "%s", text);
    gui_pane_set_dirty(pane->parent, pane);
    gui_select_pane(pane->parent, pane);
    return 0;
}

int win_close_allert (pane_t *pane)
{
    pane = gui_release_pane(pane->parent);
    if (pane) {
       gui_select_pane(pane->parent, pane);
    }
    return 0;
}

typedef struct con_line_s {
    struct con_line_s *next;
    char *ptr;
    rgba_t color;
    uint16_t pos;
    uint8_t flags;
} con_line_t;

typedef struct {
    win_t win;
    comp_handler_t useract;
    component_t *com;

    uint16_t wmax, hmax;

    char *textbuf;
    con_line_t *linehead, *linetail;
    con_line_t *lastline;
    con_line_t line[1];
} win_con_t;

static int win_con_repaint (pane_t *pane, component_t *com, void *user);

static inline win_con_t *WCON_HANDLE (void *pane)
{
    return (win_con_t *)WIN_HANDLE_CHECK(pane, WIN_CONSOLE);
}

void win_con_set_clbk (void *_pane, comp_handler_t h)
{
    win_con_t *win = WCON_HANDLE(_pane);
    win->useract = h;
}

static int wcon_print_line (int *nextline, char *dest, const char *src, int len)
{
    int cnt = 0;

    *nextline = 0;
    for (cnt = 0; *src && cnt < len; cnt++) {
        if (*src == '\n') {
            *nextline = 1;
            *dest = 0;
            cnt++;
            break;
        } else if (*src == '\r') {
           *dest = ' ';
        } else {
           *dest = *src;
        }
        dest++;
        src++;
    }
    if (*src || cnt == len) {
        *nextline = 1;
    }
    *dest = 0;
    return cnt;
}

static con_line_t *wcon_next_line (win_con_t *con)
{
    con_line_t *line = con->linehead;
    /*Move first to the tail, to make it 'scrolling'*/
    con->linehead = line->next;
    con->linetail->next = line;
    con->linetail = line;
    line->next = NULL;
    return line;
}

static inline void wcon_reset_line (con_line_t *line)
{
    line->pos = 0;
}

static void wcon_lsetup_lines (win_con_t *con, rgba_t textcolor)
{
    char *buf = con->textbuf;
    con_line_t *line, *prev = NULL;
    int i;

    line = &con->line[0];
    con->linehead = line;

    for(i = 0; i < con->hmax; i++) {
        line->color = textcolor;
        line->flags = 0;
        line->pos = 0;
        line->ptr = buf;
        buf += con->wmax;
        prev = line;
        line++;
        if (prev) {
            prev->next = line;
        }
    }
    con->linetail = (line - 1);
    con->lastline = con->linehead;
}

static win_con_t *
wcon_alloc (gui_t *gui, const char *name,
                const void *font, int x, int y, int w, int h)
{
    uint32_t wmax, hmax;
    int textsize, textoff;
    pane_t *pane;
    win_con_t *win;
    fontprop_t fprop;
    int winmemsize = sizeof(win_con_t);

    gui_get_font_prop(&fprop, font);

    wmax = w / fprop.w;
    hmax = h / fprop.h;
    hmax = hmax ? hmax - 1 : 0;

    textsize = hmax * wmax;
    winmemsize += hmax * sizeof(con_line_t);
    winmemsize += textsize;
    textoff = winmemsize - textsize;

    pane = gui_create_pane(gui, name ? name : "noname", winmemsize);

    win = (win_con_t *)(pane + 1);
    d_memset(win, 0, winmemsize);
    win->win.pane = pane;
    win->win.type = WIN_CONSOLE;
    win->wmax = wmax;
    win->hmax = hmax;
    win->textbuf = (char *)win + textoff;

    gui_set_pane(gui, pane);

    return win;
}

static int win_act_handler (pane_t *pane, component_t *c, void *user)
{
    int ret = 0;
    win_con_t *win = WCON_HANDLE(pane);

    win_user_handler(&win->win, (gevt_t *)user);

    if (win->useract) {
        ret = win->useract(pane, c, user);
    }
    return ret;
}

pane_t *win_new_console (gui_t *gui, prop_t *prop, int x, int y, int w, int h)
{
    const void *font = prop->fontprop.font;
    component_t *com;
    win_con_t *win;
    dim_t dim;
    point_t border = {2, 2};

    if (font == NULL) {
        font = gui->font;
        assert(font);
        prop->fontprop.font = font;
    }

    win = wcon_alloc(gui, prop->name, font, x, y, w, h);
    wcon_lsetup_lines(win, prop->fcolor);

    win_trunc_frame(&dim, &border, x, y, w, h);

    prop->user_draw = 1;
    com = gui_create_comp(gui, "output", "");
    com->draw = win_con_repaint;
    com->act = win_act_handler;
    gui_set_comp(win->win.pane, com, dim.x, dim.y, dim.w, dim.h);
    gui_set_prop(com, prop);
    win->com = com;

    win_setup_frame(win->win.pane, &border, &dim);

    win->win.pane->onfocus = com;

    return win->win.pane;
}

static int wcon_append_line (int *nextline, win_con_t *con, con_line_t *line, const char *str)
{
    char *ptr;
    int len = con->wmax - line->pos;
    int cnt;

    if (len <= 0) {
        line->pos = 0;
        len = con->wmax;
    }

    ptr = line->ptr + line->pos;
    cnt = wcon_print_line(nextline, ptr, str, len);
    line->pos += cnt;
    return cnt;
}

int win_con_get_dim (pane_t *pane, int *w, int *h)
{
    win_con_t *con;

    con = WCON_HANDLE(pane);
    *w = con->wmax;
    *h = con->hmax;
    return 0;
}

static inline con_line_t *
__get_line (win_con_t *con, int y)
{
    return &con->line[y];
}

static char *__get_buf (win_con_t *con, con_line_t *line, int x, int *size)
{
    char *buf;

    assert(x < con->wmax);
    buf = line->ptr + x;
    *size = con->wmax - x;
    line->pos = *size;
    return buf;
}

static inline int
__win_con_print_at (win_con_t *con, int x, int y, const char *str, rgba_t textcolor)
{
    con_line_t *line;
    char *buf;
    int max;

    line = __get_line(con, y);
    line->color = textcolor;
    buf = __get_buf(con, line, x, &max);
    d_memset(buf, ' ', max);
    max = sprintf(buf, "%s", str);
    return max;
}

int win_con_print_at (pane_t *pane, int x, int y, const char *str, rgba_t textcolor)
{
    return __win_con_print_at(WCON_HANDLE(pane), x, y, str, textcolor);
}

int win_con_printline (pane_t *pane, int y,
                                    const char *str, rgba_t textcolor)
{
    return __win_con_print_at(WCON_HANDLE(pane), 0, y, str, textcolor);
}

int win_con_printline_c (pane_t *pane, int y,
                                const char *str, rgba_t textcolor)
{
    win_con_t *con = WCON_HANDLE(pane);
    int len = strlen(str);
    int x = 0;

    if (len > con->wmax) {
        len = con->wmax;
    } else {
        x = (con->wmax - len) / 2;
    }
    return win_con_print_at(pane, x, y, str, textcolor);
}

void win_con_clear (pane_t *pane)
{
    win_con_t *con;
    con_line_t *line;

    con = WCON_HANDLE(pane);
    line = con->linehead;

    while (line) {
        d_memset(line->ptr, ' ', con->wmax);
        line->pos = 0;
        line = line->next;
    }
}

int win_con_append (pane_t *pane, const char *str, rgba_t textcolor)
{
    win_con_t *con;
    const char *strptr = str;
    int linecnt = 0, tmp, nextline;
    con_line_t *line;

    con = WCON_HANDLE(pane);
    line = con->lastline;
    assert(line);
    while (*strptr && linecnt < con->hmax) {

        nextline = 0;
        tmp = wcon_append_line(&nextline, con, line, strptr);
        strptr += tmp;

        if (nextline) {
            line = wcon_next_line(con);
            wcon_reset_line(line);
            linecnt++;
        }
    }
    con->lastline = line;

    return strptr - str;
}

static inline void
win_con_clean_line (component_t *com, fontprop_t *fprop, con_line_t *line, int linenum)
{
    if (line->pos) {
        dim_t dim = {0, linenum * fprop->h, line->pos * fprop->w, fprop->h};
        gui_rect_fill(com->parent->parent, &com->dim, &dim, com->bcolor);
    }
}

static int win_con_repaint (pane_t *pane, component_t *com, void *user)
{
    fontprop_t fprop;
    win_con_t *con;
    con_line_t *line;
    int linecnt = 0;

    con = WCON_HANDLE(pane);
    assert(com == con->com);

    gui_get_font_prop(&fprop, com->font);
    line = con->linehead;

    while (line) {
        if (line->pos) {
            win_con_clean_line(com, &fprop, line, linecnt);
            gui_draw_string(con->com, linecnt, line->color, line->ptr);
            linecnt++;
        }
        line = line->next;
    }
    return linecnt;
}


typedef struct {
    win_t win;
    int percent;
    component_t *title, *bar;
} win_progress_t;

static inline win_progress_t *
WPROG_HANDLE (void *pane)
{
    return (win_progress_t *)WIN_HANDLE_CHECK(pane, WIN_PROGRESS);
}

static int win_prog_act_resp (pane_t *pane, component_t *com, void *user);
static int win_prog_repaint (pane_t *pane, component_t *com, void *user);

static win_progress_t *win_prog_alloc (gui_t *gui, const char *name)
{
    win_progress_t *win;
    pane_t *pane;
    int memsize = sizeof(win_progress_t);

    pane = gui_create_pane(gui, name, memsize);

    win = (win_progress_t *)(pane + 1);
    win->win.pane = pane;
    win->win.type = WIN_PROGRESS;
    return win;
}

pane_t *win_new_progress (gui_t *gui, prop_t *prop, int x, int y, int w, int h)
{
    const point_t border = {4, 4};
    const int htitle = 32;
    win_progress_t *win;
    pane_t *pane;
    component_t *com;
    dim_t dim = {x, y, w, h};
    const void *font = prop->fontprop.font;

    if (font == NULL) {
        font = gui->font;
        prop->fontprop.font = font;
    }
    prop->ispad = 0;
    prop->user_draw = 0;

    win = win_prog_alloc(gui, prop->name);
    pane = win->win.pane;
    gui_set_panexy(gui, pane, x, y, w, h);

    win_trunc_frame(&dim, &border, 0, htitle, w, h - htitle);

    com = gui_create_comp(gui, "statusbar", "     "); /*Reserved space to print "100%\0"*/
    gui_set_comp(pane, com, dim.x, dim.y, dim.w, dim.h);
    com->draw = win_prog_repaint;
    com->act = win_prog_act_resp;
    win->bar = com;

    gui_set_prop(com, prop);

    win_setup_frame(pane, &border, &dim);

    com = gui_create_comp(gui, "title", NULL);
    gui_set_comp(pane, com, 0, 0, w, htitle);
    win->title = com;

    gui_set_prop(com, prop);
    pane->selectable = 0;
    return pane;
}

int win_prog_set (pane_t *pane, const char *text, int percent)
{
    win_progress_t *win = WPROG_HANDLE(pane);

    if (percent > 100) {
        percent = 100;
    }

    if (win->percent && win->percent == percent) {
        return 0;
    }

    win_user_handler(&win->win, NULL);

    if (text) {
        gui_com_set_dirty(pane->parent, win->title);
        gui_print(win->title, "[%s]", text);
    }
    gui_com_set_dirty(pane->parent, win->bar);
    gui_print(win->bar, "%02.03i%%", percent);
    win->percent = percent;
    gui_set_repaint(pane, 1);
    return 1;
}

static int win_prog_act_resp (pane_t *pane, component_t *com, void *user)
{
    return 0;
}

static int win_prog_repaint (pane_t *pane, component_t *com, void *user)
{
    win_progress_t *win = WPROG_HANDLE(pane);
    dim_t dim = {0, 0, com->dim.w, com->dim.h};
    int compl, left;

    if (win->percent == 100) {
        gui_com_fill(com, COLOR_BLUE);
    } else if (win->percent >= 0) {
        compl = (dim.w * win->percent) / 100;
        left = dim.w - compl;

        dim.w = compl;
        gui_rect_fill(pane->parent, &com->dim, &dim, COLOR_BLUE);
        dim.x = compl;
        dim.w = left;
        gui_rect_fill(pane->parent, &com->dim, &dim, COLOR_WHITE);
    } else {
        gui_com_fill(com, COLOR_RED);
    }
    return 1;
} 

typedef struct {
    win_t win;
    component_t *com;
    void *cache;
} win_jpeg_t;

static inline win_jpeg_t *
WJPEG_HANDLE (void *pane)
{
    return (win_jpeg_t *)WIN_HANDLE_CHECK(pane, WIN_JPEG);
}

pane_t *win_new_jpeg (gui_t *gui, prop_t *prop, int x, int y, int w, int h)
{
    pane_t *pane = gui_create_pane(gui, "jpeg", sizeof(win_jpeg_t));
    win_jpeg_t *win = (win_jpeg_t *)(pane + 1);
    component_t *com;
    dim_t *dim = &gui->dim;

    gui_set_panexy(gui, pane, dim->x + (dim->w / 2), dim->y, dim->w / 2, dim->h);
    pane->selectable = 0;

    com = gui_create_comp(gui, "pic", "");
    gui_set_comp(pane, com, 0, 0, -1, -1);
    prop->user_draw = 1;
    gui_set_prop(com, prop);
    win->com = com;
    win->win.pane = pane;
    win->win.type = WIN_JPEG;

    return pane;
}

rawpic_t *win_jpeg_decode (pane_t *pane, const char *path)
{
    const uint32_t memsize = (2 << 20);
    win_jpeg_t *win = WJPEG_HANDLE(pane);

    if (NULL == win->cache) {
        win->cache = heap_alloc_shared(memsize);
    }
    if (NULL == win->cache) {
        return NULL;
    }
    return jpeg_2_rawpic(path, win->cache, memsize);
}

void win_jpeg_set_rawpic (pane_t *pane, void *pic, int top)
{
    win_jpeg_t *win = WJPEG_HANDLE(pane);
    gui_set_pic(win->com, pic, top);
}

#endif /*BSP_DRIVER*/

