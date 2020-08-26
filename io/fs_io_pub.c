#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <dev_io.h>
#include <heap.h>


void *d_fcache (const char *path, size_t *size)
{
    int f;
    void *p;

    *size = d_open(path, &f, "r");
    if (f < 0) {
        return NULL;
    }

    p = heap_alloc_shared(*size);
    if (!p) {
        d_close(f);
        return NULL;
    }
    d_read(f, p, *size);
    d_close(f);
    return p;
}

int d_dirlist (const char *path, fiter_t *flist)
{
    fobj_t obj;
    int dir;

    dir = d_opendir(path);
    if (dir < 0) {
        return -1;
    }
    while (d_readdir(dir, &obj) >= 0) {
        if (flist->clbk(obj.name, (obj.attr.dir) ? d_true : d_false)) {
            break;
        }
    }
    d_closedir(dir);
    return 0;
}

int d_printf (int handle, const char *fmt, ...)
{
    va_list args;
    int size;

    va_start(args, fmt);
    size = _d_vprintf(handle, fmt, args);
    va_end(args);

    return size;
}

int _d_vprintf (int h, const char *fmt, va_list argptr)
{
    char            string[1024];
    int size = 0;
    size = vsnprintf(string, sizeof(string), fmt, argptr);
    size = d_write(h, string, size);
    return size;
}


