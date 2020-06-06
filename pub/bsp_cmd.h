#ifndef __BSP_CMD_H__
#define __BSP_CMD_H__

#ifdef __cplusplus
    extern "C" {
#endif

#define CMD_MAX_NAME        (16)
#define CMD_MAX_PATH        (128)
#define CMD_MAX_BUF         (256)
#define CMD_MAX_ARG         (16)
#define CMD_MAX_RECURSION    (16)

typedef int (*cmd_func_t) (int, const char **);
typedef int (*cmd_handler_t) (const char *, int);

typedef enum {
    CMDERR_OK,
    CMDERR_GENERIC,
    CMDERR_NOARGS,
    CMDERR_NOPATH,
    CMDERR_NOCORE,
    CMDERR_INVPARM,
    CMDERR_PERMISS,
    CMDERR_UNKNOWN,
    CMDERR_MAX,
} cmd_errno_t;

typedef enum {
    DVAR_FUNC,
    DVAR_INT32,
    DVAR_FLOAT,
    DVAR_STR,
} dvar_obj_t;

typedef struct {
    void *ptr;
    uint16_t ptrsize;
    uint16_t size;
    dvar_obj_t type;
    uint32_t flags;
} cmdvar_t;

typedef struct bsp_cmd_api_s {
    bspdev_t dev;
    int (*var_reg) (cmdvar_t *, const char *);
    int (*var_int32) (int32_t *, const char *);
    int (*var_float) (float *, const char *);
    int (*var_str) (char *, int, const char *);
    int (*var_func) (cmd_func_t, const char *);
    int (*var_rm) (const char *);
    int (*exec) (const char *, int);
    void (*tickle) (void);
} bsp_cmd_api_t;

#define BSP_CMD_API(func) ((bsp_cmd_api_t *)(g_bspapi->cmd))->func

#if BSP_INDIR_API

#define cmd_register_var      BSP_CMD_API(var_reg)
#define cmd_register_i32      BSP_CMD_API(var_int32)
#define cmd_register_float    BSP_CMD_API(var_float)
#define cmd_register_str      BSP_CMD_API(var_str)
#define cmd_register_func     BSP_CMD_API(var_func)
#define cmd_execute            BSP_CMD_API(exec)
#define cmd_tickle             BSP_CMD_API(tickle)

#else /*BSP_INDIR_API*/

int cmd_register_var (cmdvar_t *var, const char *name);
int cmd_register_i32 (int32_t *var, const char *name);
int cmd_register_float (float *var, const char *name);
int cmd_register_str (char *str, int len, const char *name);
int cmd_register_func (cmd_func_t func, const char *name);
int cmd_execute (const char *, int);
void cmd_tickle (void);

int cmd_init (void);
void cmd_deinit (void);
int cmd_unregister (const char *);
int cmd_execute (const char *, int);
int cmd_exec_dsr (const char *, const char *);

void bsp_stdin_register_if (int (*) (int , const char **));
void bsp_stdin_unreg_if (int (*) (int , const  char **));
void bsp_stout_unreg_if (cmd_func_t clbk);
void bsp_stdout_register_if (cmd_func_t clbk);

#endif /*BSP_INDIR_API*/

#ifdef __cplusplus
    }
#endif

#endif /*__BSP_CMD_H__*/

