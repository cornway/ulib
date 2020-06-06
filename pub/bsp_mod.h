#ifndef __BSP_MOD_INT_H__
#define __BSP_MOD_INT_H__

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct bsp_mod_api_s {
    bspdev_t dev;
    void *(*insert) (const bsp_heap_api_t *, const char *, const char *);
    int (*remove) (const char *);
    int (*probe) (const char *);
    int (*register_api) (const char *, const void *, int);
    const void *(*get_api) (const char *, int *);
} bsp_mod_api_t;

#define BSP_MOD_API(func) ((bsp_mod_api_t *)(g_bspapi->mod))->func

#if BSP_INDIR_API

#define bspmod_init              BSP_MOD_API(dev.init)
#define bspmod_deinit            BSP_MOD_API(dev.deinit)
#define bspmod_conf              BSP_MOD_API(dev.conf)
#define bspmod_info              BSP_MOD_API(dev.info)
#define bspmod_priv              BSP_MOD_API(dev.priv)
#define bspmod_insert            BSP_MOD_API(insert)
#define bspmod_remove            BSP_MOD_API(remove)
#define bspmod_probe             BSP_MOD_API(probe)
#define bspmod_register_api      BSP_MOD_API(register_api)
#define bspmod_get_api           BSP_MOD_API(get_api)

#else /*BSP_INDIR_API*/

void *bspmod_insert (const bsp_heap_api_t *, const char *, const char *);
int bspmod_remove (const char *);
int bspmod_probe (const char *name);
int bspmod_register_api (const char *, const void *, int);
const void *bspmod_get_api (const char *name, int *apisize);

#endif /*BSP_INDIR_API*/


#ifdef __cplusplus
    }
#endif

#endif /*__BSP_MOD_INT_H__*/
