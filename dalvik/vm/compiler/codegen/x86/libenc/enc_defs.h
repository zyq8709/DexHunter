/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/**
 * @author Alexander V. Astapchuk
 */
#ifndef _ENCODER_DEFS_H_
#define _ENCODER_DEFS_H_


// Used to isolate experimental or being tuned encoder into a separate
// namespace so it can coexist with a stable one in the same bundle.
#ifdef ENCODER_ISOLATE
    #define ENCODER_NAMESPACE_START namespace enc_ia32 {
    #define ENCODER_NAMESPACE_END };
#else
    #define ENCODER_NAMESPACE_START
    #define ENCODER_NAMESPACE_END
#endif

#include <assert.h>
#include "enc_defs_ext.h"

#ifndef COUNTOF
    /**
     * Number of items in an array.
     */
    #define COUNTOF(a)      (sizeof(a)/sizeof(a[0]))
#endif

#ifdef _EM64T_
    /**
     * A stack pointer of default platform's size.
     */
    #define REG_STACK       RegName_RSP
    /**
     * A max GP register (with a highest index number)
     */
    #define REG_MAX         RegName_R15
    /**
     * Total number of GP registers including stack pointer.
     */
    #define MAX_REGS        15
#else
    #define REG_STACK       RegName_ESP
    #define REG_MAX         RegName_EDI
    #define MAX_REGS        8
#endif

ENCODER_NAMESPACE_START

/**
 * A number of bytes 'eaten' by an ordinary PUSH/POP.
 */
#define STACK_SLOT_SIZE (sizeof(void*))


/**
 * A recommended by Intel Arch Manual aligment for instructions that
 * are targets for jmps.
 */
#define JMP_TARGET_ALIGMENT     (16)
/**
 * A maximum possible size of native instruction.
 */
#define MAX_NATIVE_INST_SIZE (15)
/**
 * The enum OpndKind describes an operand's location - memory, immediate or a register.
 * It can be used as a bit mask.
 */
typedef enum OpndKind {
    /**
     * A change must be balanced with at least the following places:
     *              Ia32::Constraint-s use the OpndKind as a mask
     *              encoder.cpp & encoder_master_info.cpp uses OpndKind as an index for hashing
     *              - perhaps there are much more places
     *
     * NOTE: an MMXReg kind is incompatible with the current constraints framework,
     *              as it's not encoded as a mask.
     */
    OpndKind_Null=0,
    OpndKind_GPReg          = 0x01, OpndKind_MinRegKind = OpndKind_GPReg,
    OpndKind_SReg           = 0x02,
#ifdef _HAVE_MMX_
    OpndKind_MMXReg         = 0x03,
#endif
    OpndKind_FPReg          = 0x04,
    OpndKind_XMMReg         = 0x08,
    OpndKind_OtherReg       = 0x10,
    OpndKind_StatusReg      = OpndKind_OtherReg,
    OpndKind_MaxRegKind     = OpndKind_StatusReg,   // a max existing kind of register
    OpndKind_MaxReg,                                // -'- + 1 to be used in array defs
    //
    OpndKind_Immediate      = 0x20, OpndKind_Imm=OpndKind_Immediate,
    OpndKind_Memory         = 0x40, OpndKind_Mem=OpndKind_Memory,
    //
    OpndKind_Reg            = 0x1F,
    OpndKind_Any            = 0x7F,
    // syntetic constants. Normally not used anywhere, but are used for
    // human-readable showing under the debugger
    OpndKind_GPReg_Mem      = OpndKind_GPReg|OpndKind_Mem,
#ifdef _HAVE_MMX_
    OpndKind_MMXReg_Mem     = OpndKind_MMXReg|OpndKind_Mem,
#endif
    OpndKind_XMMReg_Mem     = OpndKind_XMMReg|OpndKind_Mem,
} OpndKind;

/**
 * Defines type of extention allowed for particular operand.
 * For example imul r32,r_m32,imm8 sign extend imm8 before performing multiplication.
 * To satisfy instruction constraints immediate operand should be either OpndExt_Signed
 * or OpndExt_Any.
 */
typedef enum OpndExt {
    OpndExt_None    = 0x0,
    OpndExt_Signed  = 0x1,
    OpndExt_Zero    = 0x2,
    OpndExt_Any     = 0x3,
}OpndExt;

/**
 * enum OpndRole defines the role of an operand in an instruction
 * Can be used as mask to combine def and use. The complete def+use
 * info can be combined in 2 bits which is used, say in Encoder::OpndRole.
 */
//TODO: this duplicates an Role used in the Ia32::Inst. That duplicate enum should be removed.
typedef enum OpndRole {
    OpndRole_Null=0,
    OpndRole_Use=0x1,
    OpndRole_Def=0x2,
    OpndRole_UseDef=OpndRole_Use|OpndRole_Def,
    OpndRole_All=0xffff,
} OpndRole;


#define REGNAME(k,s,i) ( ((k & OpndKind_Any)<<24) | ((s & OpndSize_Any)<<16) | (i&0xFF) )

// Gregory -
// It is critical that all register indexes (3rd number) inside of the
// following table go in ascending order. That is R8 goes after
// RDI. It is necessary for decoder when extending registers from RAX-RDI
// to R8-R15 by simply adding 8 to the index on EM64T architecture
typedef enum RegName {

    RegName_Null = 0,

#ifdef _EM64T_
    /*
    An index part of the RegName-s for RAX-RDI, EAX-ESI, AX-SI and AL-BH is
    the same as the index used during instructions encoding. The same rule
    applies for XMM regsters for IA32.
    For new EM64T registers (both GP and XMM) the index need to be corrected to
    obtain the index used in processor's instructions.
    */
    RegName_RAX = REGNAME(OpndKind_GPReg,OpndSize_64,0),
    RegName_RCX = REGNAME(OpndKind_GPReg,OpndSize_64,1),
    RegName_RDX = REGNAME(OpndKind_GPReg,OpndSize_64,2),
    RegName_RBX = REGNAME(OpndKind_GPReg,OpndSize_64,3),
    RegName_RSP = REGNAME(OpndKind_GPReg,OpndSize_64,4),
    RegName_RBP = REGNAME(OpndKind_GPReg,OpndSize_64,5),
    RegName_RSI = REGNAME(OpndKind_GPReg,OpndSize_64,6),
    RegName_RDI = REGNAME(OpndKind_GPReg,OpndSize_64,7),

    RegName_R8  = REGNAME(OpndKind_GPReg,OpndSize_64,8),
    RegName_R9  = REGNAME(OpndKind_GPReg,OpndSize_64,9),
    RegName_R10 = REGNAME(OpndKind_GPReg,OpndSize_64,10),
    RegName_R11 = REGNAME(OpndKind_GPReg,OpndSize_64,11),
    RegName_R12 = REGNAME(OpndKind_GPReg,OpndSize_64,12),
    RegName_R13 = REGNAME(OpndKind_GPReg,OpndSize_64,13),
    RegName_R14 = REGNAME(OpndKind_GPReg,OpndSize_64,14),
    RegName_R15 = REGNAME(OpndKind_GPReg,OpndSize_64,15),
#endif //~_EM64T_

    RegName_EAX=REGNAME(OpndKind_GPReg,OpndSize_32,0),
    RegName_ECX=REGNAME(OpndKind_GPReg,OpndSize_32,1),
    RegName_EDX=REGNAME(OpndKind_GPReg,OpndSize_32,2),
    RegName_EBX=REGNAME(OpndKind_GPReg,OpndSize_32,3),
    RegName_ESP=REGNAME(OpndKind_GPReg,OpndSize_32,4),
    RegName_EBP=REGNAME(OpndKind_GPReg,OpndSize_32,5),
    RegName_ESI=REGNAME(OpndKind_GPReg,OpndSize_32,6),
    RegName_EDI=REGNAME(OpndKind_GPReg,OpndSize_32,7),

#ifdef _EM64T_
    RegName_R8D  = REGNAME(OpndKind_GPReg,OpndSize_32,8),
    RegName_R9D  = REGNAME(OpndKind_GPReg,OpndSize_32,9),
    RegName_R10D = REGNAME(OpndKind_GPReg,OpndSize_32,10),
    RegName_R11D = REGNAME(OpndKind_GPReg,OpndSize_32,11),
    RegName_R12D = REGNAME(OpndKind_GPReg,OpndSize_32,12),
    RegName_R13D = REGNAME(OpndKind_GPReg,OpndSize_32,13),
    RegName_R14D = REGNAME(OpndKind_GPReg,OpndSize_32,14),
    RegName_R15D = REGNAME(OpndKind_GPReg,OpndSize_32,15),
#endif //~_EM64T_

    RegName_AX=REGNAME(OpndKind_GPReg,OpndSize_16,0),
    RegName_CX=REGNAME(OpndKind_GPReg,OpndSize_16,1),
    RegName_DX=REGNAME(OpndKind_GPReg,OpndSize_16,2),
    RegName_BX=REGNAME(OpndKind_GPReg,OpndSize_16,3),
    RegName_SP=REGNAME(OpndKind_GPReg,OpndSize_16,4),
    RegName_BP=REGNAME(OpndKind_GPReg,OpndSize_16,5),
    RegName_SI=REGNAME(OpndKind_GPReg,OpndSize_16,6),
    RegName_DI=REGNAME(OpndKind_GPReg,OpndSize_16,7),

#ifdef _EM64T_
    RegName_R8S  = REGNAME(OpndKind_GPReg,OpndSize_16,8),
    RegName_R9S  = REGNAME(OpndKind_GPReg,OpndSize_16,9),
    RegName_R10S = REGNAME(OpndKind_GPReg,OpndSize_16,10),
    RegName_R11S = REGNAME(OpndKind_GPReg,OpndSize_16,11),
    RegName_R12S = REGNAME(OpndKind_GPReg,OpndSize_16,12),
    RegName_R13S = REGNAME(OpndKind_GPReg,OpndSize_16,13),
    RegName_R14S = REGNAME(OpndKind_GPReg,OpndSize_16,14),
    RegName_R15S = REGNAME(OpndKind_GPReg,OpndSize_16,15),
#endif //~_EM64T_

    RegName_AL=REGNAME(OpndKind_GPReg,OpndSize_8,0),
    RegName_CL=REGNAME(OpndKind_GPReg,OpndSize_8,1),
    RegName_DL=REGNAME(OpndKind_GPReg,OpndSize_8,2),
    RegName_BL=REGNAME(OpndKind_GPReg,OpndSize_8,3),
    // FIXME: Used in enc_tabl.cpp
    // AH is not accessible on EM64T, instead encoded register is SPL, so decoded
    // register will return incorrect enum
    RegName_AH=REGNAME(OpndKind_GPReg,OpndSize_8,4),
#if !defined(_EM64T_)
    RegName_CH=REGNAME(OpndKind_GPReg,OpndSize_8,5),
    RegName_DH=REGNAME(OpndKind_GPReg,OpndSize_8,6),
    RegName_BH=REGNAME(OpndKind_GPReg,OpndSize_8,7),
#else
    RegName_SPL=REGNAME(OpndKind_GPReg,OpndSize_8,4),
    RegName_BPL=REGNAME(OpndKind_GPReg,OpndSize_8,5),
    RegName_SIL=REGNAME(OpndKind_GPReg,OpndSize_8,6),
    RegName_DIL=REGNAME(OpndKind_GPReg,OpndSize_8,7),
    RegName_R8L=REGNAME(OpndKind_GPReg,OpndSize_8,8),
    RegName_R9L=REGNAME(OpndKind_GPReg,OpndSize_8,9),
    RegName_R10L=REGNAME(OpndKind_GPReg,OpndSize_8,10),
    RegName_R11L=REGNAME(OpndKind_GPReg,OpndSize_8,11),
    RegName_R12L=REGNAME(OpndKind_GPReg,OpndSize_8,12),
    RegName_R13L=REGNAME(OpndKind_GPReg,OpndSize_8,13),
    RegName_R14L=REGNAME(OpndKind_GPReg,OpndSize_8,14),
    RegName_R15L=REGNAME(OpndKind_GPReg,OpndSize_8,15),
#endif

    RegName_ES=REGNAME(OpndKind_SReg,OpndSize_16,0),
    RegName_CS=REGNAME(OpndKind_SReg,OpndSize_16,1),
    RegName_SS=REGNAME(OpndKind_SReg,OpndSize_16,2),
    RegName_DS=REGNAME(OpndKind_SReg,OpndSize_16,3),
    RegName_FS=REGNAME(OpndKind_SReg,OpndSize_16,4),
    RegName_GS=REGNAME(OpndKind_SReg,OpndSize_16,5),

    RegName_EFLAGS=REGNAME(OpndKind_StatusReg,OpndSize_32,0),

#if !defined(TESTING_ENCODER)
    RegName_FP0=REGNAME(OpndKind_FPReg,OpndSize_80,0),
    RegName_FP1=REGNAME(OpndKind_FPReg,OpndSize_80,1),
    RegName_FP2=REGNAME(OpndKind_FPReg,OpndSize_80,2),
    RegName_FP3=REGNAME(OpndKind_FPReg,OpndSize_80,3),
    RegName_FP4=REGNAME(OpndKind_FPReg,OpndSize_80,4),
    RegName_FP5=REGNAME(OpndKind_FPReg,OpndSize_80,5),
    RegName_FP6=REGNAME(OpndKind_FPReg,OpndSize_80,6),
    RegName_FP7=REGNAME(OpndKind_FPReg,OpndSize_80,7),
#endif
    RegName_FP0S=REGNAME(OpndKind_FPReg,OpndSize_32,0),
    RegName_FP1S=REGNAME(OpndKind_FPReg,OpndSize_32,1),
    RegName_FP2S=REGNAME(OpndKind_FPReg,OpndSize_32,2),
    RegName_FP3S=REGNAME(OpndKind_FPReg,OpndSize_32,3),
    RegName_FP4S=REGNAME(OpndKind_FPReg,OpndSize_32,4),
    RegName_FP5S=REGNAME(OpndKind_FPReg,OpndSize_32,5),
    RegName_FP6S=REGNAME(OpndKind_FPReg,OpndSize_32,6),
    RegName_FP7S=REGNAME(OpndKind_FPReg,OpndSize_32,7),

    RegName_FP0D=REGNAME(OpndKind_FPReg,OpndSize_64,0),
    RegName_FP1D=REGNAME(OpndKind_FPReg,OpndSize_64,1),
    RegName_FP2D=REGNAME(OpndKind_FPReg,OpndSize_64,2),
    RegName_FP3D=REGNAME(OpndKind_FPReg,OpndSize_64,3),
    RegName_FP4D=REGNAME(OpndKind_FPReg,OpndSize_64,4),
    RegName_FP5D=REGNAME(OpndKind_FPReg,OpndSize_64,5),
    RegName_FP6D=REGNAME(OpndKind_FPReg,OpndSize_64,6),
    RegName_FP7D=REGNAME(OpndKind_FPReg,OpndSize_64,7),

#if !defined(TESTING_ENCODER)
    RegName_XMM0=REGNAME(OpndKind_XMMReg,OpndSize_128,0),
    RegName_XMM1=REGNAME(OpndKind_XMMReg,OpndSize_128,1),
    RegName_XMM2=REGNAME(OpndKind_XMMReg,OpndSize_128,2),
    RegName_XMM3=REGNAME(OpndKind_XMMReg,OpndSize_128,3),
    RegName_XMM4=REGNAME(OpndKind_XMMReg,OpndSize_128,4),
    RegName_XMM5=REGNAME(OpndKind_XMMReg,OpndSize_128,5),
    RegName_XMM6=REGNAME(OpndKind_XMMReg,OpndSize_128,6),
    RegName_XMM7=REGNAME(OpndKind_XMMReg,OpndSize_128,7),

#ifdef _EM64T_
    RegName_XMM8  = REGNAME(OpndKind_XMMReg,OpndSize_128,0),
    RegName_XMM9  = REGNAME(OpndKind_XMMReg,OpndSize_128,1),
    RegName_XMM10 = REGNAME(OpndKind_XMMReg,OpndSize_128,2),
    RegName_XMM11 = REGNAME(OpndKind_XMMReg,OpndSize_128,3),
    RegName_XMM12 = REGNAME(OpndKind_XMMReg,OpndSize_128,4),
    RegName_XMM13 = REGNAME(OpndKind_XMMReg,OpndSize_128,5),
    RegName_XMM14 = REGNAME(OpndKind_XMMReg,OpndSize_128,6),
    RegName_XMM15 = REGNAME(OpndKind_XMMReg,OpndSize_128,7),
#endif //~_EM64T_

#endif  // ~TESTING_ENCODER

    RegName_XMM0S=REGNAME(OpndKind_XMMReg,OpndSize_32,0),
    RegName_XMM1S=REGNAME(OpndKind_XMMReg,OpndSize_32,1),
    RegName_XMM2S=REGNAME(OpndKind_XMMReg,OpndSize_32,2),
    RegName_XMM3S=REGNAME(OpndKind_XMMReg,OpndSize_32,3),
    RegName_XMM4S=REGNAME(OpndKind_XMMReg,OpndSize_32,4),
    RegName_XMM5S=REGNAME(OpndKind_XMMReg,OpndSize_32,5),
    RegName_XMM6S=REGNAME(OpndKind_XMMReg,OpndSize_32,6),
    RegName_XMM7S=REGNAME(OpndKind_XMMReg,OpndSize_32,7),
#ifdef _EM64T_
    RegName_XMM8S=REGNAME(OpndKind_XMMReg,OpndSize_32,8),
    RegName_XMM9S=REGNAME(OpndKind_XMMReg,OpndSize_32,9),
    RegName_XMM10S=REGNAME(OpndKind_XMMReg,OpndSize_32,10),
    RegName_XMM11S=REGNAME(OpndKind_XMMReg,OpndSize_32,11),
    RegName_XMM12S=REGNAME(OpndKind_XMMReg,OpndSize_32,12),
    RegName_XMM13S=REGNAME(OpndKind_XMMReg,OpndSize_32,13),
    RegName_XMM14S=REGNAME(OpndKind_XMMReg,OpndSize_32,14),
    RegName_XMM15S=REGNAME(OpndKind_XMMReg,OpndSize_32,15),
#endif // ifdef _EM64T_
    RegName_XMM0D=REGNAME(OpndKind_XMMReg,OpndSize_64,0),
    RegName_XMM1D=REGNAME(OpndKind_XMMReg,OpndSize_64,1),
    RegName_XMM2D=REGNAME(OpndKind_XMMReg,OpndSize_64,2),
    RegName_XMM3D=REGNAME(OpndKind_XMMReg,OpndSize_64,3),
    RegName_XMM4D=REGNAME(OpndKind_XMMReg,OpndSize_64,4),
    RegName_XMM5D=REGNAME(OpndKind_XMMReg,OpndSize_64,5),
    RegName_XMM6D=REGNAME(OpndKind_XMMReg,OpndSize_64,6),
    RegName_XMM7D=REGNAME(OpndKind_XMMReg,OpndSize_64,7),
#ifdef _EM64T_
    RegName_XMM8D=REGNAME(OpndKind_XMMReg,OpndSize_64,8),
    RegName_XMM9D=REGNAME(OpndKind_XMMReg,OpndSize_64,9),
    RegName_XMM10D=REGNAME(OpndKind_XMMReg,OpndSize_64,10),
    RegName_XMM11D=REGNAME(OpndKind_XMMReg,OpndSize_64,11),
    RegName_XMM12D=REGNAME(OpndKind_XMMReg,OpndSize_64,12),
    RegName_XMM13D=REGNAME(OpndKind_XMMReg,OpndSize_64,13),
    RegName_XMM14D=REGNAME(OpndKind_XMMReg,OpndSize_64,14),
    RegName_XMM15D=REGNAME(OpndKind_XMMReg,OpndSize_64,15),
#endif // ifdef _EM64T_
#ifdef _HAVE_MMX_
    RegName_MMX0=REGNAME(OpndKind_MMXReg,OpndSize_64,0),
    RegName_MMX1=REGNAME(OpndKind_MMXReg,OpndSize_64,1),
    RegName_MMX2=REGNAME(OpndKind_MMXReg,OpndSize_64,2),
    RegName_MMX3=REGNAME(OpndKind_MMXReg,OpndSize_64,3),
    RegName_MMX4=REGNAME(OpndKind_MMXReg,OpndSize_64,4),
    RegName_MMX5=REGNAME(OpndKind_MMXReg,OpndSize_64,5),
    RegName_MMX6=REGNAME(OpndKind_MMXReg,OpndSize_64,6),
    RegName_MMX7=REGNAME(OpndKind_MMXReg,OpndSize_64,7),
#endif  // _HAVE_MMX_
} RegName;

#if 0   // Android x86: use mnemonics defined in enc_defs_ext.h
/**
 * Conditional mnemonics.
 * The values match the 'real' (==processor's) values of the appropriate
 * condition values used in the opcodes.
 */
enum ConditionMnemonic {

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
};


#define CCM(prefix,cond) Mnemonic_##prefix##cond=Mnemonic_##prefix##cc+ConditionMnemonic_##cond

//=========================================================================================================
enum Mnemonic {

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

#ifdef _HAVE_MMX_
    Mnemonic_PADDQ,                     // Add Packed Quadword Integers
    Mnemonic_PAND,                      // Logical AND
    Mnemonic_POR,                       // Bitwise Logical OR
    Mnemonic_PSUBQ,                     // Subtract Packed Quadword Integers
#endif

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
Mnemonic_SAR,                           // Shift right
Mnemonic_ROR,                           // Rotate right
Mnemonic_RCR,                           // Rotate right through CARRY flag
Mnemonic_ROL,                           // Rotate left
Mnemonic_RCL,                           // Rotate left through CARRY flag
Mnemonic_SHR,                           // Unsigned shift right
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
};

#undef CCM
#endif

/**
 * @brief Instruction prefixes, according to arch manual.
 */
typedef enum InstPrefix {
    InstPrefix_Null = 0,
    // Group 1
    InstPrefix_LOCK = 0xF0,
    InstPrefix_REPNE = 0xF2,
    InstPrefix_REPNZ = InstPrefix_REPNE,
    InstPrefix_REP = 0xF3, InstPrefix_REPZ = InstPrefix_REP,
    // Group 2
    InstPrefix_CS = 0x2E,
    InstPrefix_SS = 0x36,
    InstPrefix_DS = 0x3E,
    InstPrefix_ES = 0x26,
    InstPrefix_FS = 0x64,
    InstPrefix_GS = 0x65,
    //
    InstPrefix_HintTaken = 0x3E,
    InstPrefix_HintNotTaken = 0x2E,
    // Group 3
    InstPrefix_OpndSize = 0x66,
    // Group 4
    InstPrefix_AddrSize = 0x67
} InstPrefix;

inline unsigned getSizeBytes(OpndSize sz)
{
    if (sz==OpndSize_64) { return 8; }
    if (sz==OpndSize_32) { return 4; }
    if (sz==OpndSize_16) { return 2; }
    if (sz==OpndSize_8)  { return 1; }
    assert(false);
    return 0;
}

inline bool isRegKind(OpndKind kind)
{
    return OpndKind_GPReg<= kind && kind<=OpndKind_MaxRegKind;
}

/**
 * @brief Returns #RegName for a given name.
 *
 * Name is case-insensitive.
 * @param regname - string name of a register
 * @return #RegName for the given name, or #RegName_Null if name is invalid
 */
RegName         getRegName(const char * regname);
/**
 * Constructs RegName from the given OpndKind, size and index.
 */
inline RegName  getRegName(OpndKind k, OpndSize s, int idx)
{
    return (RegName)REGNAME(k,s,idx);
}
/**
 * Extracts a bit mask with a bit set at the position of the register's index.
 */
inline unsigned getRegMask(RegName reg)
{
    return 1<<(reg&0xff);
}
/**
 * @brief Extracts #RegKind from the #RegName.
 */
inline OpndKind getRegKind(RegName reg)
{
    return (OpndKind)(reg>>24);
}
/**
 * @brief Extracts #OpndSize from #RegName.
 */
inline OpndSize getRegSize(RegName reg)
{
    return (OpndSize)((reg>>16)&0xFF);
}
/**
 * Extracts an index from the given RegName.
 */
inline unsigned char getRegIndex(RegName reg)
{
    return (unsigned char)(reg&0xFF);
}
/**
 * Returns a string name of the given RegName. The name returned is in upper-case.
 * Returns NULL if invalid RegName specified.
 */
const char *    getRegNameString(RegName reg);
/**
 * Returns string name of a given OpndSize.
 * Returns NULL if invalid OpndSize passed.
 */
const char *    getOpndSizeString(OpndSize size);
/**
 * Returns OpndSize passed by its string representation (case insensitive).
 * Returns OpndSize_Null if invalid string specified.
 * The 'sizeString' can not be NULL.
 */
OpndSize        getOpndSize(const char * sizeString);
/**
 * Returns string name of a given OpndKind.
 * Returns NULL if the passed kind is invalid.
 */
const char *    getOpndKindString(OpndKind kind);
/**
 * Returns OpndKind found by its string representation (case insensitive).
 * Returns OpndKind_Null if the name is invalid.
 * The 'kindString' can not be NULL.
 */
OpndKind        getOpndKind(const char * kindString);
/**
 *
 */
const char *    getConditionString(ConditionMnemonic cm);

/**
 * Constructs an RegName with the same index and kind, but with a different size from
 * the given RegName (i.e. getRegAlias(EAX, OpndSize_16) => AX; getRegAlias(BL, OpndSize_32) => EBX).
 * The constructed RegName is not checked in any way and thus may be invalid.
 * Note, that the aliasing does not work for at least AH,BH,CH,DH, ESI, EDI, ESP and EBP regs.
 */
inline RegName getAliasReg(RegName reg, OpndSize sz)
{
    return (RegName)REGNAME(getRegKind(reg), sz, getRegIndex(reg));
}

/**
 * brief Tests two RegName-s of the same kind for equality.
 *
 * @note Does work for 8 bit general purpose registers (AH, AL, BH, BL, etc).
 */
inline bool equals(RegName r0, RegName r1)
{
    return getRegKind(r0) == getRegKind(r1) &&
           getRegIndex(r0) == getRegIndex(r1);
}

ENCODER_NAMESPACE_END

#endif  // ifndef _ENCODER_DEFS_H_
