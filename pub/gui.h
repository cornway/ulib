#include <stdint.h>
#include <misc_utils.h>
#include <debug.h>

#include "../gui/colors.h"

/*Must be up-to-date with HAL routines!!!*/
enum {
    GUI_CENTER_ALIGN = 0x1,
    GUI_RIGHT_ALIGN = 0x2,
    GUI_LEFT_ALIGN = 0x3,
};

typedef uint32_t rgba_t;

#define GUI_MAX_NAME 16
#define GUI_MAX_STRBUF 256

#define GUI_KEY_UP      ('u')
#define GUI_KEY_DOWN    ('d')
#define GUI_KEY_LEFT    ('l')
#define GUI_KEY_RIGHT   ('r')
#define GUI_KEY_RETURN  ('e')
#define GUI_KEY_SELECT  ('x')
#define GUI_KEY_1       ('1')
#define GUI_KEY_2       ('2')
#define GUI_KEY_3       ('3')
#define GUI_KEY_4       ('4')
#define GUI_KEY_5       ('5')
#define GUI_KEY_6       ('6')
#define GUI_KEY_7       ('7')
#define GUI_KEY_8       ('8')
#define GUI_KEY_9       ('9')
#define GUI_KEY_10      ('10')

typedef struct {
    void *data;
    uint16_t w, h;
    uint8_t colormode;
    uint8_t alpha;
    uint8_t sprite: 1;
} rawpic_t;

typedef enum {
    GUIX_INVTYPE,
    GUIX_COMP,
    GUIX_PANE,
    GUIX_MAXTYPE,
} gui_obj_type_t;

typedef struct {
    int x, y, w, h;
} dim_t;

typedef struct {
    union {
        struct {int x, y;};
        struct {int w, h;};
    };
} point_t;

struct component_s;
struct pane_s;
struct gui_s;

typedef int (*comp_handler_t) (struct pane_s *p, struct component_s *c, void *user);

typedef struct {
    const void *font;
    uint8_t w, h;
} fontprop_t;

typedef struct prop_s {
    fontprop_t fontprop;
    rgba_t fcolor, bcolor;
    d_bool ispad;
    d_bool user_draw;
    const char *name;
} prop_t;

#define GUI_COMMON(type, parenttype)  \
struct {                                \
    struct type *next;                  \
    struct parenttype *parent;         \
    rgba_t bcolor, fcolor;              \
    void *ctxt, *user;                  \
    dim_t dim;                          \
    uint16_t type:  6,                  \
             ispad:   1,                \
             userdraw: 1;               \
}

typedef struct component_s {
    GUI_COMMON(component_s, pane_s);

    const void *font;
    comp_handler_t act, release;
    comp_handler_t draw;
    rawpic_t *pic;

    uint16_t text_offset;
    uint16_t text_size;
    uint16_t text_index;

    uint16_t glow:     8,
             showname: 1,
             selectable: 1,
             pictop: 1,
             reserved: 5;
    uint32_t userflags;

    char name[GUI_MAX_NAME];
    char text[1];
} component_t;

/*must be about fixed size*/
typedef struct pane_s {
    GUI_COMMON(pane_s, gui_s);

    struct pane_s *child;
    component_t *onfocus;
    
    component_t *head;
    struct pane_s *selected_next;

    char name[GUI_MAX_NAME];

    uint8_t selectable: 1,
            repaint: 1;
} pane_t;

typedef struct gui_bsp_api_s {
    struct mem_s {
        void *(*alloc) (uint32_t);
        void (*free) (void *);
    } mem;
} gui_bsp_api_t;

#define gui_bsp_alloc(gui, size) \
    (gui)->bspapi.mem.alloc(size)

#define gui_bsp_free(gui, ptr) \
    (gui)->bspapi.mem.free(ptr)

typedef struct gui_s {
    dim_t dim;
    void *ctxt;
    uint8_t destroy;
    pane_t *head;
    pane_t *selected_head, *selected_tail;
    uint32_t repainttsf;
    int32_t framerate;
    int32_t dbglvl;
    const void *font;
    char name[GUI_MAX_NAME];

    gui_bsp_api_t bspapi;
} gui_t;

typedef enum {
    GUINONE,
    GUIACT,
    GUIRELEASE,
    GUIEVTMAX
} GUIEVENT;

typedef struct gevt_s {
    point_t p;
    GUIEVENT e;
    char sym;
    uint8_t symbolic: 1;
} gevt_t;

typedef struct app_gui_api_s {
    bspdev_t dev;
} app_gui_api_t;

#if BSP_INDIR_API

#else /*BSP_INDIR_API*/

#endif

#define gui_print(com, args...) gui_printxy(com, 0, 0, args)

d_bool dim_check (const dim_t *d, const point_t *p);
void dim_place (dim_t *d, const dim_t *s);
void dim_tolocal (dim_t *d, const dim_t *s);
void dim_tolocal_p (point_t *d, const dim_t *s);
void dim_get_origin (point_t *d, const dim_t *s);
void dim_set_origin (dim_t *d, const point_t *s);
void dim_set_right (dim_t *d, dim_t *s);
void dim_set_left (dim_t *d, dim_t *s);
void dim_set_top (dim_t *d, dim_t *s);
void dim_set_bottom (dim_t *d, dim_t *s);
void dim_extend (dim_t *dest, dim_t *ref);
d_bool dim_check_intersect (dim_t *durty, dim_t *dim);

void gui_init (gui_t *gui, const char *name, uint8_t framerate,
                dim_t *dim, gui_bsp_api_t *bspapi);

void gui_destroy (gui_t *gui);
pane_t *gui_create_pane (gui_t *gui, const char *name, int extra);
void gui_set_pane (gui_t *gui, pane_t *pane);
void gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h);
void gui_set_child (pane_t *parent, pane_t *child);

void gui_set_pic (component_t *com, rawpic_t *pic, int top);

component_t *gui_create_comp (gui_t *gui, const char *name, const char *text);
void gui_set_comp (pane_t *pane, component_t *c, int x, int y, int w, int h);
void gui_set_prop (component_t *c, prop_t *prop);

void gui_set_text (component_t *com, const char *text, int x, int y);
void gui_printxy (component_t *com, int x, int y, const char *fmt, ...) PRINTF_ATTR(4, 5);
int gui_apendxy (component_t *com, int x, int y, const char *fmt, ...) PRINTF_ATTR(4, 5);
int gui_draw_string (component_t *com, int line, rgba_t textcolor, const char *str);
int gui_draw_string_c (component_t *com, int line, rgba_t textcolor, const char *str);

void gui_rect_fill (gui_t *gui, dim_t *dest, dim_t *rect, rgba_t color);
void gui_com_fill (component_t *com, rgba_t color);

void gui_com_clear (component_t *com);
void gui_draw (gui_t *gui, int force);
int gui_send_event (gui_t *gui, gevt_t *evt);
void gui_wakeup_com (gui_t *gui, component_t *com);
pane_t *gui_get_pane_4_name (gui_t *gui, const char *name);
component_t *gui_search_com (pane_t *pane, const char *name);
void gui_select_pane (gui_t *gui, pane_t *pane);
pane_t *gui_release_pane (gui_t *gui);
pane_t *gui_select_next_pane (gui_t *gui);
component_t *gui_set_focus (pane_t *pane, component_t *com, component_t *prev);
component_t *gui_set_next_focus (gui_t *gui);
void gui_com_set_dirty (gui_t *gui, component_t *com);
void gui_pane_set_dirty (gui_t *gui, pane_t *pane);

void gui_get_font_prop (fontprop_t *prop, const void *font);
const void *gui_get_font_4_size (gui_t *gui, int size, int bestmatch);

typedef int (*win_alert_hdlr_t) (const component_t *);
typedef int (*win_user_hdlr_t) (gevt_t *);

void win_con_set_clbk (void *pane, comp_handler_t h);
void win_set_user_clbk (void *pane, win_user_hdlr_t clbk);

pane_t *win_new_allert (gui_t *gui, int w, int h, const char *msg);
int win_alert (pane_t *pane, const char *text,
                  win_alert_hdlr_t accept, win_alert_hdlr_t decline);

int win_close_allert (pane_t *pane);

pane_t *win_new_console (gui_t *gui, prop_t *prop, int x, int y, int w, int h);
int win_con_append (pane_t *pane, const char *str, rgba_t textcolor);
int win_con_get_dim (pane_t *pane, int *w, int *h);
int win_con_print_at (pane_t *pane, int x, int y, const char *str, rgba_t textcolor);
int win_con_printline (pane_t *pane, int y,
                                    const char *str, rgba_t textcolor);
int win_con_printline_c (pane_t *pane, int y,
                                    const char *str, rgba_t textcolor);
void win_con_clear (pane_t *pane);

pane_t *win_new_progress (gui_t *gui, prop_t *prop, int x, int y, int w, int h);
int win_prog_set (pane_t *pane, const char *text, int percent);

pane_t *win_new_jpeg (gui_t *gui, prop_t *prop, int x, int y, int w, int h);
rawpic_t *win_jpeg_decode (pane_t *pane, const char *path);
void win_jpeg_set_rawpic (pane_t *pane, void *pic, int top);


