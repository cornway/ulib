#if defined(BSP_DRIVER)

#include <string.h>
#include <ctype.h>

#include "int/term_int.h"
#include "int/bsp_cmd_int.h"

#include <bsp_cmd.h>
#include <misc_utils.h>
#include <debug.h>


#define TERM_MAX_CMD_BUF 256

#define INOUT_MAX_FUNC 4
#define MAX_TOKENS 16

typedef int (*tknmatch_t) (char c, int *state);

typedef enum {
    INVALID,
    SOLID,
    SQUASH,
} tkntype_t;

static cmd_func_t serial_rx_clbk[INOUT_MAX_FUNC] = {NULL};
static cmd_func_t *last_rx_clbk = &serial_rx_clbk[0];

static cmd_func_t serial_tx_clbk[INOUT_MAX_FUNC] = {NULL};
static cmd_func_t *last_tx_clbk = &serial_tx_clbk[0];

inout_clbk_t inout_early_clbk = NULL;

int str_remove_spaces (char *str)
{
    char *dest = str, *src = str;
    while (*src) {
        if (!isspace(*src)) {
            *dest = *src;
            dest++;
        }
        src++;
    }
    *dest = 0;
    return (src - dest);
}

int str_parse_tok (const char *str, const char *tok, uint32_t *val)
{
    int len = strlen(tok), ret = 0;
    tok = strstr(str, tok);
    if (!tok) {
        return ret;
    }
    str = tok + len;
    if (*str != '=') {
        ret = -1;
        goto done;
    }
    str++;
    if (!sscanf(str, "%u", val)) {
        ret = -1;
    }
    ret = 1;
done:
    if (ret < 0) {
        dprintf("invalid value : \'%s\'\n", tok);
    }
    return ret;
}

/*Split 'quoted' text with other*/
static int
str_tkn_split (const char **argv, tknmatch_t match, int *argc,
                    char *str, uint32_t size)
{
    int matchcnt = 0, totalcnt = 0;
    int tknstart = 1, dummy = 0;
    int maxargs = *argc;

    assert(size > 0);
    argv[totalcnt++] = str;
    while (size && *str && totalcnt < maxargs) {

        if (match(*str, &dummy)) {
            *str = 0;
            if (!tknstart) {
                argv[totalcnt++] = str + 1;
            } else {
                argv[totalcnt++] = str + 1;
                matchcnt++;
            }
            tknstart = 1 - tknstart;
        }
        str++;
        size--;
    }
    *argc = totalcnt;
    return matchcnt;
}

static int
str_tkn_continue (const char **dest, const char **src, tknmatch_t tkncmp,
                     tkntype_t *flags, int argc, int maxargc)
{
    int i;
    int tmp, total = 0;

    for (i = 0; i < argc && maxargc > 0; i++) {
        tmp = 0;
        if (flags[i] == SQUASH) {
            /*Split into*/
            tmp = d_wstrtok(dest, maxargc, (char *)src[i]);
        } else {
            dest[0] = src[i];
            tmp = 1;
        }
        dest += tmp;
        total += tmp;
        maxargc -= tmp;
    }
    return total;
}


/*brief : remove empty strings - ""*/
static int str_tkn_clean (const char **dest, const char **src, tkntype_t *flags, int argc)
{
    int i = 0, maxargc = argc;

    for (i = 0; i < maxargc; i++) {
        if (src[i][0]) {
            dest[0] = src[i];
            /*each even - squashable, odd - solid, e.g - " ", ' ', { }, ..*/
            if (i & 0x1) {
                *flags = SOLID;
            } else {
                *flags = SQUASH;
            }
            flags++;
            dest++;
        } else {
            argc--;
        }
    }
    dest[0] = NULL;
    return argc;
}

int quotematch (char c, int *state)
{
    if (c == '\'' || c == '\"') {
        return 1;
    }
    return 0;
}

/*brief : convert " 1 '2 3 4' 5 6 " -> {"1", "2 3 4", "5 6"}*/
/*argc - in argc*/
/*argv - output buffer*/
/*ret - result argc*/
static int str_tokenize_string (int argc, const char **argv,
                                       char *str, uint32_t size)
{
    const char *tempbuf[MAX_TOKENS] = {0},
               *splitbuf[MAX_TOKENS] = {0},
               **tempptr = &tempbuf[0],
               **splitptr = &splitbuf[0];
    tkntype_t flags[MAX_TOKENS];

    int totalcnt;

    if (argc < 2) {
        return -1;
    }

    totalcnt = MAX_TOKENS;
    str_tkn_split(splitptr, quotematch, &totalcnt, str, size);

    totalcnt = str_tkn_clean(tempptr, splitptr, flags, totalcnt);

    totalcnt = str_tkn_continue(splitptr, tempptr, quotematch, flags, totalcnt, MAX_TOKENS);

    totalcnt = str_tkn_clean(argv, splitptr, flags, totalcnt);
    return totalcnt;
}

static inline int
str_tokenize_parms (int argc, const char **argv,
                    char *buf, uint32_t size)
{
    return str_tokenize_string(argc, argv, buf, size);
}

#define STR_MAXARGS     9
#define STR_ARGKEY      "$"
#define STR_ARG_TKNSIZE (sizeof(STR_ARGKEY) + 1 - 1) /*$[0..9]*/
#define STR_ARGV(str)    strstr(str, STR_ARGKEY)
#define STR_ARGN(str)   ((str)[1] - '0')

static inline int __strncpy (char *dest, const char *src, int maxlen)
{
    int n = 0;

    while (*src && maxlen) {
        *dest = *src;
        dest++; src++;
        n++;
        maxlen--;
    }
    return n;
}

int str_insert_args (char *dest, const char *src, int argc, const char **argv)
{
    const char *srcptr = src, *argptr;
    char *dstptr = dest;
    int n, argn;

    while (*srcptr) {
        argptr = STR_ARGV(srcptr);
        if (argptr) {
            n = argptr - srcptr;
            if (n) {
                n = __strncpy(dstptr, srcptr, n);
                dstptr += n;
                srcptr = argptr + STR_ARG_TKNSIZE;
            }
            argn = STR_ARGN(argptr);
            if (argn >= argc) {
                return -CMDERR_NOARGS;
            }
            n = __strncpy(dstptr, argv[argn], -1);
            dstptr += n;
            *dstptr = ' ';
            dstptr++;
        } else {
            n = __strncpy(dstptr, srcptr, -1);
            dstptr += n;
            break;
        }
    }
    *dstptr = 0;
    return CMDERR_OK;
}

int str_check_args_present (const char *str)
{
    str = STR_ARGV(str);
    if (str) {
        return 1;
    }
    return 0;
}

void bsp_stdin_register_if (cmd_func_t clbk)
{
    if (last_rx_clbk == &serial_rx_clbk[INOUT_MAX_FUNC]) {
        return;
    }
    *last_rx_clbk++ = clbk;
}

void bsp_stdout_register_if (cmd_func_t clbk)
{
    if (last_tx_clbk == &serial_tx_clbk[INOUT_MAX_FUNC]) {
        return;
    }
    *last_tx_clbk++ = clbk;
}

static cmd_func_t *__bsp_inout_unreg_if (cmd_func_t *begin, cmd_func_t *end,
                                    cmd_func_t *last, cmd_func_t h)
{
    cmd_func_t *first = &begin[0];
    if (!last) {
        return NULL;
    }
    while (first != &end[0]) {
        if (*first == h) {
            *first = *(--last);
            *last = NULL;
            return NULL;
        }
        first++;
    }
    return last;
}

void bsp_stdin_unreg_if (cmd_func_t clbk)
{
    last_rx_clbk = __bsp_inout_unreg_if(&serial_rx_clbk[0], &serial_rx_clbk[INOUT_MAX_FUNC],
                          last_rx_clbk, clbk);
}

void bsp_stout_unreg_if (cmd_func_t clbk)
{
    last_tx_clbk = __bsp_inout_unreg_if(&serial_tx_clbk[0], &serial_tx_clbk[INOUT_MAX_FUNC],
                          last_tx_clbk, clbk);
}

static int
__bsp_stdin_handle_argv
                (cmd_func_t *_begin, cmd_func_t *end,
                int argc, const char **argv);

static int
__bsp_stdin_iter_fwd_ascii (cmd_func_t *begin, cmd_func_t *end,
                                      char *buf, uint32_t size);

static int
__bsp_stdout_iter_fwd_raw
                (cmd_func_t *_begin, cmd_func_t *end,
                int argc, const char **argv);

cmd_func_t stdin_redir = NULL;

cmd_func_t bsp_stdin_stash (cmd_func_t func)
{
    cmd_func_t tmp = stdin_redir;
    stdin_redir = func;
    return tmp;
}

cmd_func_t bsp_stdin_unstash (cmd_func_t func)
{
    cmd_func_t tmp = stdin_redir;
    stdin_redir = func;
    return tmp;
}

int bsp_inout_forward (char *buf, int size, char dir)
{
    int offset = 0, err = 0;
    const char *bufptr = (const char *)buf;

    if (dir == '<' && stdin_redir) {
        offset = size;
        size = stdin_redir(size, &bufptr);
        offset = offset - size;
    }
    if (!size) {
        return CMDERR_OK;
    }
    switch (dir) {
        case '>':
            err = __bsp_stdout_iter_fwd_raw(&serial_tx_clbk[0], last_tx_clbk, 1, &bufptr);
        break;
        case '<':
            err = __bsp_stdin_iter_fwd_ascii(&serial_rx_clbk[0], last_rx_clbk, buf, size);
        break;
        default: assert(0);
    }
    return err;
}


static int
__bsp_stdout_iter_fwd_raw (cmd_func_t *_begin, cmd_func_t *end, int argc, const char **argv)
{
    int err = 0;
    cmd_func_t *begin = _begin;
    while (*begin && begin < end) {
        err = (*begin)(argc, argv);
        if (err) {
            dprintf("%s() : fail: %i\n", __func__, err);
            break;
        }
        begin++;
    }
    return err;
}

static int
__bsp_stdin_iter_fwd_ascii (cmd_func_t *begin, cmd_func_t *end,
                                     char *buf, uint32_t size)
{
    const char *argvbuf[MAX_TOKENS] = {NULL};
    const char **argv = (const char **)&argvbuf[0];
    int argc = MAX_TOKENS;

    argc = str_tokenize_parms(argc, &argv[0], buf, size);
    return __bsp_stdin_handle_argv(begin, end, argc, argv);
}

static int __bsp_stdin_handle_argv 
                (cmd_func_t *_begin, cmd_func_t *end,
                int argc, const char **argv)
{
    cmd_func_t *begin = _begin;
    int prev_argc = argc;

    while (d_true) {

        do {
            argv = &argv[prev_argc - argc];
            argc = (*begin)(argc, argv);
            if (argc <= 0) {
                break;
            }
            begin++;
        } while (argc > 0 && begin < end);

        if (argc <= 0) {
            break;
        }
        if (prev_argc == argc) {
            dprintf("%s() : unknown arguments\n", __func__);
            break;
        }
        prev_argc = argc;
        begin = _begin;
    }
    return argc;
}

static int __printfmt (int unused, const char *fmt, ...)
{
    va_list         argptr;
    int size;

    va_start (argptr, fmt);
    size = dvprintf(fmt, argptr);
    va_end (argptr);
    return size;
}

int __hexdump_u8 (printfmt_t printfmt, int stream, const uint8_t *data, int len, int rowlength)
{
    int col, row, colmax, bytescnt = len;
    int maxrows;
    uint8_t *startptr = (uint8_t *)data, *endptr = startptr;

    if (!rowlength) {
        rowlength = len;
    }
    maxrows = len / rowlength;
    colmax = min(len, rowlength);

    for (row = 0; row < maxrows; row++) {

        startptr += colmax;
        endptr = startptr + colmax;
        printfmt(stream, "[0x%p : 0x%p] : ", startptr, endptr);

        for (col = 0; col < colmax; col++) {
            printfmt(stream, "0x%02x ", data[row + row * rowlength]);
        }
        printfmt(stream, "\n");
    }
    len = len - (row * rowlength);
    assert(len >= 0 && len < rowlength);

    if (len) {
        startptr += colmax;
        endptr = startptr + colmax;
        printfmt(stream, "[0x%p : 0x%p] : ", startptr, endptr);
        for (col = 0; col < len; col++) {
            printfmt(stream, "0x%02x ", data[row + row * rowlength]);
        }
    }
    return bytescnt;
}

void hexdump_u8 (const void* data, int len, int rowlength)
{
    __hexdump_u8(__printfmt, -1, (uint8_t *)data, len, rowlength);
}

int __hexdump_le_u32 (printfmt_t printfmt, int stream,
                              const uint32_t *data, int len, int rowlength)
{
    int col, row, colmax, bytescnt = len;
    int maxrows;
    uint8_t *startptr = (uint8_t *)data, *endptr = startptr;

    len = len / sizeof(uint32_t);

    if (!rowlength) {
        rowlength = len;
    }
    maxrows = len / rowlength;
    colmax = min(len, rowlength);

    for (row = 0; row < maxrows; row++) {

        startptr += colmax;
        endptr = startptr + colmax;
        printfmt(stream, "[0x%p : 0x%p] : ", startptr, endptr);

        for (col = 0; col < colmax; col++) {
            printfmt(stream, "0x%08x ", data[row + row * rowlength]);
        }
        printfmt(stream, "\n");
    }
    len = len - (row * rowlength);
    assert(len >= 0 && len < rowlength);

    if (len) {
        startptr += colmax;
        endptr = startptr + colmax;
        printfmt(stream, "[0x%p : 0x%p] : ", startptr, endptr);
        for (col = 0; col < len; col++) {
            printfmt(stream, "0x%08x ", data[row + row * rowlength]);
        }
    }
    return bytescnt;
}

void hexdump_le_u32 (const void *data, int len, int rowlength)
{
    __hexdump_le_u32(__printfmt, -1, (const uint32_t *)data, len, rowlength);
}

void hexdump (const void *data, int bits, int len, int rowlength)
{
    __hexdump(__printfmt, -1, (const uint32_t *)data, bits, len, rowlength);
}

void __hexdump (printfmt_t printfmt, int stream,
                  const void *data, int bits, int len, int rowlength)
{
    switch(bits) {
        case 8: __hexdump_u8(printfmt, -1, (uint8_t *)data, len, rowlength);
        break;
        case 16: dprintf("hexdump_u16: not yet");
        break;
        case 32: __hexdump_le_u32(printfmt, -1, (uint32_t *)data, len, rowlength);
        break;
        case 64: dprintf("hexdump_u64: not yet");
        break;
        default: assert(0);
        break;
    }
}

#endif

