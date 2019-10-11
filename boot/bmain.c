#include <string.h>
#include <debug.h>
#include "int/boot_int.h"
#include "../int/term_int.h"
#include <gfx.h>
#include <gui.h>
#include <dev_io.h>
#include <nvic.h>
#include <input_main.h>
#include <lcd_main.h>
#include <heap.h>
#include <bsp_sys.h>
#include <bsp_cmd.h>
#include <audio_main.h>
#include <misc_utils.h>
#include <jpeg.h>
#include <bconf.h>

static gui_t gui;
static pane_t *pane_console, *pane_selector,
              *pane_alert, *pane_progress,
              *pane_jpeg;

static int b_exec_selector_cursor = 0;

static void boot_gui_bsp_init (gui_t *gui);

typedef struct {
    int (*func) (const char *);
    bsp_exec_file_type_t file_type;
    const char *file_ext;
} b_exec_type_func_t;

static int b_execute_boot (const char *path);

static const b_exec_type_func_t b_exec_type_func_tbl[] =
{
    [BIN_FILE] = {b_execute_boot, BIN_FILE, ".bin"},
    [BIN_LINK] = {b_execute_link, BIN_LINK, ".als"},
    [BIN_CMD] =  {b_execute_boot, BIN_CMD,  ".cmd"},
    {NULL, BIN_MAX, ""},
};

complete_ind_t complete_ind_clbk = NULL;

bsp_exec_file_type_t
bsp_bin_file_fmt_supported (const char *in)
{
    int i;
    const b_exec_type_func_t *desc_type;

    for (i = 0; i < arrlen(b_exec_type_func_tbl); i++) {

        desc_type = &b_exec_type_func_tbl[i];
        if (!d_astrnmatch(in, desc_type->file_ext, 0 - strlen(desc_type->file_ext))) {
            return b_exec_type_func_tbl[i].file_type;
        }
    }
    return BIN_MAX;
}

int
bsp_setup_bin_param (exec_desc_t *bin)
{
    int f, size;
    uint8_t buf[sizeof(bin->parm)];

    size = d_open(bin->path, &f, "r");
    if (f < 0) {
        return -1;
    }
    d_read(f, buf, sizeof(buf));
    bhal_setup_bin_param(&bin->parm, buf, size);
    d_close(f);
    return 0;
}

void
bsp_load_exec_title_pic (const char *dirpath, exec_desc_t *bin, const char *path)
{
    if (path && path[0]) {
        bin->pic = win_jpeg_decode(pane_jpeg, path);
    } else {
        char name[D_MAX_NAME] = {0}, tpath[D_MAX_PATH];
        const char *argv[2] = {0};

        strcpy(name, bin->name);
        d_vstrtok(argv, 2, name, '.');

        snprintf(tpath, sizeof(tpath), "%s/%s.jpg", dirpath, argv[0]);
        bin->pic = win_jpeg_decode(pane_jpeg, tpath);
    }
}

int b_execute_link (const char *path)
{
    int f;
    char strbuf[CMD_MAX_BUF];

    d_open(path, &f, "r");
    if (f < 0) {
        dprintf("%s() : fail\n", __func__);
        return -CMDERR_NOPATH;
    }
    if (!d_gets(f, strbuf, sizeof(strbuf))) {
        return -CMDERR_NOCORE;
    }
    d_close(f);
    cmd_exec_dsr(strbuf, "");
    return CMDERR_OK;
}

int b_execute_cmd (const char *path)
{
    cmd_exec_dsr("runcmd", path);
    return CMDERR_OK;
}

static int
b_execute_boot (const char *path)
{
    return cmd_exec_dsr("boot", path);
}

static int
b_execute_app (exec_desc_t *bin)
{
    int ret = -CMDERR_INVPARM;

    if (bin->filetype < BIN_MAX) {
        ret = b_exec_type_func_tbl[bin->filetype].func(bin->path);
    }
    return ret;
}

static void
b_selector_move_cursor (int dir)
{
    int max = bres_get_executables_num();

    if (dir < 0) {
        b_exec_selector_cursor--;
        if (b_exec_selector_cursor < 0) {
            b_exec_selector_cursor = max - 1;
        }
    } else {
        b_exec_selector_cursor++;
        if (b_exec_selector_cursor == max) {
            b_exec_selector_cursor = 0;
        }
    }
}

static int
b_gui_print_apps_list (pane_t *pane)
{
    const int prsize = 20;
    const exec_desc_t *binarray[32];
    int start, selected, maxy, size, i, dummy;
    char str[CMD_MAX_BUF];

    win_con_clear(pane);
    win_con_get_dim(pane, &dummy, &maxy);

    assert(arrlen(binarray) > maxy);
    bres_querry_executables_for_range(binarray, &start, b_exec_selector_cursor, &size, maxy);

    selected = maxy / 2;
    assert(selected >= start);

    for (i = 0; i < start; i++) {
        win_con_printline(pane, i, "    *", COLOR_BLACK);
    }
    for (i = start + size; i < maxy; i++) {
        win_con_printline(pane, i, "    *", COLOR_BLACK);
    }
    for (i = start; i < start + size; i++) {
        if (i == selected) {
            int off = prsize - strlen(binarray[i - start]->name);

            d_memset(str, ' ', off);
            snprintf(str + off, sizeof(str) - off, "%s <", binarray[i - start]->name);
            win_con_printline(pane, i, str, COLOR_RED);
        } else {
            snprintf(str, sizeof(str), "%*.*s", -prsize, -prsize, binarray[i - start]->name);
            win_con_printline(pane, i, str, COLOR_BLACK);
        }
    }
    win_jpeg_set_rawpic(pane_jpeg, binarray[selected - start]->pic, 1);
    return CMDERR_OK;
}

static int
b_console_user_clbk (gevt_t *evt)
{
    if (evt) {
        bsfx_start_sound(SFX_MOVE, 100);
        return 1;
    }
    return 0;
}

static int
b_alert_user_clbk (gevt_t *evt)
{
    if (evt->sym == GUI_KEY_LEFT ||
        evt->sym == GUI_KEY_RIGHT) {
        bsfx_start_sound(SFX_MOVE, 100);
    }
    return 1;
}

static int
b_alert_decline_clbk (const component_t *com)
{
    bsfx_start_sound(SFX_NOWAY, 100);
    return 1;
}

static int
b_alert_accept_clbk (const component_t *com)
{
    int ret;

    bsfx_title_music(0, 0);
    bsfx_start_sound(SFX_START_APP, 100);
    d_sleep(100);

    gui_pane_set_dirty(&gui, pane_progress);
    gui_select_pane(&gui, pane_progress);

    ret = b_execute_app(bres_get_executable_for_num(b_exec_selector_cursor));
    if (ret != CMDERR_OK) {
        bsfx_title_music(1, 40);
    } else {
        gui.destroy = 1;
    }
    return ret;
}

static int
b_gui_input_event_hanlder (pane_t *pane, component_t *com, void *user)
{
    gevt_t *evt = (gevt_t *)user;
    int dir = -1;

    switch (evt->sym) {
        case GUI_KEY_UP:
            dir = 1;
        case GUI_KEY_DOWN:
            b_selector_move_cursor(dir);
            b_gui_print_apps_list(pane);
            bsfx_start_sound(SFX_MOVE, 100);
        break;
        case GUI_KEY_RETURN:
        {
            char buf[CMD_MAX_BUF];
            exec_desc_t *bin = bres_get_executable_for_num(b_exec_selector_cursor);

            bsfx_start_sound(SFX_WARNING, 100);
            snprintf(buf, sizeof(buf), "Open :\n\'%s\'\nApplicarion?", bin->name);
            win_alert(pane_alert, buf, b_alert_accept_clbk, b_alert_decline_clbk);
        }
        break;
    }
    return 1;
}

static int gui_stdout_hook (int argc, const char **argv)
{
    assert(argc > 0);
    win_con_append(pane_console, argv[0], COLOR_WHITE);
    return 0;
}

static inline void
boot_gui_set_proc_stat (const char *msg, int percent)
{
    if (win_prog_set(pane_progress, msg, percent)) {
        gui_draw(&gui, 1);
        d_sleep(10);
    }
}

void boot_gui_preinit (void)
{
    dim_t dim;
    prop_t prop = {0};

    boot_gui_bsp_init(&gui);

    prop.fcolor = COLOR_WHITE;
    prop.ispad = d_false;
    prop.user_draw = d_false;

    prop.bcolor = COLOR_BLACK;
    prop.name = "console";
    prop.fontprop.font = gui_get_font_4_size(&gui, 16, 1);
    pane_console = win_new_console(&gui, &prop, gui.dim.x, gui.dim.y, gui.dim.w, gui.dim.h);
    gui_select_pane(&gui, pane_console);
    win_set_user_clbk(pane_console, b_console_user_clbk);

    prop.fontprop.font = NULL;
    prop.bcolor = COLOR_LGREY;
    prop.name = "binselect";
    pane_selector = win_new_console(&gui, &prop, gui.dim.x, gui.dim.y, gui.dim.w / 2, gui.dim.h);
    win_con_set_clbk(pane_selector, b_gui_input_event_hanlder);

    pane_alert = win_new_allert(&gui, gui.dim.w / 2, 250, "Console90");
    win_set_user_clbk(pane_alert, b_alert_user_clbk);

    dim = pane_alert->dim;
    dim_set_top(&dim, &dim);
    dim.h = 64;
    dim.y += 10;
    prop.name = "progbar1";
    prop.fcolor = COLOR_BLACK;
    prop.bcolor = COLOR_WHITE;
    prop.fontprop.font = gui_get_font_4_size(&gui, 24, 1);
    pane_progress = win_new_progress(&gui, &prop, dim.x, dim.y, dim.w, dim.h);

    prop.name = "pane_jpeg";
    prop.bcolor = COLOR_LGREY;
    pane_jpeg = win_new_jpeg(&gui, &prop, gui.dim.x + (gui.dim.w / 2),
                                          gui.dim.y, gui.dim.w / 2, gui.dim.h);
    gui_set_child(pane_selector, pane_jpeg);

    bsp_stdout_register_if(gui_stdout_hook);

    gui_select_pane(&gui, pane_progress);
}

void b_dev_deinit_callback (void)
{
    bres_exec_unload();
    bsp_stout_unreg_if(gui_stdout_hook);
    gui_destroy(&gui);
}

int boot_main (int argc, const char **argv)
{
extern void (*dev_deinit_callback) (void);

    jpeg_init("");
    boot_gui_preinit();

    boot_gui_set_proc_stat("Loading...", 0);
    bres_exec_scan_path(boot_gui_set_proc_stat, "");
    b_gui_print_apps_list(pane_selector);

    boot_gui_set_proc_stat("Sfx...", 0);
    bsfx_sound_precache(boot_gui_set_proc_stat, 50);
    dprintf("Ready\n");

    boot_gui_set_proc_stat("Muic...", 75);
    bsfx_title_music(1, 40);
    boot_gui_set_proc_stat(BOOT_STARTUP_MUSIC_PATH, 99);
    boot_gui_set_proc_stat("Done...", 100);
    gui_release_pane(&gui);
    gui_select_pane(&gui, pane_selector);

    complete_ind_clbk = boot_gui_set_proc_stat;
    dev_deinit_callback = b_dev_deinit_callback;

    while (!gui.destroy)
    {
        bsp_tickle();
        gui_draw(&gui, 0);
        input_proc_keys(NULL);
    }
    bsp_tickle();
    complete_ind_clbk = NULL;
    assert(0);
    return 0;
}

static void gui_char_input_event_wrap (gui_t *gui, gevt_t *evt)
{
    if (evt->e != GUIACT) {
        return;
    }

    switch (evt->sym) {
        case GUI_KEY_LEFT:
        case GUI_KEY_RIGHT:
            if (gui->selected_head == pane_alert) {
                gui_send_event(gui, evt);
                break;
            }
        case GUI_KEY_SELECT:
            bsfx_start_sound(SFX_SCROLL, 100);
            gui_select_next_pane(gui);
        break;
        default:
            gui_send_event(gui, evt);
        break;
    }
}

void boot_deliver_input_event (void *_evt)
{
    gevt_t *evt = (gevt_t *)_evt;

    if (evt->symbolic) {
        gui_char_input_event_wrap(&gui, evt);
    } else {
        gui_send_event(&gui, evt);
    }
}

static void boot_gui_bsp_init (gui_t *gui)
{
#define GUI_FPS (25)
    screen_t s = {0};
    dim_t dim = {0};

    gui_bsp_api_t bspapi =
    {
        .mem =
            {
                .alloc = heap_alloc_shared,
                .free = heap_free,
            },
    };

    vid_wh(&s);
    dim.w = s.width;
    dim.h = s.height;

    gui_init(gui, "gui", GUI_FPS, &dim, &bspapi);
extern int boot_input_init (void);
    boot_input_init();
#undef GUI_FPS
}
