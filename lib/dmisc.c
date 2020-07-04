#include <ctype.h>

#include <arch.h>
#include "../../common/int/bsp_cmd_int.h"

#include <term.h>
#include <bsp_cmd.h>
#include <misc_utils.h>
#include <debug.h>
#include <bsp_sys.h>

#define TERM_MAX_CMD_BUF 256

#define INOUT_MAX_FUNC 4
#define MAX_TOKENS 16

typedef enum {
    INVALID,
    SOLID,
    SQUASH,
} tkntype_t;

typedef int (*tknmatch_t) (char c, int *state);

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
    uint8_t *startptr = (uint8_t *)data;

    if (!rowlength) {
        rowlength = len;
    }
    maxrows = len / rowlength;
    colmax = min(len, rowlength);

    for (row = 0; row < maxrows; row++) {

        printfmt(stream, "[0x%p : 0x%p] : ", startptr, startptr + colmax);
        startptr += colmax;

        for (col = 0; col < colmax; col++) {
            printfmt(stream, "0x%02x ", data[row * rowlength + col]);
        }
        printfmt(stream, "\n");
    }
    len = len - (row * rowlength);
    assert(len >= 0 && len < rowlength);

    if (len) {
        printfmt(stream, "[0x%p : 0x%p] : ", startptr, startptr + colmax);
        for (col = 0; col < len; col++) {
            printfmt(stream, "0x%02x ", data[row * rowlength + col]);
        }
        printfmt(stream, "\n");
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
    uint8_t *startptr = (uint8_t *)data;

    len = len / sizeof(uint32_t);

    if (rowlength <= 0) {
        rowlength = len;
    }
    maxrows = len / rowlength;
    colmax = min(len, rowlength);

    for (row = 0; row < maxrows; row++) {
        printfmt(stream, "[0x%p : 0x%p] : ", startptr, startptr + colmax * sizeof(uint32_t));

        startptr += colmax;

        for (col = 0; col < colmax; col++) {
            printfmt(stream, "0x%08x ", data[row * rowlength + col]);
        }
        printfmt(stream, "\n");
    }
    len = len - (row * rowlength);
    assert(len >= 0 && len < rowlength);

    if (len) {
        printfmt(stream, "[0x%p : 0x%p] : ", startptr, startptr + colmax);
        for (col = 0; col < len; col++) {
            printfmt(stream, "0x%08x ", data[row * rowlength + col]);
        }
        printfmt(stream, "\n");
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

void binprint (const void *data, int bits)
{
    int i;
    uint32_t word = *(uint32_t *)data;

    switch (bits) {
        case 8:
        case 16:
        case 32:
            break;
        default:
            assert(0);
    }
    __printfmt(-1, "0b");
    for (i = 0; i < bits; i++) {
        if (word & (1 << i)) {
            __printfmt(-1, "1");
        } else {
            __printfmt(-1, "0");
        }
    }
    __printfmt(-1, "\n");
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

void __bindump (printfmt_t printfmt, int stream,
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

int __tty_append_crlf (char * buf, int pos)
{
    buf[pos] = '\n';
    buf[pos + 1] = '\r';
    return pos + 2;
}

