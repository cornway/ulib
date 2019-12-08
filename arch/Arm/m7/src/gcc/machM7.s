                    .syntax unified
                    .section .text

                    .global __arch_soft_reset

__arch_soft_reset:
                    DSB
                    LDR R0, =0xE000ED0C
                    LDR R1, =0x05FA0004
                    STR R1, [R0]
                    DSB
                    B .

.macro              __get_memptr P0, P1
                    PUSH {R2}
                    LDR R2, =\P0
                    STR R2, [R0]
                    LDR R0, =\P1
                    SUB R2, R0, R2
                    STR R2, [R1]
                    POP {R2}
                    BX  LR
.endm

                    .global __arch_get_stack
__arch_get_stack:
                    __get_memptr __stack_mem, __stack_end

                    .global __arch_get_heap
__arch_get_heap:
                    __get_memptr __heap_mem, __heap_end

                    .global __arch_get_shared
__arch_get_shared:
                    __get_memptr __shared_mem, __shared_end

                    .global __arch_get_usr_heap
__arch_get_usr_heap:
                    __get_memptr __user_heap_mem, __user_heap_end

                    .global __arch_asmgoto                        
__arch_asmgoto:
                    BX R0
                    B   .

                    .global __arch_startup
__arch_startup:
                    PUSH {R0, R1, R2}
                    LDR R0, =_sbss
                    LDR R1, =_ebss
                    LDR R2, =#0
__loop:
                    CMP R0, R1
                    IT LT
                    STRLT R2, [R0], #4
                    BLT __loop
                    POP {R0, R1, R2}
                    BX LR

                    .global __arch_fpu_enable
__arch_fpu_enable:
                    PUSH {R0, R1}
                    LDR.W R0, =0xE000ED88
                    LDR R1, [R0]
                    ORR R1, R1, #(0xF << 20)
                    STR R1, [R0]
                    POP {R0, R1}
                    BX LR

.macro              __cpu_save P0
                    DMB
                    TST     LR, #0x04
                    ITE     EQ
                    MRSEQ   R0, MSP
                    MRSNE   R0, PSP
                    MOV     R2, LR
                    MRS     R1, CONTROL
                    STMDB   R0!, {R4 - R11, LR}
                    TST     LR, #0x10 @check fpu
                    IT      EQ
                    VSTMDBEQ  R0!, {S16 - S31}
                    BL      \P0
                    TST     R2, #0x10
                    IT      EQ
                    VLDMIAEQ R0!, {S16 - S31}
                    LDMIA   R0!, {R4 - R11, LR}
                    TST     R1, #0x02
                    ITE     EQ
                    MSREQ   MSP, R0
                    MSRNE   PSP, R0
                    DMB
                    MSR     CONTROL, R1
                    BX      R2
.endm

                    .global HardFault_Handler
HardFault_Handler:  BL __c_hard_fault

                    .end