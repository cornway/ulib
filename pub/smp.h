#ifndef __SMP_H__
#define __SMP_H__

int hal_smp_init (int core_id);
int hal_smp_deinit (void);
int hal_smp_hsem_alloc (const char *name);
int hal_smp_hsem_destroy (int s);
int hal_smp_hsem_lock (int s);
int hal_smp_hsem_spinlock (int s);
int hal_smp_hsem_release (int s);

#define HAL_SMP_TASK_PEND (0x1)
#define HAL_SMP_TASK_EXEC (0x2)

typedef struct hal_smp_task_s {
    struct hal_smp_task_s *next;
    void *arg;
    void (*func) (void *arg); 
    size_t arg_size;
    uint32_t flags;
} hal_smp_task_t;

hal_smp_task_t *hal_smp_sched_task (void (*func) (void *), void *usr, size_t usr_size);
hal_smp_task_t *hal_smp_next_task (void);
void hal_smp_sync_task (hal_smp_task_t *task);
void hal_smp_free_task (hal_smp_task_t *task);
void hal_smp_release_task (hal_smp_task_t *task);


#endif /* __SMP_H__ */


