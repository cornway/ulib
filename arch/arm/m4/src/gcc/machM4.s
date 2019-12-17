                    .syntax unified
                    .section .text

                    .global SystemSoftReset

SystemSoftReset:
                    DSB
                    LDR R0, =0xE000ED0C
                    LDR R1, =0x05FA0004
                    STR R1, [R0]
                    DSB
                    B .

                    .global __arch_get_stack

__arch_get_stack:
                    PUSH {R2}
                    LDR R2, =Stack_Mem
                    STR R2, [R0]
                    LDR R2, =Stack_Size
                    STR R2, [R1]
                    POP {R2}
                    BX  LR

                    .global __arch_get_heap

__arch_get_heap:
                    PUSH {R2}
                    LDR R2, =Heap_Mem
                    STR R2, [R0]
                    LDR R2, =Heap_Size
                    STR R2, [R1]
                    POP {R2}
                    BX  LR


                    .global __arch_get_shared

__arch_get_shared:
                    PUSH {R2}
                    LDR R2, =Shared_Mem
                    STR R2, [R0]
                    LDR R2, =Shared_Size
                    STR R2, [R1]
                    POP {R2}
                    BX  LR

                    .global __arch_asmgoto
                        
__arch_asmgoto:
                    BX R0
                    B   .
