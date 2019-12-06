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

                    .end
