#include <string.h>

#include <debug.h>
#include "int/boot_int.h"

#include <misc_utils.h>
#include <dev_io.h>
#include <bsp_cmd.h>
#include <bconf.h>

typedef struct {
    char titlepic[32];
} exec_conf_t;

static exec_desc_t *
bres_collect_exec_path (const char *dirpath, const char *path,
                        const char *originname, bsp_exec_file_type_t type);

static void bres_exec_list_link (exec_desc_t *bin);
static void bres_exec_pack_list (void);

void *
bres_cache_file_2_mem (const bsp_heap_api_t *heapapi, const char *path, int *binsize)
{
    int f;
    int fsize, size;
    void *cache;

    fsize = d_open(path, &f, "r");
    if (f < 0) {
        dprintf("%s() : failed to open : \'%s\'\n", __func__, path);
        return NULL;
    }
    size = ROUND_UP(fsize, 32);
    cache = heapapi->malloc(size);
    assert(cache);

    dprintf("caching bin : dest <0x%p> size [0x%08x]\n", cache, fsize);

    if (d_read(f, cache, fsize) < fsize) {
        dprintf("%s() : missing part\n", __func__);
        heapapi->free(cache);
        cache = NULL;
    } else {
        *binsize = fsize;
    }
    d_close(f);

    if (cache)
        dprintf("Cache done : <0x%p> : 0x%08x bytes\n", cache, fsize);

    return cache;
}

void bres_exec_scan_path (void (*statfunc) (const char *, int), const char *path)
{
    fobj_t fobj;
    int dir, bindir;
    char buf[BOOT_MAX_PATH];
    char binpath[BOOT_MAX_NAME];
    d_bool filesfound = 0;

    dir = d_opendir(path);
    if (dir < 0) {
        dprintf("%s() : fail\n", __func__);
    }
    while (d_readdir(dir, &fobj) >= 0) {

        if (fobj.attr.dir) {
            fobj_t binobj;
            snprintf(buf, sizeof(buf), "%s/%s/"BOOT_BIN_DIR_NAME, path, fobj.name);

            bindir = d_opendir(buf);
            if (bindir < 0) {
                continue;
            }
            while (d_readdir(bindir, &binobj) >= 0) {
                if (binobj.attr.dir == 0) {
                    bsp_exec_file_type_t type = bsp_bin_file_fmt_supported(binobj.name);

                    if (type != BIN_MAX && type != BIN_FILE) {
                        snprintf(binpath, sizeof(binpath), "%s/%s", buf, binobj.name);
                        if (bres_collect_exec_path(buf, binpath, binobj.name, type)) {
                            filesfound++;
                            if (statfunc) {
                                statfunc(binobj.name, filesfound);
                            }

                        }
                    }
                }
            }
            if (!filesfound) {
                dprintf("%s() : no exe here : \'%s\'\n", __func__, buf);
            }
            d_closedir(bindir);
        }
    }
    d_closedir(dir);
    dprintf("%s() : Found : %u files\n", __func__, filesfound);

    bres_exec_pack_list();
}

static exec_desc_t *
__bsp_setup_desc (const char *dirpath, exec_desc_t *bin, const char *path,
                        const char *originname, bsp_exec_file_type_t type)
{
    snprintf(bin->name, sizeof(bin->name), "%s", originname);
    snprintf(bin->path, sizeof(bin->path), "%s", path);
    bin->filetype = type;
    return bin;
}

static void
bsp_preparse_exec (exec_desc_t *bin, exec_conf_t *conf)
{
    int f;
    char buf[128], *p;
    const char *argv[16];
    int argc;

    d_open(bin->path, &f, "r");
    if (f < 0) {
        return;
    }
    p = d_gets(f, buf, sizeof(buf));
    if (p) {
        p = d_gets(f, buf, sizeof(buf));
    }
    if (!p) {
        goto done;
    }
    argc = d_wstrtok(argv, arrlen(argv), buf);
    while (argc--) {
        if (strcmp(argv[argc], "--titlepic") == 0) {
            snprintf(conf->titlepic, sizeof(conf->titlepic), "%s", argv[argc + 1]);
        }
    }
done:
    d_close(f);
}

exec_desc_t *
bsp_setup_bin_desc (const char *dirpath, exec_desc_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    bin = __bsp_setup_desc(dirpath, bin, path, originname, type);
    if (bsp_setup_bin_param(bin) < 0) {
        return NULL;
    }
    bsp_load_exec_title_pic(dirpath, bin, NULL);
    return bin;
}

exec_desc_t *
bsp_setup_bin_link (const char *dirpath, exec_desc_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    exec_conf_t conf = {{0}};

    bin = __bsp_setup_desc(dirpath, bin, path, originname, type);
    if (!bin) {
        return NULL;
    }
    bsp_preparse_exec(bin, &conf);
    bsp_load_exec_title_pic(dirpath, bin, conf.titlepic);
    bin->parm.progaddr = 0;
    return bin;
}

static exec_desc_t *
bres_collect_exec_path (const char *dirpath, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    exec_desc_t *bin = (exec_desc_t *)heap_malloc(sizeof(*bin));

    if (type == BIN_FILE) {
        bin = bsp_setup_bin_desc(dirpath, bin, path, originname, type);
        assert(bin);
    } else if (type == BIN_LINK) {
        bin = bsp_setup_bin_link(dirpath, bin, path, originname, type);
    }
    if (bin) {
        bres_exec_list_link(bin);
    }
    return bin;
}

static exec_desc_t *bres_exec_list_head = NULL;
static int bres_exec_list_size = 0;
static exec_desc_t **bres_exec_packed_list_ptr = NULL;

static void
bres_exec_list_link (exec_desc_t *bin)
{
    bin->next = bres_exec_list_head;
    bres_exec_list_head = bin;
    bres_exec_list_size++;
}

static void
bres_exec_list_unlink (exec_desc_t *del)
{
    exec_desc_t *bin = bres_exec_list_head, *prev = NULL;

    while (bin) {

        if (del == bin) {
            if (prev) {
                prev->next = bin->next;
            } else {
                bres_exec_list_head = bin->next;
            }
            bres_exec_list_size--;
            heap_free(bin);
            break;
        }
        prev = bin;
        bin = bin->next;
    }
}

static void bres_exec_pack_list (void)
{
    int i = 0;
    exec_desc_t *bin = bres_exec_list_head;

    bres_exec_packed_list_ptr = heap_malloc(sizeof(exec_desc_t *) * (bres_exec_list_size + 1));
    assert(bres_exec_packed_list_ptr);

    while (bin) {
        bres_exec_packed_list_ptr[i] = bin;
        bin->id = i;
        bin = bin->next;
        i++;
    }
}

void bres_exec_unload (void)
{
    exec_desc_t *bin = bres_exec_list_head, *prev;

    while (bin) {
        prev = bin;
        bin = bin->next;
        heap_free(prev);
    }
    bres_exec_list_head = NULL;
    bres_exec_list_size = 0;
    if (bres_exec_packed_list_ptr) {
        heap_free(bres_exec_packed_list_ptr);
        bres_exec_packed_list_ptr = NULL;
    }
}

int bres_rebuild_exec_list (void)
{
    if (bres_exec_packed_list_ptr) {
        heap_free(bres_exec_packed_list_ptr);
    }
    bres_exec_pack_list();
    return CMDERR_OK;
}

int bres_dump_exec_list (int argc, const char **argv)
{
    int i = 0;
    exec_desc_t *bin = bres_exec_list_head;
    boot_bin_parm_t *parm;

    dprintf("%s() :\n", __func__);
    while (bin) {
        parm = &bin->parm;

        if (bin->filetype == BIN_FILE) {
            dprintf("[%i] %s, %s, <0x%p> %u bytes\n",
                i++, bin->name, bin->path, (void *)parm->progaddr, parm->size);
        } else if (bin->filetype == BIN_LINK) {
            dprintf("[%i] %s, %s, <link file>\n",
                i++, bin->name, bin->path);
        } else {
            dprintf("unable to handle : [%i] %s, %s, ???\n",
                i++, bin->name, bin->path);
            assert(0)
        }
        bin = bin->next;
    }
    return argc;
}

void
bres_querry_executables_for_range (const exec_desc_t **binarray, int *pstart,
                                               int cursor, int *size, int maxsize)
{
    int tmp, top, bottom, start, i;
    int mid = maxsize / 2;
    assert(bres_exec_list_size && bres_exec_list_size > cursor);
    assert(bres_exec_packed_list_ptr);

    top = (bres_exec_list_size - cursor);
    if (top > mid) {
        top = mid;
    }
    bottom = cursor;
    if (bottom > mid) {
        bottom = mid;
    }
    tmp = top + bottom;
    start = cursor - bottom;

    *size = tmp;
    *pstart = mid - bottom;
    for (i = 0; i < tmp; i++, start++) {
        binarray[i] = bres_exec_packed_list_ptr[start];
    }
}

void *bres_get_executable_for_num (int num)
{
    return bres_exec_packed_list_ptr[num];
}

int bres_get_executables_num (void)
{
    return bres_exec_list_size;
}

