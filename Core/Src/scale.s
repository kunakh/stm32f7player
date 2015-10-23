/*
 * @file    scale.s
 * @author  Peter Kunakh
 * @version 1.0
 * @date    1-October-2015
 * @brief   This file scaling YCbCr42x to RGB565
 */

        RSEG    CODE:CODE(2)
        THUMB
        PUBLIC  ScaleYUV2RGB565

ScaleYUV2RGB565:
        PUSH.W  {R4-R12, LR}
        SUB     SP, SP, #0x2c
        STR     R2, [SP, #0x24]
        LDR     R2, [SP, #0x54]
        STR     R0, [SP, #0x1c]
        LSLS    R2, R2, #16
        LDR     R0, [SP, #0x5c]
        STR     R1, [SP, #0x20]
        LDR     R1, [SP, #0x58]
        STR     R3, [SP, #0x8]
        MOVS    R3, #0
        SDIV    R2, R2, R0
        MOV     R5, R2
        ASRS    R2, R2, #1
        LDR     R0, [SP, #0x60]
        STR     R2, [SP, #0x18]
        LSLS    R2, R1, #16
        STR     R3, [SP, #0x4]
        SDIV    R2, R2, R0
        STR     R2, [SP, #0xc]
        ASRS    R2, R2, #1
        STR     R2, [SP]
        LDRB.W  R2, [SP, #0x70]
        CLZ     R0, R2
        SUBS    R2, R1, #1
        LSRS    R0, R0, #5
        STR     R2, [SP, #0x10]
        ADD     R1, R1, R0
        ASRS    R1, R1, R0
        SUBS    R2, R1, #1
        STR     R2, [SP, #0x14]
L48:
        LDR     R3, [SP, #0x60]
        LDR     R2, [SP, #0x4]
        CMP     R2, R3
        BGE.W   L16c
        LDR     R3, [SP]
        LDR     R1, [SP, #0x1c]
        ASRS    R2, R3, #16
        LDR     R3, [SP, #0x10]
        LDR     R4, [SP]
        CMP     R2, R3
        LDR     R3, [SP, #0x64]
        LDR     R7, [SP, #0x24]
        ITE     GT
        LDRGT   R2, [SP, #0x10]
        BICLE.W R2, R2, R2, ASR #31
        LDR.W   LR, [SP, #0x18]
        MLA     R2, R3, R2, R1
        LDR     R1, [SP]
        ADD.W   R3, R0, #16
        ASR.W   R3, R1, R3
        LDR     R1, [SP, #0x14]
        CMP     R3, R1
        LDR     R1, [SP, #0xc]
        IT      LE
        BICLE.W R3, R3, R3, ASR #31
        ADD     R4, R4, R1
        IT      GT
        LDRGT   R3, [SP, #0x14]
        LDR     R1, [SP, #0x68]
        STR     R4, [SP]
        MULS    R3, R1, R3
        LDR     R1, [SP, #0x20]
        LDR     R4, [SP, #0x8]
        ADD     R1, R1, R3
        ADD     R3, R3, R7
        LDR     R7, [SP, #0x5c]
        MOV     R6, R7
        MOV.W   R11, #74
        MVN.W   R12, #24
ScaleYUV2RGB565Row:
        ADD.W   R7, R2, LR, LSR #16
        LDRB.W  R8, [R7]
        LSR.W   R7, LR, #17
        LDRB.W  R9, [R1, R7]
        LDRB.W  R10, [R3, R7]
        ADD     LR, LR, R5
        LDR     R7, =0xffffbae0
        MOVT    R11, #129
        PKHBT   R9, R8, R9, LSL #16
        PKHBT   R8, R8, R10, LSL #16
        SMLAD   R7, R11, R9, R7
        USAT    R10, #5, R7, ASR #9
        LDR     R7, =0xffffc860
        MOVT    R11, #102
        SMLAD   R7, R11, R8, R7
        USAT    R7, #5, R7, ASR #9
        ORR.W   R10, R10, R7, LSL #11
        LDR     R7, =0x2200
        MOVT    R11, #65484
        SMLAD   R7, R11, R8, R7
        SMLABT  R7, R12, R9, R7
        USAT    R7, #6, R7, ASR #8
        ORR.W   R10, R10, R7, LSL #5
        STRH.W  R10, [R4], #0x2
        ADD.W   R7, R2, LR, LSR #16
        LDRB.W  R8, [R7]
        LSR.W   R7, LR, #17
        LDRB.W  R9, [R1, R7]
        LDRB.W  R10, [R3, R7]
        ADD     LR, LR, R5
        LDR     R7, =0xffffbc60
        MOVT    R11, #129
        PKHBT   R9, R8, R9, LSL #16
        PKHBT   R8, R8, R10, LSL #16
        SMLAD   R7, R11, R9, R7
        USAT    R10, #5, R7, ASR #9
        LDR     R7, =0xffffc9e0
        MOVT    R11, #102
        SMLAD   R7, R11, R8, R7
        USAT    R7, #5, R7, ASR #9
        ORR.W   R10, R10, R7, LSL #11
        LDR     R7, =0x22c0
        MOVT    R11, #65484
        SMLAD   R7, R11, R8, R7
        SMLABT  R7, R12, R9, R7
        USAT    R7, #6, R7, ASR #8
        ORR.W   R10, R10, R7, LSL #5
        STRH.W  R10, [R4], #0x2
        SUBS    R6, #2
        BNE.N   ScaleYUV2RGB565Row
        LDR     R3, [SP, #0x4]
        LDR     R2, [SP, #0x8]
        ADDS    R3, #1
        STR     R3, [SP, #0x4]
        LDR     R3, [SP, #0x6c]
        ADD     R2, R2, R3
        STR     R2, [SP, #0x8]
        B.N     L48
L16c:
        ADD     SP, SP, #0x2c
        POP.W   {R4-R12, PC}
        END
