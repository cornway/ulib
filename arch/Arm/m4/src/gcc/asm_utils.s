                    .syntax unified
                    .section .text

                    .global asmread32

asmread32:
                    PUSH {R1, R2}
                    AND  R1, R0, #0x3
                    EOR  R0, R1
                    LDR  R2, [R0]
                    TEQ  R1, #0
                    IT   EQ
                    MOVEQ R0, R2
                    BEQ  return32

                    TEQ  R1, #0x2
                    IT   EQ
                    BEQ  swap32

                    PUSH {R3}
                    LDR  R3, [R0, #0x4]
                    LSL  R0, R1, #0x3
                    MOV  R2, R2, LSR R0
                    EOR  R0, R0, #(0x3 << 0x3)
                    ADD  R0, #0x8
                    MOV  R3, R3, LSL R0
                    ORR  R0, R2, R3
                    POP  {R3}
return32:
                    POP  {R1, R2}
                    BX   LR
swap32:
                    LDR  R1          , [R0, #0x4]
                    MOV  R2, R2, LSR #0x10
                    MOV  R1, R1, LSL #0x10
                    ORR  R0, R2, R1
                    B    return32

                    .global asmread16

asmread16:
                    PUSH {R1, R2}
                    AND  R1, R0, #0x3
                    EOR  R0, R1
                    LDR  R2, [R0]
                    TEQ  R1, #0x3
                    BEQ worstcase16
                    LSL  R1, #0x3
                    MOV R0, R2, LSR R1
return16:
                    LDR  R1, =#0xffff
                    AND  R0, R1
                    POP  {R1, R2}
                    BX   LR
worstcase16:
                    LDR  R1, [R0, #0x4]
                    MOV  R2, R2, LSR #0x18
                    MOV  R1, R1, LSL #0x8
                    ORR  R0, R2, R1
                    B return16
