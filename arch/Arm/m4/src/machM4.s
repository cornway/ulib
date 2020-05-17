                    EXPORT EnableFPU
                    EXPORT __arch_soft_reset
                    EXPORT __arch_rise              [WEAK]
                    EXPORT __arch_get_stack
                    EXPORT __arch_get_heap
                    EXPORT __arch_asmgoto
                    EXPORT __arch_get_shared
					EXPORT __arch_get_usr_heap
                    EXPORT __arch_rise

                    EXPORT SVC_Handler

                    IMPORT Stack_Mem
                    IMPORT Stack_Size
                    IMPORT Heap_Mem
                    IMPORT Heap_Size
                    IMPORT UserExceptionH

                    PRESERVE8
                    THUMB

                    AREA    |.text|, CODE, READONLY

                    ALIGN
__arch_rise         PROC
                    SWI     0x02
                    BX      LR
                    ENDP
                    ALIGN

EnableFPU           PROC
                    PUSH {R0, R1}
                    ; CPACR is located at address 0xE000ED88
                    LDR R0, =0xE000ED88
                    ; Read CPACR
                    LDR R1, [R0]
                    ; Set bits 20-23 to enable CP10 and CP11 coprocessors
                    ORR R1, R1, #(0xF << 20)
                    ; Write back the modified value to the CPACR
                    STR R1, [R0]; wait for store to complete
                    DSB
                    ;reset pipeline now the FPU is enabled
                    ISB
                    
                    LDR R0, =0xE000EF34 ;FPCCR
                    LDR R1, [R0]
                    ORR R1, R1, #(0x3 << 30) ;set bits 30-31 to enable lazy staking and automatic status store
                    STR R1, [R0]
                    DSB
                    POP {R1, R0}
                    BX  LR
                    ENDP
                    ALIGN

SVC_Handler         PROC   
                    B __arch_exception
                    ENDP

__arch_exception    PROC
                    DMB
                    MRS     R0, MSP
                    BL      UserExceptionH
                    BX      LR
                    ENDP

__arch_soft_reset   PROC
                    DSB
                    LDR R0, =0xE000ED0C ;AIRCR
                    LDR R1, =0x05FA0004
                    STR R1, [R0]
                    DSB
                    B .
                    ENDP

__arch_get_stack    PROC
                    PUSH {R2}
                    LDR R2, =Stack_Mem
                    STR R2, [R0]
                    LDR R2, =Stack_Size
                    STR R2, [R1]
                    POP {R2}
                    BX  LR
                    ENDP

__arch_get_heap     PROC
                    PUSH {R2}
                    LDR R2, =Heap_Mem
                    STR R2, [R0]
                    LDR R2, =Heap_Size
                    STR R2, [R1]
                    POP {R2}
                    BX  LR
                    ENDP

__arch_get_usr_heap PROC
                    PUSH {R2}
                    AND R2, #0
                    STR R2, [R0]
                    AND R2, #0
                    STR R2, [R1]
                    POP {R2}
                    BX  LR
                    ENDP

__arch_get_shared   PROC
                    PUSH {R2}
                    AND R2, #0
                    STR R2, [R0]
                    AND R2, #0
                    STR R2, [R1]
                    POP {R2}
                    BX  LR
                    ENDP

__arch_asmgoto      PROC
                    BX R0
                    B   .
                    ENDP
                    END