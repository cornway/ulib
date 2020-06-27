
#ifndef __SWTIM_H__
#define __SWTIM_H__

#ifdef __cplusplus
    extern "C" {
#endif

typedef void (*swtim_chandler_t) (void *arg);

void swtim_core_init (void);
void *swtim_init (swtim_chandler_t h, void *arg, uint32_t flags);
void *swtim_timeout (void *inst, uint32_t ms);
void *swtim_untimeout (void *inst);
void *swtim_sync (void *inst);
void swtim_tick (void);


#ifdef __cplusplus
    }
#endif

#endif /* __SWTIM_H__ */
