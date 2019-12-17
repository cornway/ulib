
#include <arch.h>

#define CPU_PRIV_ACCESS 0
#define CPU_UNPRIV_ACCESS 1

#define CPU_USE_MSP 0
#define CPU_USE_PSP 2

#define HANDLER_NOFPU_MSP   (0xF1U)
#define THREAD_NOFPU_MSP    (0xF9U)
#define THREAD_NOFPU_PSP    (0xFDU)
#define HANDLER_FPU_MSP     (0xE1U)
#define THREAD_FPU_MSP      (0xE9U)
#define THREAD_FPU_PSP      (0xEDU)

#define EXC_RETURN(exc)     (0xFFFFFF00U | exc)

#define EXC_RETURN_USE_FPU_BM   (0x10U)
#define EXC_RETURN_HANDLER_GM   (0xfU)
#define EXC_RETURN_HANDLER_VAL  (0x1)

#define FPU_STACK_SIZE      (33 * sizeof(arch_word_t))
#define CPU_STACK_SIZE      (17 * sizeof(arch_word_t))
    
#define STACK_ALLIGN        (8U)
    
#define CPU_XPSR_T_BM       (0x01000000U)

#define CPU_ACCESS_LEVEL_0 (CPU_USE_PSP | CPU_UNPRIV_ACCESS)
#define CPU_ACCESS_LEVEL_1 (CPU_USE_PSP | CPU_PRIV_ACCESS)
#define CPU_ACCESS_LEVEL_2 (CPU_USE_MSP | CPU_UNPRIV_ACCESS)
#define CPU_ACCESS_LEVEL_3 (CPU_USE_MSP | CPU_PRIV_ACCESS)


typedef INT32_T (*v_callback_t) (arch_word_t, void *);

typedef struct {
    arch_word_t EXC_RET;
    arch_word_t R11; /*user top*/
    arch_word_t R10;
    arch_word_t R9;
    arch_word_t R8;
    arch_word_t R7;
    arch_word_t R6;
    arch_word_t R5;
    arch_word_t R4; /*irq top*/
    arch_word_t R0; 
    arch_word_t R1;
    arch_word_t R2;
    arch_word_t R3;
    arch_word_t R12;
    arch_word_t LR;
    arch_word_t PC;
    arch_word_t XPSR; /*pre irq top*/
} cpu_frame_t; /*stack frame implementation for no fpu context store*/

typedef struct {
    arch_word_t S16[16];
    cpu_frame_t cpu;

    arch_word_t S[16];
    arch_word_t FPSCR;
} cpu_fpu_frame_t; /*stack frame implementation for lazy fpu context store*/


typedef struct {
    arch_word_t LR;
    arch_word_t Ctrl;
    union {
        cpu_frame_t cpu_nofpu;
        cpu_fpu_frame_t cpu;
    };
} cpu_frame_t;

typedef struct {
    cpu_frame_t *frame;
    void *pc;
    void *ret;
    void *stack;
    arch_word_t status;
    cpu_frame_t cpu;
} cpu_frame_desc_t;

void *__cpu_unwind (cpu_frame_desc_t *desc, void *pc)
{
    if (!pc) {
        pc = desc->pc;
    } else {
        pc = desc->ret;
    }
    return pc;
}

void *cpu_unwind (cpu_frame_desc_t *desc, void *pc)
{
    return __cpu_unwind(desc, pc);
}

void *cpu_unwind (void *frame, void *pc)
{

}

void *__cpu_isr (void *frame)
{
    const arch_word_t offset = sizeof(cpu_frame_desc_t) - sizeof(cpu_frame_t);
    cpu_frame_desc_t *desc = (cpu_frame_desc_t *)((uint8_t *)frame - offset);

    if (desc->cpu.LR & EXC_RETURN_USE_FPU_BM) {
        desc->frame = &desc->cpu.cpu.cpu;
        desc->stack = (void *)((uint8_t *)frame + sizeof(desc->cpu.cpu));
    } else {
        desc->frame = &desc->cpu.cpu_nofpu;
        desc->stack = (void *)((uint8_t *)frame + sizeof(desc->cpu.cpu_nofpu));
    }
    desc->pc = (void *)desc->frame->PC;
    desc->ret = (void *)desc->frame->LR;
    desc->status = desc->frame->XPSR;
}

void *__cpu_rise (void *frame)
{
    cpu_frame_desc_t *desc = __cpu_isr(frame);
    void *pc = cpu_unwind(desc, NULL);
    void *ret = cpu_unwind(desc, pc);

    dprintf("%s() :\n", __func__);
    dprintf("pc at : <0x%p>\n", pc);
    dprintf("ret at : <0x%p>\n", ret);
    dprintf("status : 0x%x\n", status);
}

void *cpu_rise (void *frame)
{
    return __cpu_rise(frame);
}

