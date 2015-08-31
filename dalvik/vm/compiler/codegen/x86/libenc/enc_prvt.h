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
#ifndef __ENC_PRVT_H_INCLUDED__
#define __ENC_PRVT_H_INCLUDED__

#include "enc_base.h"

ENCODER_NAMESPACE_START
/*
 * @file
 * @brief Contains some definitions/constants and other stuff used by the
 *        Encoder internally.
 */

enum OpcodeByteKind {
    //OpcodeByteKind_Opcode = 0x0000,
    OpcodeByteKind_ZeroOpcodeByte           = 0x0100,
    //
    // The names _SlashR,  _SlahsNum, _ib, _iw, etc
    // represent the appropriate abbreviations used
    // in the mnemonic descriptions in the Intel's arch manual.
    //
    OpcodeByteKind_SlashR                   = 0x0200,
    OpcodeByteKind_SlashNum                 = 0x0300,
    OpcodeByteKind_ib                       = 0x0400,
    OpcodeByteKind_iw                       = 0x0500,
    OpcodeByteKind_id                       = 0x0600,
#ifdef _EM64T_
    OpcodeByteKind_io                       = 0x0700,
#endif
    OpcodeByteKind_cb                       = 0x0800,
    OpcodeByteKind_cw                       = 0x0900,
    OpcodeByteKind_cd                       = 0x0A00,
    //OpcodeByteKind_cp                     = 0x0B00,
    //OpcodeByteKind_co                     = 0x0C00,
    //OpcodeByteKind_ct                     = 0x0D00,

    OpcodeByteKind_rb                       = 0x0E00,
    OpcodeByteKind_rw                       = 0x0F00,
    OpcodeByteKind_rd                       = 0x1000,
#ifdef _EM64T_
    OpcodeByteKind_ro                       = 0x1100,
    //OpcodeByteKind_REX                    = 0x1200,
    OpcodeByteKind_REX_W                    = 0x1300,
#endif
    OpcodeByteKind_plus_i                   = 0x1400,
    /**
        * a special marker, means 'no opcode on the given position'
        * used in opcodes array, to specify the empty slot, say
        * to fill an em64t-specific opcode on ia32.
        * last 'e' made lowercase to avoid a mess with 'F' in
        * OpcodeByteKind_LAST .
        */
    OpcodeByteKind_EMPTY                    = 0xFFFE,
    /**
        * a special marker, means 'no more opcodes in the array'
        * used in in opcodes array to show that there are no more
        * opcodes in the array for a given mnemonic.
        */
    OpcodeByteKind_LAST                     = 0xFFFF,
    /**
        * a mask to extract the OpcodeByteKind
        */
    OpcodeByteKind_KindMask                 = 0xFF00,
    /**
        * a mask to extract the opcode byte when presented
        */
    OpcodeByteKind_OpcodeMask               = 0x00FF
};

#ifdef USE_ENCODER_DEFINES

#define N           {0, 0, 0, 0 }
#define U           {1, 0, 1, OpndRole_Use }
#define D           {1, 1, 0, OpndRole_Def }
#define DU          {1, 1, 1, OpndRole_Def|OpndRole_Use }

#define U_U         {2, 0, 2, OpndRole_Use<<2 | OpndRole_Use }
#define D_U         {2, 1, 1, OpndRole_Def<<2 | OpndRole_Use }
#define D_DU        {2, 2, 1, OpndRole_Def<<2 | (OpndRole_Def|OpndRole_Use) }
#define DU_U        {2, 1, 2, ((OpndRole_Def|OpndRole_Use)<<2 | OpndRole_Use) }
#define DU_DU       {2, 2, 2, ((OpndRole_Def|OpndRole_Use)<<2 | (OpndRole_Def|OpndRole_Use)) }

#define DU_DU_DU    {3, 3, 3, ((OpndRole_Def|OpndRole_Use)<<4) | ((OpndRole_Def|OpndRole_Use)<<2) | (OpndRole_Def|OpndRole_Use) }
#define DU_DU_U     {3, 2, 3, (((OpndRole_Def|OpndRole_Use)<<4) | ((OpndRole_Def|OpndRole_Use)<<2) | OpndRole_Use) }
#define D_DU_U      {3, 2, 2, (((OpndRole_Def)<<4) | ((OpndRole_Def|OpndRole_Use)<<2) | OpndRole_Use) }
#define D_U_U       {3, 1, 2, (((OpndRole_Def)<<4) | ((OpndRole_Use)<<2) | OpndRole_Use) }

// Special encoding of 0x00 opcode byte. Note: it's all O-s, not zeros.
#define OxOO        OpcodeByteKind_ZeroOpcodeByte

#define Size16      InstPrefix_OpndSize

#define _r          OpcodeByteKind_SlashR

#define _0          OpcodeByteKind_SlashNum|0
#define _1          OpcodeByteKind_SlashNum|1
#define _2          OpcodeByteKind_SlashNum|2
#define _3          OpcodeByteKind_SlashNum|3
#define _4          OpcodeByteKind_SlashNum|4
#define _5          OpcodeByteKind_SlashNum|5
#define _6          OpcodeByteKind_SlashNum|6
#define _7          OpcodeByteKind_SlashNum|7

// '+i' for floating-point instructions
#define _i          OpcodeByteKind_plus_i


#define ib          OpcodeByteKind_ib
#define iw          OpcodeByteKind_iw
#define id          OpcodeByteKind_id

#define cb          OpcodeByteKind_cb
#define cw          OpcodeByteKind_cw
#define cd          OpcodeByteKind_cd

#define rb          OpcodeByteKind_rb
#define rw          OpcodeByteKind_rw
#define rd          OpcodeByteKind_rd

#define AL          {OpndKind_GPReg, OpndSize_8, OpndExt_Any, RegName_AL}
#define AH          {OpndKind_GPReg, OpndSize_8, OpndExt_Any, RegName_AH}
#define AX          {OpndKind_GPReg, OpndSize_16, OpndExt_Any, RegName_AX}
#define EAX         {OpndKind_GPReg, OpndSize_32, OpndExt_Any, RegName_EAX}
#ifdef _EM64T_
    #define RAX     {OpndKind_GPReg, OpndSize_64, OpndExt_Any, RegName_RAX }
#endif

#define CL          {OpndKind_GPReg, OpndSize_8, OpndExt_Any, RegName_CL}
#define ECX         {OpndKind_GPReg, OpndSize_32, OpndExt_Any, RegName_ECX}
#ifdef _EM64T_
    #define RCX         {OpndKind_GPReg, OpndSize_64, OpndExt_Any, RegName_RCX}
#endif

#define DX          {OpndKind_GPReg, OpndSize_16, OpndExt_Any, RegName_DX}
#define EDX         {OpndKind_GPReg, OpndSize_32, OpndExt_Any, RegName_EDX}
#ifdef _EM64T_
    #define RDX     { OpndKind_GPReg, OpndSize_64, OpndExt_Any, RegName_RDX }
#endif

#define ESI         {OpndKind_GPReg, OpndSize_32, OpndExt_Any, RegName_ESI}
#ifdef _EM64T_
    #define RSI     { OpndKind_GPReg, OpndSize_64, OpndExt_Any, RegName_RSI }
#endif

#define EDI         {OpndKind_GPReg, OpndSize_32, OpndExt_Any, RegName_EDI}
#ifdef _EM64T_
    #define RDI     { OpndKind_GPReg, OpndSize_64, OpndExt_Any, RegName_RDI }
#endif

#define r8          {OpndKind_GPReg, OpndSize_8, OpndExt_Any, RegName_Null}
#define r16         {OpndKind_GPReg, OpndSize_16, OpndExt_Any, RegName_Null}
#define r32         {OpndKind_GPReg, OpndSize_32, OpndExt_Any, RegName_Null}
#ifdef _EM64T_
    #define r64     { OpndKind_GPReg, OpndSize_64, OpndExt_Any, RegName_Null }
#endif

#define r_m8        {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_8, OpndExt_Any, RegName_Null}
#define r_m16       {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_16, OpndExt_Any, RegName_Null}
#define r_m32       {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_32, OpndExt_Any, RegName_Null}

#define r_m8s        {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_8, OpndExt_Signed, RegName_Null}
#define r_m16s       {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_16, OpndExt_Signed, RegName_Null}
#define r_m32s       {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_32, OpndExt_Signed, RegName_Null}

#define r_m8u        {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_8, OpndExt_Zero, RegName_Null}
#define r_m16u       {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_16, OpndExt_Zero, RegName_Null}
#define r_m32u       {(OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_32, OpndExt_Zero, RegName_Null}

//'m' was only used in LEA mnemonic, but is replaced with
// set of exact sizes. See more comments for LEA instruction in TheTable.
//#define m           {OpndKind_Mem, OpndSize_Null, RegName_Null}
#define m8          {OpndKind_Mem, OpndSize_8, OpndExt_Any, RegName_Null}
#define m16         {OpndKind_Mem, OpndSize_16, OpndExt_Any, RegName_Null}
#define m32         {OpndKind_Mem, OpndSize_32, OpndExt_Any, RegName_Null}
#define m64         {OpndKind_Mem, OpndSize_64, OpndExt_Any, RegName_Null}
#ifdef _EM64T_
    #define r_m64   { (OpndKind)(OpndKind_GPReg|OpndKind_Mem), OpndSize_64, OpndExt_Any, RegName_Null }
#endif

#define imm8        {OpndKind_Imm, OpndSize_8, OpndExt_Any, RegName_Null}
#define imm16       {OpndKind_Imm, OpndSize_16, OpndExt_Any, RegName_Null}
#define imm32       {OpndKind_Imm, OpndSize_32, OpndExt_Any, RegName_Null}

#define imm8s        {OpndKind_Imm, OpndSize_8, OpndExt_Signed, RegName_Null}
#define imm16s       {OpndKind_Imm, OpndSize_16, OpndExt_Signed, RegName_Null}
#define imm32s       {OpndKind_Imm, OpndSize_32, OpndExt_Signed, RegName_Null}

#define imm8u        {OpndKind_Imm, OpndSize_8, OpndExt_Zero, RegName_Null}
#define imm16u       {OpndKind_Imm, OpndSize_16, OpndExt_Zero, RegName_Null}
#define imm32u       {OpndKind_Imm, OpndSize_32, OpndExt_Zero, RegName_Null}

#ifdef _EM64T_
    #define imm64   {OpndKind_Imm, OpndSize_64, OpndExt_Any, RegName_Null }
#endif

//FIXME: moff-s are in fact memory refs, but presented as immediate.
// Need to specify this in OpndDesc.
#define moff8        {OpndKind_Imm, OpndSize_32, OpndExt_Any, RegName_Null}
#define moff16       {OpndKind_Imm, OpndSize_32, OpndExt_Any, RegName_Null}
#define moff32       {OpndKind_Imm, OpndSize_32, OpndExt_Any, RegName_Null}
#ifdef _EM64T_
    #define moff64       {OpndKind_Imm, OpndSize_64, OpndExt_Any, RegName_Null}
#endif


#define rel8        {OpndKind_Imm, OpndSize_8, OpndExt_Any, RegName_Null}
#define rel16       {OpndKind_Imm, OpndSize_16, OpndExt_Any, RegName_Null}
#define rel32       {OpndKind_Imm, OpndSize_32, OpndExt_Any, RegName_Null}

#define mm64        {OpndKind_MMXReg, OpndSize_64, OpndExt_Any, RegName_Null}
#define mm_m64      {(OpndKind)(OpndKind_MMXReg|OpndKind_Mem), OpndSize_64, OpndExt_Any, RegName_Null}

#define xmm64       {OpndKind_XMMReg, OpndSize_64, OpndExt_Any, RegName_Null}
#define xmm_m64     {(OpndKind)(OpndKind_XMMReg|OpndKind_Mem), OpndSize_64, OpndExt_Any, RegName_Null}

#define xmm32       {OpndKind_XMMReg, OpndSize_32, OpndExt_Any, RegName_Null}
#define xmm_m32     {(OpndKind)(OpndKind_XMMReg|OpndKind_Mem), OpndSize_32, OpndExt_Any, RegName_Null}

#define FP0S        {OpndKind_FPReg, OpndSize_32, OpndExt_Any, RegName_FP0S}
#define FP0D        {OpndKind_FPReg, OpndSize_64, OpndExt_Any, RegName_FP0D}
#define FP1S        {OpndKind_FPReg, OpndSize_32, OpndExt_Any, RegName_FP1S}
#define FP1D        {OpndKind_FPReg, OpndSize_64, OpndExt_Any, RegName_FP1D}
#define fp32        {OpndKind_FPReg, OpndSize_32, OpndExt_Any, RegName_Null}
#define fp64        {OpndKind_FPReg, OpndSize_64, OpndExt_Any, RegName_Null}

#ifdef _EM64T_
    #define io      OpcodeByteKind_io
    #define REX_W   OpcodeByteKind_REX_W

#endif

#endif // USE_ENCODER_DEFINES

/**
 * @brief Represents the REX part of instruction.
 */
struct  Rex {
    unsigned char b : 1;
    unsigned char x : 1;
    unsigned char r : 1;
    unsigned char w : 1;
    unsigned char dummy : 4;        // must be '0100'b
    unsigned int  :24;
};

/**
 * @brief Describes SIB (scale,index,base) byte.
 */
struct SIB {
    unsigned char base:3;
    unsigned char index:3;
    unsigned char scale:2;
    unsigned int  padding:24;
};
/**
 * @brief Describes ModRM byte.
 */
struct ModRM
{
    unsigned char rm:3;
    unsigned char reg:3;
    unsigned char mod:2;
    unsigned int  padding:24;
};



/**
* exactly the same as EncoderBase::OpcodeDesc, but also holds info about
* platform on which the opcode is applicable.
*/
struct OpcodeInfo {
    enum platform {
        /// an opcode is valid on all platforms
        all,
        // opcode is valid on IA-32 only
        em64t,
        // opcode is valid on Intel64 only
        ia32,
        // opcode is added for the sake of disassembling, should not be used in encoding
        decoder,
        // only appears in master table, replaced with 'decoder' in hashed version
        decoder32,
        // only appears in master table, replaced with 'decoder' in hashed version
        decoder64,
    };
    platform                        platf;
    unsigned                        opcode[4+1+1];
    EncoderBase::OpndDesc           opnds[3];
    EncoderBase::OpndRolesDesc      roles;
};

/**
 * @defgroup MF_ Mnemonic flags
*/

    /**
 * Operation has no special properties.
    */
#define MF_NONE             (0x00000000)
    /**
 * Operation affects flags
    */
#define MF_AFFECTS_FLAGS    (0x00000001)
    /**
 * Operation uses flags - conditional operations, ADC/SBB/ETC
    */
#define MF_USES_FLAGS       (0x00000002)
    /**
 * Operation is conditional - MOVcc/SETcc/Jcc/ETC
    */
#define MF_CONDITIONAL      (0x00000004)
/**
 * Operation is symmetric - its args can be swapped (ADD/MUL/etc).
 */
#define MF_SYMMETRIC        (0x00000008)
/**
 * Operation is XOR-like - XOR, SUB - operations of 'arg,arg' is pure def,
 * without use.
 */
#define MF_SAME_ARG_NO_USE  (0x00000010)

///@} // ~MNF

/**
 * @see same structure as EncoderBase::MnemonicDesc, but carries
 * MnemonicInfo::OpcodeInfo[] instead of OpcodeDesc[].
 * Only used during prebuilding the encoding tables, thus it's hidden under
 * the appropriate define.
 */
struct MnemonicInfo {
    /**
    * The mnemonic itself
    */
    Mnemonic    mn;
    /**
     * Various characteristics of mnemonic.
     * @see MF_
     */
    unsigned    flags;
    /**
     * Number of args/des/uses/roles for the operation. For the operations
     * which may use different number of operands (i.e. IMUL/SHL) use the
     * most common value, or leave '0' if you are sure this info is not
     * required.
     */
    EncoderBase::OpndRolesDesc              roles;
    /**
     * Print name of the mnemonic
     */
    const char *                            name;
    /**
     * Array of opcodes.
     * The terminating opcode description always have OpcodeByteKind_LAST
     * at the opcodes[i].opcode[0].
     * The size of '25' has nothing behind it, just counted the max
     * number of opcodes currently used (MOV instruction).
     */
    OpcodeInfo                              opcodes[25];
};

ENCODER_NAMESPACE_END

#endif  // ~__ENC_PRVT_H_INCLUDED__
