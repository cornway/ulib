#ifndef __TERM_INT_H__
#define __TERM_INT_H__

#define TTY_OUT '>'
#define TTY_IN '<'

int d_astrnmatch (const char *a, const char *b, int n);
int str_parse_tok (const char *str, const char *tok, uint32_t *val);
int d_vstrtok (const char **tok, int tokcnt, char *str, const char c);
int d_wstrtok (const char **tok, int tokcnt, char *str);
void d_stoalpha (char *str);
#define str_replace_2_ascii(str) d_stoalpha(str)
int str_insert_args (char *dest, const char *src, int argc, const char **argv);
int str_check_args_present (const char *str);

void hexdump_u8 (const void *data, int len, int rowlength);
void hexdump_le_u32 (const void *data, int len, int rowlength);

typedef int (*printfmt_t) (int, const char *, ...);

int __hexdump_u8 (printfmt_t printfmt, int stream, const uint8_t *data, int len, int rowlength);
int __hexdump_le_u32 (printfmt_t printfmt, int stream, const uint32_t *data, int len, int rowlength);
void __hexdump (printfmt_t printfmt, int stream,
                  const void *data, int bits, int len, int rowlength);
void hexdump (const void *data, int bits, int len, int rowlength);
void binprint (const void *data, int bits);


int bsp_inout_forward (char *buf, int size, char dir);

static inline uint8_t __tty_is_crlf_char (char c)
{
    return (c == '\r') ? 0x1 :
           (c == '\n') ? 0x3 : 0;
}

#endif /* __TERM_INT_H__ */

