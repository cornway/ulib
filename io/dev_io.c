
#if defined(BSP_DRIVER)
#include <string.h>
#include <ctype.h>

#include <ff.h>
#include <ff_gen_drv.h>

#include "../../common/int/sd_diskio.h"
#include <misc_utils.h>
#include <dev_io.h>
#include <dev_conf.h>
#include <debug.h>
#include <heap.h>

#ifndef DEVIO_READONLY
#warning "DEVIO_READONLY is undefined, using TRUE"
#define DEVIO_READONLY 1
#endif

#ifndef MAX_HANDLES
#warning "MAX_HANDLES is undefined, using 3"
#define MAX_HANDLES    6
#endif

static FATFS DEV_Fs ALIGN(8);
static char DEV_Path[4] = {0};

typedef struct {
    int type;
    uint32_t refcnt;
    uint32_t ptr[];
} fobjhdl_t;

typedef struct {
    FIL file;
} ALIGN(8) fhandle_t;

typedef struct {
    DIR dir;
    FILINFO fn;
} ALIGN(8) dirhandle_t;

static fobjhdl_t *handles[MAX_HANDLES * 2];

static int _devio_mount (char *path);
static void _devio_unmount (char *path);

#define alloc_file(nump) allochandle(handles, nump, 0)
#define alloc_dir(nump) allochandle(handles, nump, 1)

#define getfile(num) gethandle(handles, num)
#define getdir(num) gethandle(handles, num)

#define freefile(num) releasehandle(handles, num)
#define freedir(num) releasehandle(handles, num)

#define chkfile(num) chk_handle(handles, num)
#define chkdir(num) chk_handle(handles, num)

static inline int chk_handle (fobjhdl_t **hdls, int num)
{
    fobjhdl_t *hdl;

    if (num < 0) {
        return 0;
    }
    hdl = hdls[num];
    return hdl->refcnt;
}

static inline void *allochandle (fobjhdl_t **hdls, int *num, int dir)
{
    int i, memsize = dir ? sizeof(dirhandle_t) : sizeof(fhandle_t);
    fobjhdl_t *h;
    *num = -1;

    for (i=0 ; i<MAX_HANDLES * 2; i++) {
        h = hdls[i];
        if (NULL == h) {
            h = heap_malloc(memsize);
            if (NULL == h) {
                break;
            }
            h->refcnt = 1;
            *num = i;
            hdls[i] = h;
            return h->ptr;
        }
    }
    return NULL;
}

static inline void *gethandle (fobjhdl_t **hdls, int num)
{
    return hdls[num] ? hdls[num]->ptr : NULL;
}

static inline void releasehandle (fobjhdl_t **hdls, int handle)
{
    heap_free(hdls[handle]);
    hdls[handle] = NULL;
}

int FR_2_ERR (FRESULT res)
{
    int err = -DERR_INT;
    switch(res) {
        case FR_OK:
            err = DERR_OK;
        break;
        case FR_DISK_ERR:
        case FR_INT_ERR:
        case FR_NOT_READY:
            err = -DERR_INT;
        break;
        case FR_NO_FILE:
        case FR_NO_PATH:
            err = -DERR_NOPATH;
        break;
        case FR_INVALID_NAME:
        case FR_DENIED:
        case FR_EXIST:
        case FR_INVALID_OBJECT:
        case FR_WRITE_PROTECTED:
        case FR_INVALID_DRIVE:
            err = -DERR_INVPARAM;
        break;
        case FR_NOT_ENABLED:
        case FR_NO_FILESYSTEM:
            err = -DERR_NOFS;
        break;
        case FR_MKFS_ABORTED:
        case FR_TIMEOUT:
        case FR_LOCKED:
        case FR_NOT_ENOUGH_CORE:
        case FR_TOO_MANY_OPEN_FILES:
        case FR_INVALID_PARAMETER:
            err = -DERR_NORES;
    }
    return err;
}

const char *_fres_to_string (FRESULT res)
{
#define errorcase(res) case res: str = #res ; break
const char *str;
    switch (res) {
        errorcase(FR_OK);
        errorcase(FR_DISK_ERR);
        errorcase(FR_INT_ERR);
        errorcase(FR_NOT_READY);
        errorcase(FR_NO_FILE);
        errorcase(FR_NO_PATH);
        errorcase(FR_INVALID_NAME);
        errorcase(FR_DENIED);
        errorcase(FR_EXIST);
        errorcase(FR_INVALID_OBJECT);
        errorcase(FR_WRITE_PROTECTED);
        errorcase(FR_INVALID_DRIVE);
        errorcase(FR_NOT_ENABLED);
        errorcase(FR_NO_FILESYSTEM);
        errorcase(FR_MKFS_ABORTED);
        errorcase(FR_TIMEOUT);
        errorcase(FR_LOCKED);
        errorcase(FR_NOT_ENOUGH_CORE);
        errorcase(FR_TOO_MANY_OPEN_FILES);
        errorcase(FR_INVALID_PARAMETER);
    }
    return str;
#undef errorcase
}

int dev_io_init (void)
{
    return _devio_mount(DEV_Path);
}

void dev_io_deinit (void)
{
extern void SD_Deinitialize(void);

    dprintf("%s() :\n", __func__);
    _devio_unmount(DEV_Path);
    SD_Deinitialize();
}


int d_open (const char *path, int *hndl, char const * att)
{
    int ret = DERR_OK;
    BYTE mode = 0;
    d_bool extend = d_true;
    FRESULT res;

    if (!att) {
        return -DERR_INVPARAM;
    }

    alloc_file(hndl);
    if (*hndl < 0) {
        return -DERR_NORES;
    }
    while (att)
    switch (*att) {
        case 'r':
            mode |= FA_READ | FA_OPEN_EXISTING;
            extend = d_false;
            att = NULL;
        break;
#if !DEVIO_READONLY
        case 'w':
            mode |= FA_WRITE | FA_OPEN_EXISTING;
            att = NULL;
        break;
#endif
        case '+':
            att++;
            extend = d_false;
            mode |= FA_CREATE_ALWAYS;
        break;
        default:
            att = NULL;
        break;
    }
    if (mode & FA_CREATE_ALWAYS) {
        res = f_unlink(path);
        if (res != FR_OK) {
            dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        }
    }
    res = f_open(getfile(*hndl), path, mode);
    if (res != FR_OK) {
        dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        freefile(*hndl);
        *hndl = -1;
        return FR_2_ERR(res);
    }
    if (extend) {
        ret = d_seek(*hndl, 0, DSEEK_END);
    }
    return ret < 0 ? ret : d_size(*hndl);
}

int d_size (int hndl)
{
    return f_size((FIL *)getfile(hndl));
}

int d_tell (int h)
{
    return (int)f_tell((FIL *)getfile(h));
}

void d_close (int h)
{
    FRESULT res;
    assert(chkfile(h));
    res = f_close(getfile(h));
    if (res != FR_OK) {
        dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
    }
    freefile(h);
}

int d_unlink (const char *path)
{
    FRESULT res;
    res = f_unlink(path);
    if (res != FR_OK) {
        dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        return FR_2_ERR(res);
    }
    return 0;
}

int d_seek (int handle, int position, uint32_t mode)
{
    FRESULT res = FR_OK;

    if (handle < 0) {
        return -DERR_INVPARAM;
    }
    switch (mode) {
        case DSEEK_SET:
            res = f_lseek(getfile(handle), position);
        break;
        case DSEEK_CUR:
            res = f_lseek(getfile(handle), d_tell(handle) + position);
        break;
        case DSEEK_END:
            res = f_lseek(getfile(handle), d_size(handle) + position);
        break;
        default:
            dprintf("Unknown SEEK mode : %u\n", mode);
            res = FR_INVALID_PARAMETER;
        break;
    }
    return FR_2_ERR(res);
}

int d_eof (int handle)
{
    if (handle < 0) {
        return -1;
    }
    return f_eof((FIL *)getfile(handle));
}

int d_read (int handle, PACKED void *dst, int count)
{
    PACKED char *data;
    UINT done = 0;
    FRESULT res = FR_NOT_READY;

    if ( handle >= 0 ) {
        data = dst;
        res = f_read(getfile(handle), data, count, &done);
    }
    if (res != FR_OK) {
        dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        return FR_2_ERR(res);
    }
    return done;
}

char *d_gets (int handle, PACKED char *dst, int count)
{
    if (f_gets(dst, count, getfile(handle)) == NULL) {
        return NULL;
    }
    return dst;
}

char d_getc (int h)
{
    char c;
    UINT btr;
    FRESULT res;
    res = f_read(getfile(h), &c, 1, &btr);
    if (res != FR_OK) {
        dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        return 0xff;
    }
    return c;
}

int d_write (int handle, PACKED const void *src, int count)
{
#if !DEVIO_READONLY
    PACKED const char *data;
    UINT done;
    FRESULT res = FR_NOT_READY;

    if ( handle >= 0 ) {
        data = src;
        res = f_write (getfile(handle), data, count, &done);
    }
    if (res != FR_OK) {
        dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        return FR_2_ERR(res);
    }
    return done;
#else
    return count;
#endif
}

int d_mkdir (const char *path)
{
#if !DEVIO_READONLY
    FRESULT res = f_mkdir(path);
    if ((res != FR_OK) && (res != FR_EXIST)) {
        return FR_2_ERR(res);
    }
#endif
    return 0;
}

int d_opendir (const char *path)
{
    int h;
    FRESULT res;
    alloc_dir(&h);
    if (h < 0) {
        dprintf("%s() : too many open files\n", __func__);
        return -1;
    }
    res = f_opendir(getdir(h), path);
    if (res != FR_OK) {
        if (res != FR_NO_PATH) {
            dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        }
        freedir(h);
        return FR_2_ERR(res);
    }
    return h;
}

int d_closedir (int dir)
{
    FRESULT res;

    assert(chkdir(dir));
    res = f_closedir(getdir(dir));
    if (res != FR_OK) {
        dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
    }
    freedir(dir);
    return 0;
}

static void inline
d_get_attr (fobj_t *obj, uint32_t data)
{
    memset(&obj->attr, 0, sizeof(obj->attr));
    if (data & AM_RDO) {
        obj->attr.rdonly = 1;
    }
    if (data & AM_HID) {
        obj->attr.hidden = 1;
    }
    if (data & AM_SYS) {
        obj->attr.system = 1;
    }
    if (data & AM_ARC) {
        obj->attr.archive = 1;
    }
    if (data & AM_DIR) {
        obj->attr.dir = 1;
    }
}

/*
bit15:9
    Year origin from 1980 (0..127)
bit8:5
    Month (1..12)
bit4:0
    Day (1..31) 
*/
static inline void
d_get_date (d_date_t *date, uint32_t data)
{
    date->year = (data >> 9) & 0x7f;
    date->month = (data >> 5) & 0xf;
    date->day = (data >> 0) & 0x1f;
}

/*
bit15:11
    Hour (0..23)
bit10:5
    Minute (0..59)
bit4:0
    Second / 2 (0..29) 
*/
static void inline
d_get_time (d_time_t *time, uint32_t data)
{
    time->h = (data >> 11) & 0x1f;
    time->m = (data >> 5) & 0x3f;
    time->s = (data >> 0) & 0x1f;
}

int d_readdir (int dir, fobj_t *fobj)
{
    
    dirhandle_t *dh = (dirhandle_t *)getdir(dir);
    f_readdir(getdir(dir) , &dh->fn);
    if (dh->fn.fname[0] == 0) {
        return -1;
    }
    snprintf(fobj->name, sizeof(fobj->name), "%s", dh->fn.fname);
    fobj->size = dh->fn.fsize;
    d_get_attr(fobj, dh->fn.fattrib);
    d_get_date(&fobj->date, dh->fn.fdate);
    d_get_time(&fobj->time, dh->fn.ftime);
    return dir;
}

uint32_t d_time (void)
{
    return HAL_GetTick();
}

static int _devio_mount (char *path)
{
    int i;
    FRESULT res;

    d_memzero(&DEV_Fs, sizeof(DEV_Fs));

    if(FATFS_LinkDriver(&SD_Driver, path)) {
        return -DERR_NOFS;
    }

    res = f_mount(&DEV_Fs, path, 1);
    if(res != FR_OK) {
        dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        return FR_2_ERR(res);
    }
    dprintf("FS mount : \'%s\'\n", path);

    d_memzero(handles, sizeof(handles));
    return 0;
}

static void _devio_unmount (char *path)
{
    FATFS_UnLinkDriver(path);
}

#endif
