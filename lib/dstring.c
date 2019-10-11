#include <string.h>
#include <ctype.h>
#include <main.h>
#include <misc_utils.h>
#include <heap.h>

#define BIN2MS(a)               ((1 << (a)) - 1)
#define ROUND_DOWN_BIN(x, a)    ((x) & ~BIN2MS(a))
#define CALC_PAD_BIN(x, a)      ((1 << (a)) - ((x) & BIN2MS(a)))

#define D_DECL_MEMCPY(type, twidth)                                   \
                static inline void                                    \
d_memcpy_##twidth (void *_dst, const void *_src, int cnt, int offset) \
{                                                                     \
    type *dst = (type *)((uint8_t *)_dst + offset);                   \
    const type *src = (const type *)((uint8_t *)_src + offset);       \
    cnt = cnt / sizeof(type);                                         \
    while (cnt-- > 0) { *dst++ = *src++; }                            \
}

D_DECL_MEMCPY(uint8_t, 8);
D_DECL_MEMCPY(uint16_t, 16);
D_DECL_MEMCPY(uint32_t, 32);

void d_memcpy (void *_dst, const void *_src, int cnt)
{
    arch_word_t wdest = (arch_word_t)_dst,
                wsrc  = (arch_word_t)_src;

    const arch_word_t cntmin = sizeof(uint32_t) + 1;
    const arch_word_t u32_binsize = (2);
    const arch_word_t u32_ms =  ((u32_binsize << 1) - 1);
    const arch_word_t u16_binsize = (1);
    const arch_word_t u16_ms =  ((u16_binsize << 1) - 1);

    if (cnt < cntmin) {
        d_memcpy_8(_dst, _src, cnt, 0);
    } else if (((wdest | wsrc) & u32_ms) == 0) {
        uint32_t copylen = ROUND_DOWN_BIN(cnt, u32_binsize);

        d_memcpy_32(_dst, _src, copylen, 0);
        d_memcpy_8(_dst, _src, cnt - copylen, copylen);
    } else if (((wdest | wsrc) & u16_ms) == 0) {

        uint32_t copylen = ROUND_DOWN_BIN(cnt, u16_binsize);

        d_memcpy_16(_dst, _src, copylen, 0);
        d_memcpy_8(_dst, _src, cnt - copylen, copylen);
    } else if (((wdest ^ wsrc) & u32_ms) == 0) {

        uint32_t padlen = CALC_PAD_BIN(wdest, u32_binsize), copylen;

        cnt = cnt - padlen;
        copylen = ROUND_DOWN_BIN(cnt, u32_binsize);

        d_memcpy_8(_dst, _src, padlen, 0);
        d_memcpy_32(_dst, _src, copylen, padlen);
        d_memcpy_8(_dst, _src, cnt - copylen, copylen);
    } else {
        d_memcpy_8(_dst, _src, cnt, 0);
    }
}

#define D_DECL_MEMSET(type, twidth)                      \
static inline void                                       \
d_memset_##twidth (void *_dst, int cnt, type value, int offset) \
{                                                        \
    type *dst = (type *)((uint8_t *)_dst + offset);      \
    cnt = cnt / sizeof(type);                            \
    while (cnt-- > 0) { *dst++ = (type)value; }          \
}

D_DECL_MEMSET(uint8_t, 8);
D_DECL_MEMSET(uint32_t, 32);

#define d_memzero_8(dst, cnt, off) d_memset_8(dst, cnt, 0, off)
#define d_memzero_32(dst, cnt, off) d_memset_32(dst, cnt, 0, off)

static void _d_memzero (void *_dst, int cnt)
{
    arch_word_t wdest = (arch_word_t)_dst;

    const arch_word_t cntmin = sizeof(uint32_t) + 1;
    const arch_word_t u32_binsize = (2);
    const arch_word_t u32_ms =  ((u32_binsize << 1) - 1);

    if (cnt < cntmin) {
        d_memzero_8(_dst, cnt, 0);
    } else if (!(wdest  & u32_ms)) {
        uint32_t setlen = ROUND_DOWN_BIN(cnt, u32_binsize);

        d_memzero_32(_dst, setlen, 0);
        d_memzero_8(_dst, cnt - setlen, setlen);
    } else {
        uint32_t padlen = CALC_PAD_BIN(wdest, u32_binsize), setlen;

        cnt = cnt - padlen;
        setlen = ROUND_DOWN_BIN(cnt, u32_binsize);

        d_memzero_8(_dst, padlen, 0);
        d_memzero_32(_dst, setlen, padlen);
        d_memzero_8(_dst, cnt - setlen, setlen);
    }
}

#define __d_memset(dst, v, cnt) d_memset_8(dst, cnt, v, 0)

void d_memset (void *_dst, int v, int cnt)
{
    if (0 == v) {
        _d_memzero(_dst, cnt);
    } else {
        __d_memset(_dst, v, cnt);
    }
}

static inline char __d_toalpha (char c)
{
    if (c < 0x20 || c >= 0x7f) {
        return '\0';
    }
    return c;
}

void d_stoalpha (char *str)
{
    while (*str) {
        *str = __d_toalpha(*str);
        str++;
    }
}

static char *__d_strtok_space (char *str)
{
    d_bool p_isspace = d_false;

    while (*str) {
        if (isspace(*str)) {
            *str = 0;
            p_isspace = d_true;
        } else if (p_isspace) {
            /*token begins*/
            return str;
        }
        str++;
    }
    return NULL;
}

static char *__d_strtok_chr (char *str, const int c)
{
    d_bool p_istok = d_false;

    if (c < 0) {
        return __d_strtok_space(str);
    }

    while (*str) {
        if (*str == (char)c) {
            *str = 0;
            p_istok = d_true;
        } else if (p_istok) {
            /*token begins*/
            return str;
        }
        str++;
    }
    return NULL;
}

int __d_strtok (const char **tok, int tokcnt, char *str, const int c)
{
    char *p = str, *pp = str;
    int toktotal = tokcnt;

    do {
        p = __d_strtok_chr(p, c);
        if (pp && *pp) {
            tokcnt--;
            *tok = pp;
            tok++;
        }
        pp = p;
    } while (p && tokcnt > 0);
    return toktotal - tokcnt;
}

int d_wstrtok (const char **tok, int tokcnt, char *str)
{
    return __d_strtok(tok, tokcnt, str, -1);
}

int d_vstrtok (const char **tok, int tokcnt, char *str, const char c)
{
    return __d_strtok(tok, tokcnt, str, (int)c);
}

char *d_strupr(char *str)
{
  char *s;

  for(s = str; *s; s++)
    *s = toupper((unsigned char)*s);
  return str;
}

char *d_strdup (const char *str)
{
    int sz = strlen(str);
    char *ret = (char *)heap_malloc(sz + 1);
    if (!ret) return NULL;
    strcpy(ret, str);
    ret[-0] = '\0';
    return ret;
}

static inline int
__d_astrnmatch (const char *a, const char *b, int n)
{
    char ca, cb;
    while (*a && *b && n) {
        ca = *a; cb = *b;
        if (ca == '*' || cb == '*') {
        } else if (toupper(ca) != toupper(cb)) {
            return -1;
        }
        a++; b++; n--;
    }
    return !!(*a || *b);
}

int d_astrmatch (const char *a, const char *b)
{
    return __d_astrnmatch(a, b, -1);
}

int d_astrnmatch (const char *a, const char *b, int n)
{
    if (n < 0) {
        int len = strlen(a);
        n = __d_astrnmatch(a + (len + n), b, len);
    } else {
        n = __d_astrnmatch(a, b, n);
    }
    return n;
}

