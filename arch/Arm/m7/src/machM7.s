                    EXPORT EnableFPU
                    EXPORT SystemSoftReset
                    EXPORT upcall                  [WEAK]
;                    EXPORT VMBOOT                  [WEAK]
                    EXPORT __arch_get_stack
                    EXPORT __arch_get_heap
                    EXPORT __arch_asmgoto
                    EXPORT __arch_get_shared

                    IMPORT Stack_Mem
                    IMPORT Stack_Size
                    IMPORT Heap_Mem
                    IMPORT Heap_Size
                    IMPORT Shared_Mem
                    IMPORT Shared_Size
                
                    MACRO 
$label              WRAP $DEST
                
                    CPSID   I
                    DMB
                    TST     LR, #0x04
                    BNE     $label.STORE_PSP
                    MRS     R0, MSP 
                    B       $label.BRS
$label.STORE_PSP    MRS     R0, PSP                    
$label.BRS          MOV     R2, LR
                    MRS     R1, CONTROL
                    STMDB   R0!, {R4 - R11, LR}
                    TST     LR, #0x10 ;check fpu
                    BNE     $label.FPU_SKIP
                    VSTMDB  R0!, {S16 - S31}
$label.FPU_SKIP     BL      $DEST
                    TST     R2, #0x10
                    BNE     $label.FPU_SKIP_
                    VLDMIA  R0!, {S16 - S31}
$label.FPU_SKIP_    LDMIA   R0!, {R4 - R11, LR} 
                    TST     R1, #0x02
                    BNE     $label.LOAD_PSP
                    MSR     MSP, R0
                    B       $label.BRL
$label.LOAD_PSP     MSR     PSP, R0 
                    DMB
$label.BRL          CPSIE   I
                    MSR     CONTROL, R1
                    BX      R2
                
                    MEND

                    PRESERVE8
                    THUMB

                    AREA    |.text|, CODE, READONLY

                    ALIGN
upcall              PROC
                    SWI     0x02
                    BX      LR
                    ENDP
                    ALIGN
                        
;VMBOOT              PROC  
;                    CPSID   I
;                    DSB
;                    BL      EnableFPU
;                    BL      VMStart  
;                    DMB
;                    MSR     PSP, R0
;                    CPSIE   I
;                    MSR     CONTROL, R1
;                    POP     {PC}
;                    ENDP
                        
;SysTick_Handler     PROC 
;SYS_TICK_           WRAP VMTick                
;                    ENDP  
                        
;PendSV_Handler      PROC  

;                    ENDP  
                        
;SVC_Handler         PROC   
;SVC_HANDLE_         WRAP VMSvc
;                    ENDP 
                    
;HardFault_Handler   PROC  
;HARD_FAULT_         WRAP VMHardFault   
;                    ENDP 
TableEnd        
                
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
                    
SystemSoftReset     PROC
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
                        
__arch_get_shared   PROC
                    PUSH {R2}
                    LDR R2, =Shared_Mem
                    STR R2, [R0]
                    LDR R2, =Shared_Size
                    STR R2, [R1]
                    POP {R2}
                    BX  LR
                    ENDP
                        
__arch_asmgoto      PROC
                    BX R0
                    B   .
                    ENDP
                    END
                       
                
                
                
                
                
                
                
                
                
                
                
                
                
                