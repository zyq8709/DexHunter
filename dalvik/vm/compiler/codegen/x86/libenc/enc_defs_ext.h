/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ENCODER_DEFS_EXT_H_
#define _ENCODER_DEFS_EXT_H_


// Used to isolate experimental or being tuned encoder into a separate
// namespace so it can coexist with a stable one in the same bundle.
#ifdef ENCODER_ISOLATE
    #define ENCODER_NAMESPACE_START namespace enc_ia32 {
    #define ENCODER_NAMESPACE_END };
#else
    #define ENCODER_NAMESPACE_START
    #define ENCODER_NAMESPACE_END
#endif

ENCODER_NAMESPACE_START
typedef enum OpndSize {
    /**
     * A change must be balanced with at least the following places:
     *              Ia32IRConstants.h :: getByteSize() uses some presumptions about OpndSize_ values
     *              Ia32::Constraint-s use the OpndSize as a mask
     *              encoder.cpp & encoder_master_info.cpp uses OpndSize as an index for hashing
     *              - perhaps there are much more places
     */
    OpndSize_Null           = 0,
    OpndSize_8             = 0x01,
    OpndSize_16            = 0x02,
    OpndSize_32            = 0x04,
    OpndSize_64            = 0x08,
#if !defined(TESTING_ENCODER)
    OpndSize_80            = 0x10,
    OpndSize_128           = 0x20,
#endif
    OpndSize_Max,
    OpndSize_Any            = 0x3F,
    OpndSize_Default        = OpndSize_Any
} OpndSize;

/**
 * Conditional mnemonics.
 * The values match the 'real' (==processor's) values of the appropriate
 * condition values used in the opcodes.
 */
typedef enum ConditionMnemonic {

    ConditionMnemonic_O=0,
    ConditionMnemonic_NO=1,
    ConditionMnemonic_B=2, ConditionMnemonic_NAE=ConditionMnemonic_B, ConditionMnemonic_C=ConditionMnemonic_B,
    ConditionMnemonic_NB=3, ConditionMnemonic_AE=ConditionMnemonic_NB, ConditionMnemonic_NC=ConditionMnemonic_NB,
    ConditionMnemonic_Z=4, ConditionMnemonic_E=ConditionMnemonic_Z,
    ConditionMnemonic_NZ=5, ConditionMnemonic_NE=ConditionMnemonic_NZ,
    ConditionMnemonic_BE=6, ConditionMnemonic_NA=ConditionMnemonic_BE,
    ConditionMnemonic_NBE=7, ConditionMnemonic_A=ConditionMnemonic_NBE,

    ConditionMnemonic_S=8,
    ConditionMnemonic_NS=9,
    ConditionMnemonic_P=10, ConditionMnemonic_PE=ConditionMnemonic_P,
    ConditionMnemonic_NP=11, ConditionMnemonic_PO=ConditionMnemonic_NP,
    ConditionMnemonic_L=12, ConditionMnemonic_NGE=ConditionMnemonic_L,
    ConditionMnemonic_NL=13, ConditionMnemonic_GE=ConditionMnemonic_NL,
    ConditionMnemonic_LE=14, ConditionMnemonic_NG=ConditionMnemonic_LE,
    ConditionMnemonic_NLE=15, ConditionMnemonic_G=ConditionMnemonic_NLE,
    ConditionMnemonic_Count=16
} ConditionMnemonic;


#define CCM(prefix,cond) Mnemonic_##prefix##cond=Mnemonic_##prefix##cc+ConditionMnemonic_##cond

//=========================================================================================================
typedef enum Mnemonic {

Mnemonic_NULL=0, Mnemonic_Null=Mnemonic_NULL,
Mnemonic_ADC,                           // Add with Carry
Mnemonic_ADD,                           // Add
Mnemonic_ADDSD,                         // Add Scalar Double-Precision Floating-Point Values
Mnemonic_ADDSS,                         // Add Scalar Single-Precision Floating-Point Values
Mnemonic_AND,                           // Logical AND

Mnemonic_BSF,                           // Bit scan forward
Mnemonic_BSR,                           // Bit scan reverse

Mnemonic_CALL,                          // Call Procedure
Mnemonic_CMC,                           // Complement Carry Flag
Mnemonic_CWD, Mnemonic_CDQ=Mnemonic_CWD,// Convert Word to Doubleword/Convert Doubleword to Qua T dword
Mnemonic_CMOVcc,                        // Conditional Move
    CCM(CMOV,O),
    CCM(CMOV,NO),
    CCM(CMOV,B), CCM(CMOV,NAE), CCM(CMOV,C),
    CCM(CMOV,NB), CCM(CMOV,AE), CCM(CMOV,NC),
    CCM(CMOV,Z), CCM(CMOV,E),
    CCM(CMOV,NZ), CCM(CMOV,NE),
    CCM(CMOV,BE), CCM(CMOV,NA),
    CCM(CMOV,NBE), CCM(CMOV,A),

    CCM(CMOV,S),
    CCM(CMOV,NS),
    CCM(CMOV,P), CCM(CMOV,PE),
    CCM(CMOV,NP), CCM(CMOV,PO),
    CCM(CMOV,L), CCM(CMOV,NGE),
    CCM(CMOV,NL), CCM(CMOV,GE),
    CCM(CMOV,LE), CCM(CMOV,NG),
    CCM(CMOV,NLE), CCM(CMOV,G),

Mnemonic_CMP,                           // Compare Two Operands
Mnemonic_CMPXCHG,                       // Compare and exchange
Mnemonic_CMPXCHG8B,                     // Compare and Exchange 8 Bytes
Mnemonic_CMPSB,                         // Compare Two Bytes at DS:ESI and ES:EDI
Mnemonic_CMPSW,                         // Compare Two Words at DS:ESI and ES:EDI
Mnemonic_CMPSD,                         // Compare Two Doublewords at DS:ESI and ES:EDI
//
// double -> float
Mnemonic_CVTSD2SS,                      // Convert Scalar Double-Precision Floating-Point Value to Scalar Single-Precision Floating-Point Value
// double -> I_32
Mnemonic_CVTSD2SI,                      // Convert Scalar Double-Precision Floating-Point Value to Doubleword Integer
// double [truncated] -> I_32
Mnemonic_CVTTSD2SI,                     // Convert with Truncation Scalar Double-Precision Floating-Point Value to Signed Doubleword Integer
//
// float -> double
Mnemonic_CVTSS2SD,                      // Convert Scalar Single-Precision Floating-Point Value to Scalar Double-Precision Floating-Point Value
// float -> I_32
Mnemonic_CVTSS2SI,                      // Convert Scalar Single-Precision Floating-Point Value to Doubleword Integer
// float [truncated] -> I_32
Mnemonic_CVTTSS2SI,                     // Convert with Truncation Scalar Single-Precision Floating-Point Value to Doubleword Integer
//
// I_32 -> double
Mnemonic_CVTSI2SD,                      // Convert Doubleword Integer to Scalar Double-Precision Floating-Point Value
// I_32 -> float
Mnemonic_CVTSI2SS,                      // Convert Doubleword Integer to Scalar Single-Precision Floating-Point Value

Mnemonic_COMISD,                        // Compare Scalar Ordered Double-Precision Floating-Point Values and Set EFLAGS
Mnemonic_COMISS,                        // Compare Scalar Ordered Single-Precision Floating-Point Values and Set EFLAGS
Mnemonic_DEC,                           // Decrement by 1
//Mnemonic_DIV,                         // Unsigned Divide
Mnemonic_DIVSD,                         // Divide Scalar Double-Precision Floating-Point Values
Mnemonic_DIVSS,                         // Divide Scalar Single-Precision Floating-Point Values

#ifdef _HAVE_MMX_
Mnemonic_EMMS,                          // Empty MMX Technology State
#endif

Mnemonic_ENTER,                         // ENTER-Make Stack Frame for Procedure Parameters
Mnemonic_FLDCW,                         // Load FPU control word
Mnemonic_FADDP,
Mnemonic_FLDZ,
Mnemonic_FADD,
Mnemonic_FSUBP,
Mnemonic_FSUB,
Mnemonic_FISUB,
Mnemonic_FMUL,
Mnemonic_FMULP,
Mnemonic_FDIVP,
Mnemonic_FDIV,
Mnemonic_FUCOM,
Mnemonic_FUCOMI,
Mnemonic_FUCOMP,
Mnemonic_FUCOMIP,
Mnemonic_FUCOMPP,
Mnemonic_FRNDINT,
Mnemonic_FNSTCW,                        // Store FPU control word
Mnemonic_FSTSW,                         // Store FPU status word
Mnemonic_FNSTSW,                         // Store FPU status word
//Mnemonic_FDECSTP,                     // Decrement Stack-Top Pointer
Mnemonic_FILD,                          // Load Integer
Mnemonic_FLD,                           // Load Floating Point Value
Mnemonic_FLDLG2,
Mnemonic_FLDLN2,
Mnemonic_FLD1,

Mnemonic_FCLEX,                         // Clear Exceptions
Mnemonic_FCHS,                          // Change sign of ST0
Mnemonic_FNCLEX,                        // Clear Exceptions

//Mnemonic_FINCSTP,                     // Increment Stack-Top Pointer
Mnemonic_FIST,                          // Store Integer
Mnemonic_FISTP,                         // Store Integer, pop FPU stack
Mnemonic_FISTTP,                        // Store Integer with Truncation
Mnemonic_FPREM,                         // Partial Remainder
Mnemonic_FPREM1,                        // Partial Remainder
Mnemonic_FST,                           // Store Floating Point Value
Mnemonic_FSTP,                          // Store Floating Point Value and pop the FP stack
Mnemonic_FSQRT,                         //Computes the square root of the source value in the stack and pop the FP stack
Mnemonic_FABS,                          //Computes the absolute value of the source value in the stack and pop the FP stack
Mnemonic_FSIN,                          //Computes the sine of the source value in the stack and pop the FP stack
Mnemonic_FCOS,                          //Computes the cosine of the source value in the stack and pop the FP stack
Mnemonic_FPTAN,                         //Computes the tangent of the source value in the stack and pop the FP stack
Mnemonic_FYL2X,
Mnemonic_FYL2XP1,
Mnemonic_F2XM1,
Mnemonic_FPATAN,
Mnemonic_FXCH,
Mnemonic_FSCALE,

Mnemonic_XCHG,
Mnemonic_DIV,                           // Unsigned Divide
Mnemonic_IDIV,                          // Signed Divide
Mnemonic_MUL,                           // Unsigned Multiply
Mnemonic_IMUL,                          // Signed Multiply
Mnemonic_INC,                           // Increment by 1
Mnemonic_INT3,                          // Call break point
Mnemonic_Jcc,                           // Jump if Condition Is Met
    CCM(J,O),
    CCM(J,NO),
    CCM(J,B), CCM(J,NAE), CCM(J,C),
    CCM(J,NB), CCM(J,AE), CCM(J,NC),
    CCM(J,Z), CCM(J,E),
    CCM(J,NZ), CCM(J,NE),
    CCM(J,BE), CCM(J,NA),
    CCM(J,NBE), CCM(J,A),
    CCM(J,S),
    CCM(J,NS),
    CCM(J,P), CCM(J,PE),
    CCM(J,NP), CCM(J,PO),
    CCM(J,L), CCM(J,NGE),
    CCM(J,NL), CCM(J,GE),
    CCM(J,LE), CCM(J,NG),
    CCM(J,NLE), CCM(J,G),
Mnemonic_JMP,                           // Jump
Mnemonic_LEA,                           // Load Effective Address
Mnemonic_LEAVE,                         // High Level Procedure Exit
Mnemonic_LOOP,                          // Loop according to ECX counter
Mnemonic_LOOPE,                          // Loop according to ECX counter
Mnemonic_LOOPNE, Mnemonic_LOOPNZ = Mnemonic_LOOPNE, // Loop according to ECX
Mnemonic_LAHF,                          // Load Flags into AH
Mnemonic_MOV,                           // Move
Mnemonic_MOVD,                          // Move Double word
Mnemonic_MOVQ,                          // Move Quadword
/*Mnemonic_MOVS,                        // Move Data from String to String*/
// MOVS is a special case: see encoding table for more details,
Mnemonic_MOVS8, Mnemonic_MOVS16, Mnemonic_MOVS32, Mnemonic_MOVS64,
//
Mnemonic_MOVAPD,                         // Move Scalar Double-Precision Floating-Point Value
Mnemonic_MOVSD,                         // Move Scalar Double-Precision Floating-Point Value
Mnemonic_MOVSS,                         // Move Scalar Single-Precision Floating-Point Values
Mnemonic_MOVSX,                         // Move with Sign-Extension
Mnemonic_MOVZX,                         // Move with Zero-Extend
//Mnemonic_MUL,                         // Unsigned Multiply
Mnemonic_MULSD,                         // Multiply Scalar Double-Precision Floating-Point Values
Mnemonic_MULSS,                         // Multiply Scalar Single-Precision Floating-Point Values
Mnemonic_NEG,                           // Two's Complement Negation
Mnemonic_NOP,                           // No Operation
Mnemonic_NOT,                           // One's Complement Negation
Mnemonic_OR,                            // Logical Inclusive OR
Mnemonic_PREFETCH,                      // prefetch

#if 1 //def _HAVE_MMX_
    Mnemonic_PADDQ,                     // Add Packed Quadword Integers
    Mnemonic_PAND,                      // Logical AND
    Mnemonic_POR,                       // Bitwise Logical OR
    Mnemonic_PSUBQ,                     // Subtract Packed Quadword Integers
#endif
Mnemonic_PANDN,
Mnemonic_PSLLQ,
Mnemonic_PSRLQ,
Mnemonic_PXOR,                          // Logical Exclusive OR
Mnemonic_POP,                           // Pop a Value from the Stack
Mnemonic_POPFD,                         // Pop a Value of EFLAGS register from the Stack
Mnemonic_PUSH,                          // Push Word or Doubleword Onto the Stack
Mnemonic_PUSHFD,                        // Push EFLAGS Doubleword Onto the Stack
Mnemonic_RET,                           // Return from Procedure

Mnemonic_SETcc,                         // Set Byte on Condition
    CCM(SET,O),
    CCM(SET,NO),
    CCM(SET,B), CCM(SET,NAE), CCM(SET,C),
    CCM(SET,NB), CCM(SET,AE), CCM(SET,NC),
    CCM(SET,Z), CCM(SET,E),
    CCM(SET,NZ), CCM(SET,NE),
    CCM(SET,BE), CCM(SET,NA),
    CCM(SET,NBE), CCM(SET,A),
    CCM(SET,S),
    CCM(SET,NS),
    CCM(SET,P), CCM(SET,PE),
    CCM(SET,NP), CCM(SET,PO),
    CCM(SET,L), CCM(SET,NGE),
    CCM(SET,NL), CCM(SET,GE),
    CCM(SET,LE), CCM(SET,NG),
    CCM(SET,NLE), CCM(SET,G),

Mnemonic_SAL, Mnemonic_SHL=Mnemonic_SAL,// Shift left
Mnemonic_SAR,                           // Unsigned shift right
Mnemonic_ROR,                           // Rotate right
Mnemonic_RCR,                           // Rotate right through CARRY flag
Mnemonic_ROL,                           // Rotate left
Mnemonic_RCL,                           // Rotate left through CARRY flag
Mnemonic_SHR,                           // Signed shift right
Mnemonic_SHRD,                          // Double Precision Shift Right
Mnemonic_SHLD,                          // Double Precision Shift Left

Mnemonic_SBB,                           // Integer Subtraction with Borrow
Mnemonic_SUB,                           // Subtract
Mnemonic_SUBSD,                         // Subtract Scalar Double-Precision Floating-Point Values
Mnemonic_SUBSS,                         // Subtract Scalar Single-Precision Floating-Point Values

Mnemonic_TEST,                          // Logical Compare

Mnemonic_UCOMISD,                       // Unordered Compare Scalar Double-Precision Floating-Point Values and Set EFLAGS
Mnemonic_UCOMISS,                       // Unordered Compare Scalar Single-Precision Floating-Point Values and Set EFLAGS

Mnemonic_XOR,                           // Logical Exclusive OR
//
// packed things,
//
Mnemonic_XORPD,                         // Bitwise Logical XOR for Double-Precision Floating-Point Values
Mnemonic_XORPS,                         // Bitwise Logical XOR for Single-Precision Floating-Point Values

Mnemonic_CVTDQ2PD,                      // Convert Packed Doubleword Integers to Packed Double-Precision Floating-Point Values
Mnemonic_CVTTPD2DQ,                     // Convert with Truncation Packed Double-Precision Floating-Point Values to Packed Doubleword Integers

Mnemonic_CVTDQ2PS,                      // Convert Packed Doubleword Integers to Packed Single-Precision Floating-Point Values
Mnemonic_CVTTPS2DQ,                     // Convert with Truncation Packed Single-Precision Floating-Point Values to Packed Doubleword Integers
//
// String operations
//
Mnemonic_STD,                           // Set direction flag
Mnemonic_CLD,                           // Clear direction flag
Mnemonic_SCAS,                          // Scan string
Mnemonic_STOS,                          // Store string

//
Mnemonic_WAIT,                          // Check pending pending unmasked floating-point exception
//
Mnemonic_Count
} Mnemonic;

#undef CCM

ENCODER_NAMESPACE_END

#endif  // ifndef _ENCODER_DEFS_EXT_H_
