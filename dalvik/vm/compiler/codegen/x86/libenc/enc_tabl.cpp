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


#include <assert.h>
#include <stdio.h>
#include <stdlib.h> //qsort
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <stdlib.h>


// need to use EM64T-specifics - new registers, defines from enc_prvt, etc...
#if !defined(_EM64T_)
    #define UNDEF_EM64T
    #define _EM64T_
#endif

#define USE_ENCODER_DEFINES
#include "enc_prvt.h"
#include "enc_defs.h"

#ifdef UNDEF_EM64T
    #undef _EM64T_
#endif

//Android x86
#if 0 //!defined(_HAVE_MMX_)
    #define Mnemonic_PADDQ  Mnemonic_Null
    #define Mnemonic_PAND   Mnemonic_Null
    #define Mnemonic_POR    Mnemonic_Null
    #define Mnemonic_PSUBQ  Mnemonic_Null
#endif

ENCODER_NAMESPACE_START


EncoderBase::MnemonicDesc EncoderBase::mnemonics[Mnemonic_Count];
EncoderBase::OpcodeDesc EncoderBase::opcodes[Mnemonic_Count][MAX_OPCODES];
unsigned char EncoderBase::opcodesHashMap[Mnemonic_Count][HASH_MAX];


/**
 * @file
 * @brief 'Master' copy of encoding data.
 */

/*
This file contains a 'master copy' of encoding table - this is the info used
by both generator of native instructions (EncoderBase class) and by
disassembling routines. The first one uses an info how to encode the
instruction, and the second does an opposite - several separate tables are
built at runtime from this main table.

=============================================================================

The table was designed for easy support and maintenance. Thus, it was made as
much close as possible to the Intel's IA32 Architecture Manual descriptions.
The info is based on the latest (at the moment of writing) revision which is
June 2005, order number 253666-016.

Normally, almost all of opcodes in the 'master' table represented exactly as
they are shown in the Intel's Architecture manual (well, with slashes
replaced with underscore). There are several exclusions especially marked.

Normally, to add an opcode/instruction, one only need to copy the whole
string from the manual, and simply replace '/' with '_'.

I.e., TheManual reads for DEC:
    (1)     FE /1 DEC r/m8 Valid Valid Decrement r/m8 by 1.
    (2)     REX + FE /1 DEC r/m8* Valid N.E. Decrement r/m8 by 1.
    (3)     REX.W + FF /1 DEC r/m64 Valid N.E. Decrement r/m64 by 1.

1. Note, that there is no need to explicitly specify REX-based opcodes for
    instruction to handle additional registers on EM64T:

    (1)     FE /1 DEC r/m8 Valid Valid Decrement r/m8 by 1.
    (3)     REX.W + FF /1 DEC r/m64 Valid N.E. Decrement r/m64 by 1.

2. Copy the string, strip off the text comments, replace '/'=>'_'. Note, that
    the second line is for EM64T only

    (1)     FE /1 DEC r/m8
    (3)     REX.W + FF /1 DEC r/m64

3. Fill out the mnemonic, opcode parameters parts

    BEGIN_MNEMONIC(DEC, MF_AFFECTS_FLAGS, DU)
    BEGIN_OPCODES()
        {OpcodeInfo::all,   {0xFE, _1},         {r_m8},         DU },
        {OpcodeInfo::em64t, {REX_W, 0xFF, _1},  {r_m64},        DU },

    DU here - one argument, it's used and defined

4. That's it, that simple !

The operand roles (DU here) are used by Jitrino's optimizing engine to
perform data flow analysis. It also used to store/obtain number of operands.

Special cases are (see the table for details):
LEA
Some FPU operations (i.e. FSTP)
packed things (XORPD, XORPS, CVTDQ2PD, CVTTPD2DQ)

Also, the Jitrino's needs require to specify all operands - including
implicit ones (see IMUL).

The master table iself does not need to be ordered - it's get sorted before
processing. It's recommended (though it's not a law) to group similar
instructions together - i.e. FPU instructions, MMX, etc.

=============================================================================

The encoding engine builds several tables basing on the 'master' one (here
'mnemonic' is a kind of synonim for 'instruction'):

- list of mnemonics which holds general info about instructions
    (EncoderBase::mnemonics)
- an array of opcodes descriptions (EncodeBase::opcodes)
- a mapping between a hash value and an opcode description record for a given
    mnemonic (EncoderBase::opcodesHashMap)

The EncoderBase::mnemonics holds general info about instructions.
The EncoderBase::opcodesHashMap is used for fast opcode selection basing on
a hash value.
The EncodeBase::opcodes is used for the encoding itself.

=============================================================================
The hash value is calculated and used as follows:

JIT-ted code uses the following operand sizes: 8-, 16-, 32- and 64-bits and
size for an operand can be encoded in just 2 bits.

The following operand locations are available: one of registers - GP, FP,
MMX, XMM (not taking segment registers), a memory and an immediate, which
gives us 6 variants and can be enumerated in 3 bits.

As a grand total, the the whole operand's info needed for opcode selection
can be packed in 5 bits. Taking into account the IMUL mnemonic with its 3
operands (including implicit ones), we're getting 15 bits per instruction and
the complete table is about 32768 items per single instruction.

Seems too many, but luckily, the 15 bit limit will never be reached: the
worst case is IMUL with its 3 operands:
(IMUL r64, r/m64, imm32)/(IMUL r32, r/m32, imm32).
So, assigning lowest value to GP register, the max value of hash can be
reduced.

The hash values to use are:
sizes:
        8               -> 11
        16              -> 10
        32              -> 01
        64              -> 00
locations:
        gp reg          -> 000
        memory          -> 001
        fp reg          -> 010
        mmx reg         -> 011
        xmm reg         -> 100
        immediate       -> 101
and the grand total for the worst case would be
[ GP 32] [GP  32] [Imm 32]
[000-01] [000-01] [101 01] = 1077

However, the implicit operands adds additional value, and the worstest case
is 'SHLD r_m32, r32, CL=r8'. This gives us the maximum number of:

[mem 32] [GP  32] [GP  8b]
[001-01] [000-01] [000-11] = 5155.

The max number is pretty big and the hash functions is quite rare, thus it
is not resonable to use a direct addressing i.e.
OpcodeDesc[mnemonic][hash_code] - there would be a huge waste of space.

Instead, we use a kind of mapping: the opcodes info is stored in packed
(here: non rare) array. The max number of opcodes will not exceed 255 for
each instruction. And we have an index array in which we store a mapping
between a hash code value and opcode position for each given instruction.

Sounds a bit sophisticated, but in real is simple, the opcode gets selected
in 2 simple steps:

1. Select [hash,mnemonic] => 'n'.

The array is pretty rare - many cells contain 0xFF which
means 'invalid hash - no opcode with given characteristics'

char EnbcoderBase::opcodesHashMap[Mnemonic_Count][HASH_MAX] =

+----+----+----+----+----+----+
| 00 | 05 | FF | FF | 03 | 12 | ...
|---------+-------------------+
| 12 | FF | FF |  n | 04 | 25 | ...   <- Mnemonic
|-----------------------------+
| FF | 11 | FF | 10 | 13 | .. | ...
+-----------------------------+
     ...         ^
                 |
                hash

2. Select [n,mnemonic] => 'opcode_desc11'

OpcodeDesc      EncoderBase::opcodes[Mnemonic_Count][MAX_OPCODES] =

+---------------+---------------+---------------+---------------+
| opcode_desc00 | opcode_desc01 | opcode_desc02 | last_opcode   | ...
+---------------+---------------+---------------+---------------+
| opcode_desc10 | opcode_desc11 | last_opcode   | xxx           | <- Mnemonic
+---------------+---------------+---------------+---------------+
| opcode_desc20 | opcode_desc21 | opcode_desc22 | opcode_desc23 | ...
+---------------+---------------+---------------+---------------+
     ...
                      ^
                      |
                      n

Now, use 'opcode_desc11'.

=============================================================================
The array of opcodes descriptions (EncodeBase::opcodes) is specially prepared
to maximize performance - the EncoderBase::encode() is quite hot on client
applications for the Jitrino/Jitrino.JET.
The preparation is that opcode descriptions from the 'master' encoding table
are preprocessed and a special set of OpcodeDesc prepared:
First, the 'raw' opcode bytes are extracted. Here, 'raw' means the bytes that
do not depened on any operands values, do not require any analysis and can be
simply copied into the output buffer during encoding. Also, number of these
'raw' bytes is counted. The fields are OpcodeDesc::opcode and
OpcodeDesc::opcode_len.

Then the fisrt non-implicit operand found and its index is stored in
OpcodeDesc::first_opnd.

The bytes that require processing and analysis ('/r', '+i', etc) are
extracted and stored in OpcodeDesc::aux0 and OpcodeDesc::aux1 fields.

Here, a special trick is performed:
    Some opcodes have register/memory operand, but this is not reflected in
    opcode column - for example, (MOVQ xmm64, xmm_m64). In this case, a fake
    '_r' added to OpcodeDesc::aux field.
    Some other opcodes have immediate operands, but this is again not
    reflected in opcode column - for example, CALL cd or PUSH imm32.
    In this case, a fake '/cd' or fake '/id' added to appropriate
    OpcodeDesc::aux field.

The OpcodeDesc::last is non-zero for the final OpcodeDesc record (which does
not have valid data itself).
*/

// TODO: To extend flexibility, replace bool fields in MnemonicDesc &
// MnemonicInfo with a set of flags packed into integer field.

unsigned short EncoderBase::getHash(const OpcodeInfo* odesc)
{
    /*
    NOTE: any changes in the hash computation must be stricty balanced with
    EncoderBase::Operand::hash_it and EncoderBase::Operands()
    */
    unsigned short hash = 0;
    // The hash computation, uses fast way - table selection instead of if-s.
    if (odesc->roles.count > 0) {
        OpndKind kind = odesc->opnds[0].kind;
        OpndSize size = odesc->opnds[0].size;
        assert(kind<COUNTOF(kind_hash));
        assert(size<COUNTOF(size_hash));
        hash = get_kind_hash(kind) | get_size_hash(size);
    }

    if (odesc->roles.count > 1) {
        OpndKind kind = odesc->opnds[1].kind;
        OpndSize size = odesc->opnds[1].size;
        assert(kind<COUNTOF(kind_hash));
        assert(size<COUNTOF(size_hash));
        hash = (hash<<HASH_BITS_PER_OPERAND) |
               (get_kind_hash(kind) | get_size_hash(size));
    }

    if (odesc->roles.count > 2) {
        OpndKind kind = odesc->opnds[2].kind;
        OpndSize size = odesc->opnds[2].size;
        assert(kind<COUNTOF(kind_hash));
        assert(size<COUNTOF(size_hash));
        hash = (hash<<HASH_BITS_PER_OPERAND) |
            (get_kind_hash(kind) | get_size_hash(size));
    }
    assert(hash <= HASH_MAX);
    return hash;
}


#define BEGIN_MNEMONIC(mn, flags, roles)     \
        { Mnemonic_##mn, flags, roles, #mn,
#define END_MNEMONIC() },
#define BEGIN_OPCODES() {
#define END_OPCODES()   { OpcodeInfo::all, {OpcodeByteKind_LAST} }}

//#define BEGIN_MNEMONIC(mn, affflags, ulags, cond, symm, roles)     \
//        { Mnemonic_##mn, affflags, ulags, cond, symm, roles, #mn,


static MnemonicInfo masterEncodingTable[] = {
//
// Null
//
BEGIN_MNEMONIC(Null, MF_NONE, N)
BEGIN_OPCODES()
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(LAHF, MF_USES_FLAGS, D)
BEGIN_OPCODES()
// TheManual says it's not always supported in em64t mode, thus excluding it
    {OpcodeInfo::ia32,    {0x9F},         {EAX}, D },
END_OPCODES()
END_MNEMONIC()
//
// ALU mnemonics - add, adc, or, xor, and, cmp, sub, sbb
// as they differ only in the opcode extention (/digit) number and
// in which number the opcode start from, the opcode definitions
// for those instructions are packed together
//
// The 'opcode_starts_from' and 'opcode_ext' in DEFINE_ALU_OPCODES()
// are enough to define OpcodeInfo::all opcodes and the 'first_opcode'
// parameter is only due to ADD instruction, which requires an zero opcode
// byte which, in turn, is coded especially in the current coding scheme.
//

#define DEFINE_ALU_OPCODES( opc_ext, opcode_starts_from, first_opcode, def_use ) \
\
    {OpcodeInfo::decoder,   {opcode_starts_from + 4, ib},           {AL,    imm8},  DU_U },\
    {OpcodeInfo::decoder,   {Size16, opcode_starts_from + 5, iw},   {AX,    imm16}, DU_U },\
    {OpcodeInfo::decoder,   {opcode_starts_from + 5, id},           {EAX,   imm32}, DU_U },\
    {OpcodeInfo::decoder64, {REX_W, opcode_starts_from+5, id},      {RAX,   imm32s},DU_U },\
\
    {OpcodeInfo::all,       {0x80, opc_ext, ib},          {r_m8,  imm8},    def_use },\
    {OpcodeInfo::all,       {Size16, 0x81, opc_ext, iw},  {r_m16, imm16},   def_use },\
    {OpcodeInfo::all,       {0x81, opc_ext, id},          {r_m32, imm32},   def_use },\
    {OpcodeInfo::em64t,     {REX_W, 0x81, opc_ext, id},   {r_m64, imm32s},  def_use },\
\
    {OpcodeInfo::all,       {Size16, 0x83, opc_ext, ib},  {r_m16, imm8s},   def_use },\
    {OpcodeInfo::all,       {0x83, opc_ext, ib},          {r_m32, imm8s},   def_use },\
    {OpcodeInfo::em64t,     {REX_W, 0x83, opc_ext, ib},   {r_m64, imm8s},   def_use },\
\
    {OpcodeInfo::all,       {first_opcode,  _r},          {r_m8,  r8},      def_use },\
\
    {OpcodeInfo::all,       {Size16, opcode_starts_from+1,  _r},  {r_m16, r16},   def_use },\
    {OpcodeInfo::all,       {opcode_starts_from+1,  _r},  {r_m32, r32},   def_use },\
    {OpcodeInfo::em64t,     {REX_W, opcode_starts_from+1, _r},    {r_m64, r64},   def_use },\
\
    {OpcodeInfo::all,       {opcode_starts_from+2,  _r},  {r8,    r_m8},  def_use },\
\
    {OpcodeInfo::all,       {Size16, opcode_starts_from+3,  _r},  {r16,   r_m16}, def_use },\
    {OpcodeInfo::all,       {opcode_starts_from+3,  _r},  {r32,   r_m32}, def_use },\
    {OpcodeInfo::em64t,     {REX_W, opcode_starts_from+3, _r},    {r64,   r_m64}, def_use },

BEGIN_MNEMONIC(ADD, MF_AFFECTS_FLAGS|MF_SYMMETRIC, DU_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES(_0, 0x00, OxOO, DU_U )
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(OR, MF_AFFECTS_FLAGS|MF_SYMMETRIC, DU_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES(_1, 0x08, 0x08, DU_U )
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(ADC, MF_AFFECTS_FLAGS|MF_USES_FLAGS|MF_SYMMETRIC, DU_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES(_2, 0x10, 0x10, DU_U )
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(SBB, MF_AFFECTS_FLAGS|MF_USES_FLAGS, DU_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES(_3, 0x18, 0x18, DU_U )
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(AND, MF_AFFECTS_FLAGS|MF_SYMMETRIC, DU_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES(_4, 0x20, 0x20, DU_U )
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(SUB, MF_AFFECTS_FLAGS|MF_SAME_ARG_NO_USE, DU_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES(_5, 0x28, 0x28, DU_U )
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(XOR, MF_AFFECTS_FLAGS|MF_SYMMETRIC|MF_SAME_ARG_NO_USE, DU_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES( _6, 0x30, 0x30, DU_U )
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CMP, MF_AFFECTS_FLAGS, U_U)
BEGIN_OPCODES()
    DEFINE_ALU_OPCODES( _7, 0x38, 0x38, U_U )
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CMPXCHG, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0xB0, _r},           {r_m8, r8, AL},     DU_DU_DU },
    {OpcodeInfo::all,   {Size16, 0x0F, 0xB1, _r},   {r_m16, r16, AX},   DU_DU_DU },
    {OpcodeInfo::all,   {0x0F, 0xB1, _r},           {r_m32, r32, EAX},  DU_DU_DU},
    {OpcodeInfo::em64t, {REX_W, 0x0F, 0xB1, _r},    {r_m64, r64, RAX},  DU_DU_DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CMPXCHG8B, MF_AFFECTS_FLAGS, D)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0xC7, _1},         {m64},     DU },
END_OPCODES()
END_MNEMONIC()

#undef DEFINE_ALU_OPCODES
//
//
//
BEGIN_MNEMONIC(ADDSD, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF2, 0x0F, 0x58, _r},   {xmm64, xmm_m64},   DU_U},
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(ADDSS, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x58, _r},   {xmm32, xmm_m32},   DU_U},
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(BSF, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0xBC},   {r32, r_m32},   D_U},
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(BSR, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0xBD},   {r32, r_m32},   D_U},
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(CALL, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xE8, cd},        {rel32},     U },
    {OpcodeInfo::ia32,  {Size16, 0xE8, cw}, {rel16},    U },
    {OpcodeInfo::ia32,  {0xFF, _2},        {r_m32},     U },
    {OpcodeInfo::em64t, {0xFF, _2},        {r_m64},     U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CMC, MF_USES_FLAGS|MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::decoder,   {0xF5},         {},     N },
END_OPCODES()
END_MNEMONIC()

//TODO: Workaround. Actually, it's D_DU, but Jitrino's CG thinks it's D_U
BEGIN_MNEMONIC(CDQ, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,       {0x99},         {DX, AX},       D_U },
    {OpcodeInfo::all,       {0x99},         {EDX, EAX},     D_U },
    {OpcodeInfo::em64t,     {REX_W, 0x99},  {RDX, RAX},     D_U },
END_OPCODES()
END_MNEMONIC()

#define DEFINE_CMOVcc_MNEMONIC( cc ) \
        BEGIN_MNEMONIC(CMOV##cc, MF_USES_FLAGS|MF_CONDITIONAL, DU_U ) \
BEGIN_OPCODES() \
    {OpcodeInfo::all,   {Size16, 0x0F, 0x40 + ConditionMnemonic_##cc, _r},  {r16, r_m16},   DU_U }, \
    {OpcodeInfo::all,   {0x0F, 0x40 + ConditionMnemonic_##cc, _r},          {r32, r_m32},   DU_U }, \
    {OpcodeInfo::em64t, {REX_W, 0x0F, 0x40 + ConditionMnemonic_##cc, _r},   {r64, r_m64},   DU_U }, \
END_OPCODES() \
END_MNEMONIC()

DEFINE_CMOVcc_MNEMONIC(O)
DEFINE_CMOVcc_MNEMONIC(NO)
DEFINE_CMOVcc_MNEMONIC(B)
DEFINE_CMOVcc_MNEMONIC(NB)
DEFINE_CMOVcc_MNEMONIC(Z)
DEFINE_CMOVcc_MNEMONIC(NZ)
DEFINE_CMOVcc_MNEMONIC(BE)
DEFINE_CMOVcc_MNEMONIC(NBE)
DEFINE_CMOVcc_MNEMONIC(S)
DEFINE_CMOVcc_MNEMONIC(NS)
DEFINE_CMOVcc_MNEMONIC(P)
DEFINE_CMOVcc_MNEMONIC(NP)
DEFINE_CMOVcc_MNEMONIC(L)
DEFINE_CMOVcc_MNEMONIC(NL)
DEFINE_CMOVcc_MNEMONIC(LE)
DEFINE_CMOVcc_MNEMONIC(NLE)

#undef DEFINE_CMOVcc_MNEMONIC

/*****************************************************************************
                                ***** SSE conversion routines *****
*****************************************************************************/
//
// double -> float
BEGIN_MNEMONIC(CVTSD2SS, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF2, 0x0F, 0x5A, _r},   {xmm32, xmm_m64}, D_U },
END_OPCODES()
END_MNEMONIC()

// double -> I_32
BEGIN_MNEMONIC(CVTSD2SI, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF2, 0x0F, 0x2D, _r},         {r32, xmm_m64}, D_U },
    {OpcodeInfo::em64t, {REX_W, 0xF2, 0x0F, 0x2D, _r},  {r64, xmm_m64}, D_U },
END_OPCODES()
END_MNEMONIC()

// double [truncated] -> I_32
BEGIN_MNEMONIC(CVTTSD2SI, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF2, 0x0F, 0x2C, _r},         {r32, xmm_m64}, D_U },
    {OpcodeInfo::em64t, {REX_W, 0xF2, 0x0F, 0x2C, _r},  {r64, xmm_m64}, D_U },
END_OPCODES()
END_MNEMONIC()

// float -> double
BEGIN_MNEMONIC(CVTSS2SD, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x5A, _r},         {xmm64, xmm_m32}, D_U },
END_OPCODES()
END_MNEMONIC()

// float -> I_32
BEGIN_MNEMONIC(CVTSS2SI, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x2D, _r},         {r32, xmm_m32}, D_U},
    {OpcodeInfo::em64t, {REX_W, 0xF3, 0x0F, 0x2D, _r},  {r64, xmm_m32}, D_U},
END_OPCODES()
END_MNEMONIC()

// float [truncated] -> I_32
BEGIN_MNEMONIC(CVTTSS2SI, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x2C, _r},         {r32, xmm_m32}, D_U},
    {OpcodeInfo::em64t, {REX_W, 0xF3, 0x0F, 0x2C, _r},  {r64, xmm_m32}, D_U},
END_OPCODES()
END_MNEMONIC()

// I_32 -> double
BEGIN_MNEMONIC(CVTSI2SD, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF2, 0x0F, 0x2A, _r},         {xmm64, r_m32}, D_U},
    {OpcodeInfo::em64t, {REX_W, 0xF2, 0x0F, 0x2A, _r},  {xmm64, r_m64}, D_U},
END_OPCODES()
END_MNEMONIC()

// I_32 -> float
BEGIN_MNEMONIC(CVTSI2SS, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x2A, _r},         {xmm32, r_m32}, D_U},
    {OpcodeInfo::em64t, {REX_W, 0xF3, 0x0F, 0x2A, _r},  {xmm32, r_m64}, D_U},
END_OPCODES()
END_MNEMONIC()

//
// ~ SSE conversions
//

BEGIN_MNEMONIC(DEC, MF_AFFECTS_FLAGS, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xFE, _1},         {r_m8},     DU },

    {OpcodeInfo::all,   {Size16, 0xFF, _1}, {r_m16},    DU },
    {OpcodeInfo::all,   {0xFF, _1},         {r_m32},    DU },
    {OpcodeInfo::em64t, {REX_W, 0xFF, _1},  {r_m64},    DU },

    {OpcodeInfo::ia32,  {Size16, 0x48|rw},  {r16},      DU },
    {OpcodeInfo::ia32,  {0x48|rd},          {r32},      DU },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(DIVSD, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all, {0xF2, 0x0F, 0x5E, _r},   {xmm64, xmm_m64},   DU_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(DIVSS, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all, {0xF3, 0x0F, 0x5E, _r},   {xmm32, xmm_m32},   DU_U },
END_OPCODES()
END_MNEMONIC()

/****************************************************************************
                 ***** FPU operations *****
****************************************************************************/

BEGIN_MNEMONIC(FADDP, MF_NONE, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDE, 0xC1},       {FP0D}, DU },
    {OpcodeInfo::all,   {0xDE, 0xC1},       {FP0S}, DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FLDZ,  MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, 0xEE},   {FP0D}, D },
    {OpcodeInfo::all,   {0xD9, 0xEE},   {FP0S}, D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FADD,  MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDC, _0},     {FP0D, m64}, DU_U },
    {OpcodeInfo::all,   {0xD8, _0},     {FP0S, m32}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FSUBP, MF_NONE, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDE, 0xE9},   {FP0D}, DU },
    {OpcodeInfo::all,   {0xDE, 0xE9},   {FP0S}, DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FSUB,   MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDC, _4},     {FP0D, m64}, DU_U },
    {OpcodeInfo::all,   {0xD8, _4},     {FP0S, m32}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FISUB,   MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDA, _4},       {FP0S, m32}, DU_U },
//    {OpcodeInfo::all,   {0xDE, _4},       {FP0S, m16}, DU_U },
END_OPCODES()
END_MNEMONIC()



BEGIN_MNEMONIC(FMUL,   MF_NONE, DU_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD8, _1},     {FP0S, m32}, DU_U },
    {OpcodeInfo::all,   {0xDC, _1},     {FP0D, m64}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FMULP, MF_NONE, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDE, 0xC9},   {FP0D}, DU },
    {OpcodeInfo::all,   {0xDE, 0xC9},   {FP0S}, DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FDIVP, MF_NONE, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDE, 0xF9},   {FP0D}, DU },
    {OpcodeInfo::all,   {0xDE, 0xF9},   {FP0S}, DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FDIV,   MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDC, _6},     {FP0D, m64}, DU_U },
    {OpcodeInfo::all,   {0xD8, _6},     {FP0S, m32}, DU_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(FUCOM, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDD, 0xE1},         {FP0D, FP1D},    DU_U },
    {OpcodeInfo::all,   {0xDD, 0xE1},         {FP0S, FP1S},    DU_U },
    // A little trick: actually, these 2 opcodes take only index of the
    // needed register. To make the things similar to other instructions
    // we encode here as if they took FPREG.
    {OpcodeInfo::all,   {0xDD, 0xE0|_i},    {fp32},         DU },
    {OpcodeInfo::all,   {0xDD, 0xE0|_i},    {fp64},         DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FUCOMI, MF_NONE, D_U )
BEGIN_OPCODES()
    // A little trick: actually, these 2 opcodes take only index of the
    // needed register. To make the things similar to other instructions
    // we encode here as if they took FPREG.
    {OpcodeInfo::all,   {0xDB, 0xE8|_i},    {fp32},         DU },
    {OpcodeInfo::all,   {0xDB, 0xE8|_i},    {fp64},         DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FUCOMP, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDD, 0xE9},             {FP0D, FP1D},    DU_U },
    {OpcodeInfo::all,   {0xDD, 0xE9},             {FP0S, FP1S},    DU_U },
    // A little trick: actually, these 2 opcodes take only index of the
    // needed register. To make the things similar to other instructions
    // we encode here as if they took FPREG.
    {OpcodeInfo::all,   {0xDD, 0xE8|_i},        {fp32},         DU },
    {OpcodeInfo::all,   {0xDD, 0xE8|_i},        {fp64},         DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FUCOMIP, MF_NONE, D_U )
BEGIN_OPCODES()
    // A little trick: actually, these 2 opcodes take only index of the
    // needed register. To make the things similar to other instructions
    // we encode here as if they took FPREG.
    {OpcodeInfo::all,   {0xDF, 0xE8|_i},        {fp32},         DU },
    {OpcodeInfo::all,   {0xDF, 0xE8|_i},        {fp64},         DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FUCOMPP, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDA, 0xE9},   {FP0D, FP1D}, DU_U },
    {OpcodeInfo::all,   {0xDA, 0xE9},   {FP0S, FP1S}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FLDCW, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, _5},     {m16},  U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FNSTCW, MF_NONE, D)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, _7},     {m16},  D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FSTSW, MF_NONE, D)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x9B, 0xDF, 0xE0}, {EAX},  D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FNSTSW, MF_NONE, D)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDF, 0xE0},   {EAX},  D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FCHS, MF_NONE, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, 0xE0},   {FP0D}, DU },
    {OpcodeInfo::all,   {0xD9, 0xE0},   {FP0S}, DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FCLEX, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x9B, 0xDB, 0xE2}, {}, N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FNCLEX, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDB, 0xE2},       {}, N },
END_OPCODES()
END_MNEMONIC()

//BEGIN_MNEMONIC(FDECSTP, MF_NONE, N)
//  BEGIN_OPCODES()
//          {OpcodeInfo::all, {0xD9, 0xF6},       {},     N },
//  END_OPCODES()
//END_MNEMONIC()

BEGIN_MNEMONIC(FILD, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDB, _0}, {FP0S, m32},    D_U },
    {OpcodeInfo::all,   {0xDF, _5}, {FP0D, m64},    D_U },
    {OpcodeInfo::all,   {0xDB, _0}, {FP0S, m32},    D_U },
END_OPCODES()
END_MNEMONIC()

//BEGIN_MNEMONIC(FINCSTP, MF_NONE, N)
//  BEGIN_OPCODES()
//          {OpcodeInfo::all, {0xD9, 0xF7},       {},     N },
//  END_OPCODES()
//END_MNEMONIC()

BEGIN_MNEMONIC(FIST, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDB, _2}, {m32, FP0S},    D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FISTP, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDB, _3}, {m32, FP0S},    D_U },
    {OpcodeInfo::all,   {0xDF, _7}, {m64, FP0D},    D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FISTTP, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xDD, _1}, {m64, FP0D},    D_U },
    {OpcodeInfo::all,   {0xDB, _1}, {m32, FP0S},    D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FRNDINT, MF_NONE, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, 0xFC}, {FP0S},    DU },
    {OpcodeInfo::all,   {0xD9, 0xFC}, {FP0D},    DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FLD, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, _0}, {FP0S, m32},    D_U },
    {OpcodeInfo::all,   {0xDD, _0}, {FP0D, m64},    D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FLDLG2, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, 0xEC}, {FP0S},    D },
    {OpcodeInfo::all,   {0xD9, 0xEC}, {FP0D},    D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FLDLN2, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, 0xED}, {FP0S},    D },
    {OpcodeInfo::all,   {0xD9, 0xED}, {FP0D},    D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FLD1, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, 0xE8}, {FP0S},    D },
    {OpcodeInfo::all,   {0xD9, 0xE8}, {FP0D},    D },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(FPREM, MF_NONE, N)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xF8},       {},     N },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FPREM1, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, 0xF5},       {},     N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FST, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, _2},         {m32, FP0S},    D_U },
    {OpcodeInfo::all,   {0xDD, _2},         {m64, FP0D},    D_U },
    // A little trick: actually, these 2 opcodes take only index of the
    // needed register. To make the things similar to other instructions
    // we encode here as if they took FPREG.
    {OpcodeInfo::all,   {0xDD, 0xD0|_i},    {fp32},         D },
    {OpcodeInfo::all,   {0xDD, 0xD0|_i},    {fp64},         D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FSTP, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xD9, _3},             {m32, FP0S},    D_U },
    {OpcodeInfo::all,   {0xDD, _3},             {m64, FP0D},    D_U },
    // A little trick: actually, these 2 opcodes take only index of the
    // needed register. To make the things similar to other instructions
    // we encode here as if they took FPREG.
    {OpcodeInfo::all,   {0xDD, 0xD8|_i},        {fp32},         D },
    {OpcodeInfo::all,   {0xDD, 0xD8|_i},        {fp64},         D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FSQRT, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xFA},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xFA},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(FYL2X, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xF1},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xF1},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(FYL2XP1, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xF9},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xF9},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(F2XM1, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xF0},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xF0},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FPATAN, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xF3},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xF3},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FXCH, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xC9},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xC9},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FSCALE, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xFD},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xFD},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FABS, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xE1},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xE1},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FSIN, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xFE},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xFE},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FCOS, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xFF},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xFF},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(FPTAN, MF_NONE, DU)
  BEGIN_OPCODES()
          {OpcodeInfo::all, {0xD9, 0xF2},       {FP0S},     DU   },
          {OpcodeInfo::all, {0xD9, 0xF2},       {FP0D},     DU   },
  END_OPCODES()
END_MNEMONIC()

//
// ~ FPU
//

BEGIN_MNEMONIC(DIV, MF_AFFECTS_FLAGS, DU_DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF7, _6},         {EDX, EAX, r_m32},  DU_DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(IDIV, MF_AFFECTS_FLAGS, DU_DU_U)
BEGIN_OPCODES()
#if !defined(_EM64T_)
    {OpcodeInfo::all,   {0xF6, _7},         {AH, AL, r_m8},     DU_DU_U },
    {OpcodeInfo::all,   {Size16, 0xF7, _7}, {DX, AX, r_m16},    DU_DU_U },
#endif
    {OpcodeInfo::all,   {0xF7, _7},         {EDX, EAX, r_m32},  DU_DU_U },
    {OpcodeInfo::em64t, {REX_W, 0xF7, _7},  {RDX, RAX, r_m64},  DU_DU_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(IMUL, MF_AFFECTS_FLAGS, D_DU_U)
BEGIN_OPCODES()
    /*{OpcodeInfo::all,   {0xF6, _5},               {AH, AL,        r_m8},  D_DU_U },
    {OpcodeInfo::all,     {Size16, 0xF7, _5},       {DX, AX,        r_m16}, D_DU_U },
    */
    //
    {OpcodeInfo::all,     {0xF7, _5},               {EDX, EAX, r_m32},  D_DU_U },
    //todo: this opcode's hash conflicts with IMUL r64,r_m64 - they're both 0.
    // this particular is not currently used, so we may safely drop it, but need to
    // revisit the hash implementation
    // {OpcodeInfo::em64t,   {REX_W, 0xF7, _5},        {RDX, RAX, r_m64},  D_DU_U },
    //
    {OpcodeInfo::all,   {Size16, 0x0F, 0xAF, _r}, {r16,r_m16},        DU_U },
    {OpcodeInfo::all,   {0x0F, 0xAF, _r},         {r32,r_m32},        DU_U },
    {OpcodeInfo::em64t, {REX_W, 0x0F, 0xAF, _r},  {r64,r_m64},        DU_U },
    {OpcodeInfo::all,   {Size16, 0x6B, _r, ib},   {r16,r_m16,imm8s},  D_DU_U },
    {OpcodeInfo::all,   {0x6B, _r, ib},           {r32,r_m32,imm8s},  D_DU_U },
    {OpcodeInfo::em64t, {REX_W, 0x6B, _r, ib},    {r64,r_m64,imm8s},  D_DU_U },
    {OpcodeInfo::all,   {Size16, 0x6B, _r, ib},   {r16,imm8s},        DU_U },
    {OpcodeInfo::all,   {0x6B, _r, ib},           {r32,imm8s},        DU_U },
    {OpcodeInfo::em64t, {REX_W, 0x6B, _r, ib},    {r64,imm8s},        DU_U },
    {OpcodeInfo::all,   {Size16, 0x69, _r, iw},   {r16,r_m16,imm16},  D_U_U },
    {OpcodeInfo::all,   {0x69, _r, id},           {r32,r_m32,imm32},  D_U_U },
    {OpcodeInfo::em64t, {REX_W, 0x69, _r, id},    {r64,r_m64,imm32s}, D_U_U },
    {OpcodeInfo::all,   {Size16, 0x69, _r, iw},   {r16,imm16},        DU_U },
    {OpcodeInfo::all,   {0x69, _r, id},           {r32,imm32},        DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MUL, MF_AFFECTS_FLAGS, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF6, _4},           {AX, AL, r_m8},     D_DU_U },
    {OpcodeInfo::all,   {Size16, 0xF7, _4},   {DX, AX, r_m16},    D_DU_U },
    {OpcodeInfo::all,   {0xF7, _4},           {EDX, EAX, r_m32},  D_DU_U },
    {OpcodeInfo::em64t, {REX_W, 0xF7, _4},    {RDX, RAX, r_m64},  D_DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(INC, MF_AFFECTS_FLAGS, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xFE, _0},           {r_m8},         DU },
    {OpcodeInfo::all,   {Size16, 0xFF, _0},   {r_m16},        DU },
    {OpcodeInfo::all,   {0xFF, _0},           {r_m32},        DU },
    {OpcodeInfo::em64t, {REX_W, 0xFF, _0},    {r_m64},        DU },
    {OpcodeInfo::ia32,  {Size16, 0x40|rw},    {r16},          DU },
    {OpcodeInfo::ia32,  {0x40|rd},            {r32},          DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(INT3, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xCC},     {},     N },
END_OPCODES()
END_MNEMONIC()

#define DEFINE_Jcc_MNEMONIC( cc ) \
        BEGIN_MNEMONIC(J##cc, MF_USES_FLAGS|MF_CONDITIONAL, U ) \
BEGIN_OPCODES() \
    {OpcodeInfo::all,   {0x70 + ConditionMnemonic_##cc, cb },           { rel8 },       U }, \
    {OpcodeInfo::ia32,  {Size16, 0x0F, 0x80 + ConditionMnemonic_##cc, cw},      { rel16 },      U }, \
    {OpcodeInfo::all,   {0x0F, 0x80 + ConditionMnemonic_##cc, cd},      { rel32 },      U }, \
END_OPCODES() \
END_MNEMONIC()


DEFINE_Jcc_MNEMONIC(O)
DEFINE_Jcc_MNEMONIC(NO)
DEFINE_Jcc_MNEMONIC(B)
DEFINE_Jcc_MNEMONIC(NB)
DEFINE_Jcc_MNEMONIC(Z)
DEFINE_Jcc_MNEMONIC(NZ)
DEFINE_Jcc_MNEMONIC(BE)
DEFINE_Jcc_MNEMONIC(NBE)

DEFINE_Jcc_MNEMONIC(S)
DEFINE_Jcc_MNEMONIC(NS)
DEFINE_Jcc_MNEMONIC(P)
DEFINE_Jcc_MNEMONIC(NP)
DEFINE_Jcc_MNEMONIC(L)
DEFINE_Jcc_MNEMONIC(NL)
DEFINE_Jcc_MNEMONIC(LE)
DEFINE_Jcc_MNEMONIC(NLE)

#undef DEFINE_Jcc_MNEMONIC

BEGIN_MNEMONIC(JMP, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xEB, cb},         {rel8},     U },
    {OpcodeInfo::ia32,  {Size16, 0xE9, cw}, {rel16},    U },
    {OpcodeInfo::all,   {0xE9, cd},         {rel32},    U },
    {OpcodeInfo::ia32,  {Size16, 0xFF, _4}, {r_m16},    U },
    {OpcodeInfo::ia32,  {0xFF, _4},         {r_m32},    U },
    {OpcodeInfo::em64t, {0xFF, _4},         {r_m64},    U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(LEA, MF_NONE, D_U )
BEGIN_OPCODES()
    /*
    A special case: the LEA instruction itself does not care about size of
    second operand. This is obviuos why it is, and thus in The Manual, a
    simple 'm' without size is used.
    However, in the Jitrino's instrucitons we'll have an operand with a size.
    Also, the hashing scheme is not supposed to handle OpndSize_Null, and
    making it to do so will lead to unnecessary complication of hashing
    scheme. Thus, instead of handling it as a special case, we simply make
    copies of the opcodes with sizes set.
        {OpcodeInfo::all,     {0x8D, _r},             {r32, m},       D_U },
        {OpcodeInfo::em64t, {0x8D, _r},               {r64, m},       D_U },
    */
    //Android x86: keep r32, m32 only, otherwise, will have decoding error
    //{OpcodeInfo::all,   {0x8D, _r},     {r32, m8},      D_U },
    {OpcodeInfo::em64t, {REX_W, 0x8D, _r},     {r64, m8},      D_U },
    //{OpcodeInfo::all,   {0x8D, _r},     {r32, m16},     D_U },
    {OpcodeInfo::em64t, {REX_W, 0x8D, _r},     {r64, m16},     D_U },
    {OpcodeInfo::all,   {0x8D, _r},     {r32, m32},     D_U },
    {OpcodeInfo::em64t, {REX_W, 0x8D, _r},     {r64, m32},     D_U },
    {OpcodeInfo::all,   {0x8D, _r},     {r32, m64},     D_U },
    {OpcodeInfo::em64t, {REX_W, 0x8D, _r},     {r64, m64},     D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(LOOP, MF_AFFECTS_FLAGS|MF_USES_FLAGS, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xE2, cb},     {ECX, rel8},    DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(LOOPE, MF_AFFECTS_FLAGS|MF_USES_FLAGS, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xE1, cb},     {ECX, rel8},    DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(LOOPNE, MF_AFFECTS_FLAGS|MF_USES_FLAGS, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xE0, cb},     {ECX, rel8},    DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MOV, MF_NONE, D_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x88, _r},         {r_m8,r8},      D_U },

    {OpcodeInfo::all,   {Size16, 0x89, _r}, {r_m16,r16},    D_U },
    {OpcodeInfo::all,   {0x89, _r},         {r_m32,r32},    D_U },
    {OpcodeInfo::em64t, {REX_W, 0x89, _r},  {r_m64,r64},    D_U },
    {OpcodeInfo::all,   {0x8A, _r},         {r8,r_m8},      D_U },

    {OpcodeInfo::all,   {Size16, 0x8B, _r}, {r16,r_m16},    D_U },
    {OpcodeInfo::all,   {0x8B, _r},         {r32,r_m32},    D_U },
    {OpcodeInfo::em64t, {REX_W, 0x8B, _r},  {r64,r_m64},    D_U },

    {OpcodeInfo::all,   {0xB0|rb},          {r8,imm8},      D_U },

    {OpcodeInfo::all,   {Size16, 0xB8|rw},  {r16,imm16},    D_U },
    {OpcodeInfo::all,   {0xB8|rd},          {r32,imm32},    D_U },
    {OpcodeInfo::em64t, {REX_W, 0xB8|rd},   {r64,imm64},    D_U },
    {OpcodeInfo::all,   {0xC6, _0},         {r_m8,imm8},    D_U },

    {OpcodeInfo::all,   {Size16, 0xC7, _0}, {r_m16,imm16},  D_U },
    {OpcodeInfo::all,   {0xC7, _0},         {r_m32,imm32},  D_U },
    {OpcodeInfo::em64t, {REX_W, 0xC7, _0},  {r_m64,imm32s}, D_U },

    {OpcodeInfo::decoder,   {0xA0},         {AL,  moff8},  D_U },
    {OpcodeInfo::decoder,   {Size16, 0xA1}, {AX,  moff16},  D_U },
    {OpcodeInfo::decoder,   {0xA1},         {EAX, moff32},  D_U },
    //{OpcodeInfo::decoder64,   {REX_W, 0xA1},  {RAX, moff64},  D_U },

    {OpcodeInfo::decoder,   {0xA2},         {moff8, AL},  D_U },
    {OpcodeInfo::decoder,   {Size16, 0xA3}, {moff16, AX},  D_U },
    {OpcodeInfo::decoder,   {0xA3},         {moff32, EAX},  D_U },
    //{OpcodeInfo::decoder64,   {REX_W, 0xA3},  {moff64, RAX},  D_U },
END_OPCODES()
END_MNEMONIC()



BEGIN_MNEMONIC(XCHG, MF_NONE, DU_DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x87, _r},   {r_m32,r32},    DU_DU },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(MOVQ, MF_NONE, D_U )
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0x6F, _r},   {mm64, mm_m64}, D_U },
    {OpcodeInfo::all,   {0x0F, 0x7F, _r},   {mm_m64, mm64}, D_U },
#endif
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x7E },  {xmm64, xmm_m64},       D_U },
    {OpcodeInfo::all,   {0x66, 0x0F, 0xD6 },  {xmm_m64, xmm64},       D_U },
//    {OpcodeInfo::em64t, {REX_W, 0x66, 0x0F, 0x6E, _r},  {xmm64, r_m64}, D_U },
//    {OpcodeInfo::em64t, {REX_W, 0x66, 0x0F, 0x7E, _r},  {r_m64, xmm64}, D_U },
    {OpcodeInfo::em64t, {REX_W, 0x66, 0x0F, 0x6E, _r},  {xmm64, r64}, D_U },
    {OpcodeInfo::em64t, {REX_W, 0x66, 0x0F, 0x7E, _r},  {r64, xmm64}, D_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(MOVD, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x66, 0x0F, 0x6E, _r}, {xmm32, r_m32}, D_U },
    {OpcodeInfo::all,   {0x66, 0x0F, 0x7E, _r}, {r_m32, xmm32}, D_U },
END_OPCODES()
END_MNEMONIC()

//
// A bunch of MMX instructions
//
#ifdef _HAVE_MMX_

BEGIN_MNEMONIC(EMMS, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0x77},       {},             N },
END_OPCODES()
END_MNEMONIC()

#endif

BEGIN_MNEMONIC(PADDQ, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xD4, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xD4, _r},   {xmm64, xmm_m64}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(PAND, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xDB, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xDB, _r},   {xmm64, xmm_m64}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(POR, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xEB, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xEB, _r},   {xmm64, xmm_m64}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(PSUBQ, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xFB, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xFB, _r},   {xmm64, xmm_m64}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(PANDN, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xDF, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xDF, _r}, {xmm64, xmm_m64},   DU_U },
END_OPCODES()
END_MNEMONIC()
BEGIN_MNEMONIC(PSLLQ, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xF3, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xF3, _r}, {xmm64, xmm_m64},   DU_U },
END_OPCODES()
END_MNEMONIC()
BEGIN_MNEMONIC(PSRLQ, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xD3, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xD3, _r}, {xmm64, xmm_m64},   DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(PXOR, MF_NONE, DU_U)
BEGIN_OPCODES()
#ifdef _HAVE_MMX_
    {OpcodeInfo::all,   {0x0F, 0xEF, _r},   {mm64, mm_m64}, DU_U },
#endif
    {OpcodeInfo::all,   {0x66, 0x0F, 0xEF, _r}, {xmm64, xmm_m64},   DU_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(MOVAPD, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x66, 0x0F, 0x28, _r},   {xmm64, xmm_m64},   D_U },
    {OpcodeInfo::all,   {0x66, 0x0F, 0x29, _r},   {xmm_m64, xmm64},   D_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(MOVSD, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all, {0xF2, 0x0F, 0x10, _r},   {xmm64, xmm_m64},   D_U },
    {OpcodeInfo::all, {0xF2, 0x0F, 0x11, _r},   {xmm_m64, xmm64},   D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MOVSS, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all, {0xF3, 0x0F, 0x10, _r},   {xmm32, xmm_m32}, D_U },
    {OpcodeInfo::all, {0xF3, 0x0F, 0x11, _r},   {xmm_m32, xmm32}, D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MOVSX, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,       {Size16, 0x0F, 0xBE, _r}, {r16, r_m8s},     D_U },
    {OpcodeInfo::all,       {0x0F, 0xBE, _r},         {r32, r_m8s},     D_U },
    {OpcodeInfo::em64t,     {REX_W, 0x0F, 0xBE, _r},  {r64, r_m8s},     D_U },

    {OpcodeInfo::all,       {0x0F, 0xBF, _r},         {r32, r_m16s},    D_U },
    {OpcodeInfo::em64t,     {REX_W, 0x0F, 0xBF, _r},  {r64, r_m16s},    D_U },

    {OpcodeInfo::em64t,     {REX_W, 0x63, _r},        {r64, r_m32s},    D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MOVZX, MF_NONE, D_U )
BEGIN_OPCODES()
    {OpcodeInfo::all,       {Size16, 0x0F, 0xB6, _r}, {r16, r_m8u},     D_U },
    {OpcodeInfo::all,       {0x0F, 0xB6, _r},         {r32, r_m8u},     D_U },
    {OpcodeInfo::em64t,     {REX_W, 0x0F, 0xB6, _r},  {r64, r_m8u},     D_U },

    {OpcodeInfo::all,       {0x0F, 0xB7, _r},         {r32, r_m16u},    D_U },
    {OpcodeInfo::em64t,     {REX_W, 0x0F, 0xB7, _r},  {r64, r_m16u},    D_U },
    //workaround to get r/rm32->r64 ZX mov functionality:
    //simple 32bit reg copying zeros high bits in 64bit reg
    {OpcodeInfo::em64t,     {0x8B, _r},               {r64, r_m32u},    D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MULSD, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF2, 0x0F, 0x59, _r}, {xmm64, xmm_m64},   DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MULSS, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x59, _r}, {xmm32, xmm_m32}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(NEG, MF_AFFECTS_FLAGS, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF6, _3},         {r_m8},         DU },

    {OpcodeInfo::all,   {Size16, 0xF7, _3}, {r_m16},        DU },
    {OpcodeInfo::all,   {0xF7, _3},         {r_m32},        DU },
    {OpcodeInfo::em64t, {REX_W, 0xF7, _3},  {r_m64},        DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(NOP, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x90}, {},     N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(NOT, MF_AFFECTS_FLAGS, DU )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF6, _2},           {r_m8},         DU },
    {OpcodeInfo::all,   {Size16, 0xF7, _2},   {r_m16},        DU },
    {OpcodeInfo::all,   {0xF7, _2},           {r_m32},        DU },
    {OpcodeInfo::em64t, {REX_W, 0xF7, _2},    {r_m64},        DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(POP, MF_NONE, D)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {Size16, 0x8F, _0}, {r_m16},    D },
    {OpcodeInfo::ia32,  {0x8F, _0},         {r_m32},    D },
    {OpcodeInfo::em64t, {0x8F, _0},         {r_m64},    D },

    {OpcodeInfo::all,   {Size16, 0x58|rw }, {r16},      D },
    {OpcodeInfo::ia32,  {0x58|rd },         {r32},      D },
    {OpcodeInfo::em64t, {0x58|rd },         {r64},      D },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(POPFD, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x9D},     {},         N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(PREFETCH, MF_NONE, U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0x18, _0},   {m8},         U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(PUSH, MF_NONE, U )
BEGIN_OPCODES()
    {OpcodeInfo::all,   {Size16, 0xFF, _6}, {r_m16},    U },
    {OpcodeInfo::ia32,  {0xFF, _6},         {r_m32},    U },
    {OpcodeInfo::em64t, {0xFF, _6},         {r_m64},    U },

    {OpcodeInfo::all,   {Size16, 0x50|rw }, {r16},      U },
    {OpcodeInfo::ia32,  {0x50|rd },         {r32},      U },
    {OpcodeInfo::em64t, {0x50|rd },         {r64},      U },

    {OpcodeInfo::all,   {0x6A},         {imm8},     U },
    {OpcodeInfo::all,   {Size16, 0x68}, {imm16},    U },
    {OpcodeInfo::ia32,  {0x68},         {imm32},    U },
//          {OpcodeInfo::em64t,   {0x68},   {imm64},    U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(PUSHFD, MF_USES_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x9C},             {},        N },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(RET, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xC3},       {},         N },
    {OpcodeInfo::all,   {0xC2, iw},   {imm16},    U },
END_OPCODES()
END_MNEMONIC()

#define DEFINE_SETcc_MNEMONIC( cc ) \
        BEGIN_MNEMONIC(SET##cc, MF_USES_FLAGS|MF_CONDITIONAL, DU) \
BEGIN_OPCODES() \
    {OpcodeInfo::all,   {0x0F,     0x90 + ConditionMnemonic_##cc}, {r_m8},  DU }, \
END_OPCODES() \
END_MNEMONIC()

DEFINE_SETcc_MNEMONIC(O)
DEFINE_SETcc_MNEMONIC(NO)
DEFINE_SETcc_MNEMONIC(B)
DEFINE_SETcc_MNEMONIC(NB)
DEFINE_SETcc_MNEMONIC(Z)
DEFINE_SETcc_MNEMONIC(NZ)
DEFINE_SETcc_MNEMONIC(BE)
DEFINE_SETcc_MNEMONIC(NBE)

DEFINE_SETcc_MNEMONIC(S)
DEFINE_SETcc_MNEMONIC(NS)
DEFINE_SETcc_MNEMONIC(P)
DEFINE_SETcc_MNEMONIC(NP)
DEFINE_SETcc_MNEMONIC(L)
DEFINE_SETcc_MNEMONIC(NL)
DEFINE_SETcc_MNEMONIC(LE)
DEFINE_SETcc_MNEMONIC(NLE)

#undef DEFINE_SETcc_MNEMONIC

#define DEFINE_SHIFT_MNEMONIC(nam, slash_num, flags) \
BEGIN_MNEMONIC(nam, flags, DU_U) \
BEGIN_OPCODES()\
    /* D0 & D1 opcodes are added w/o 2nd operand (1) because */\
    /* they are used for decoding only so only instruction length is needed */\
    {OpcodeInfo::decoder,   {0xD0, slash_num},            {r_m8/*,const_1*/},   DU },\
    {OpcodeInfo::all,       {0xD2, slash_num},              {r_m8,  CL},        DU_U },\
    {OpcodeInfo::all,       {0xC0, slash_num, ib},          {r_m8,  imm8},      DU_U },\
\
    {OpcodeInfo::decoder,   {Size16, 0xD1, slash_num},    {r_m16/*,const_1*/},  DU },\
    {OpcodeInfo::all,       {Size16, 0xD3, slash_num},      {r_m16, CL},        DU_U },\
    {OpcodeInfo::all,       {Size16, 0xC1, slash_num, ib},  {r_m16, imm8 },     DU_U },\
\
    {OpcodeInfo::decoder,   {0xD1, slash_num},              {r_m32/*,const_1*/}, DU },\
    {OpcodeInfo::decoder64, {REX_W, 0xD1, slash_num},       {r_m64/*,const_1*/}, DU },\
\
    {OpcodeInfo::all,       {0xD3, slash_num},              {r_m32, CL},        DU_U },\
    {OpcodeInfo::em64t,     {REX_W, 0xD3, slash_num},       {r_m64, CL},        DU_U },\
\
    {OpcodeInfo::all,       {0xC1, slash_num, ib},          {r_m32, imm8},      DU_U },\
    {OpcodeInfo::em64t,     {REX_W, 0xC1, slash_num, ib},   {r_m64, imm8},      DU_U },\
END_OPCODES()\
END_MNEMONIC()


DEFINE_SHIFT_MNEMONIC(ROL, _0, MF_AFFECTS_FLAGS)
DEFINE_SHIFT_MNEMONIC(ROR, _1, MF_AFFECTS_FLAGS)
DEFINE_SHIFT_MNEMONIC(RCL, _2, MF_AFFECTS_FLAGS|MF_USES_FLAGS)
DEFINE_SHIFT_MNEMONIC(RCR, _3, MF_AFFECTS_FLAGS|MF_USES_FLAGS)

DEFINE_SHIFT_MNEMONIC(SAL, _4, MF_AFFECTS_FLAGS)
DEFINE_SHIFT_MNEMONIC(SHR, _5, MF_AFFECTS_FLAGS)
DEFINE_SHIFT_MNEMONIC(SAR, _7, MF_AFFECTS_FLAGS)

#undef DEFINE_SHIFT_MNEMONIC

BEGIN_MNEMONIC(SHLD, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0xA5},   {r_m32, r32, CL}, DU_DU_U },
    {OpcodeInfo::all,   {0x0F, 0xA4},   {r_m32, r32, imm8}, DU_DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(SHRD, MF_AFFECTS_FLAGS, N)
// TODO: the def/use info is wrong
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0xAD},   {r_m32, r32, CL}, DU_DU_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(SUBSD, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF2, 0x0F, 0x5C, _r}, {xmm64, xmm_m64}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(SUBSS, MF_NONE, DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x5C, _r}, {xmm32, xmm_m32}, DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(TEST, MF_AFFECTS_FLAGS, U_U)
BEGIN_OPCODES()

    {OpcodeInfo::decoder,   {0xA8, ib},             { AL, imm8},    U_U },
    {OpcodeInfo::decoder,   {0xA9, iw},             { AX, imm16},   U_U },
    {OpcodeInfo::decoder,   {0xA9, id},             { EAX, imm32},  U_U },
    {OpcodeInfo::decoder64, {REX_W, 0xA9, id},      { RAX, imm32s}, U_U },

    {OpcodeInfo::all,       {0xF6, _0, ib},         {r_m8,imm8},    U_U },

    {OpcodeInfo::all,       {Size16, 0xF7, _0, iw}, {r_m16,imm16},  U_U },
    {OpcodeInfo::all,       {0xF7, _0, id},         {r_m32,imm32},  U_U },
    {OpcodeInfo::em64t,     {REX_W, 0xF7, _0, id},  {r_m64,imm32s}, U_U },

    {OpcodeInfo::all,       {0x84, _r},             {r_m8,r8},      U_U },

    {OpcodeInfo::all,       {Size16, 0x85, _r},     {r_m16,r16},    U_U },
    {OpcodeInfo::all,       {0x85, _r},             {r_m32,r32},    U_U },
    {OpcodeInfo::em64t,     {REX_W, 0x85, _r},      {r_m64,r64},    U_U },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(UCOMISD, MF_AFFECTS_FLAGS, U_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x66, 0x0F, 0x2E, _r}, {xmm64, xmm_m64}, U_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(UCOMISS, MF_AFFECTS_FLAGS, U_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0x2E, _r},       {xmm32, xmm_m32}, U_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(COMISD, MF_AFFECTS_FLAGS, U_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x66, 0x0F, 0x2F, _r}, {xmm64, xmm_m64}, U_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(COMISS, MF_AFFECTS_FLAGS, U_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x0F, 0x2F, _r},       {xmm32, xmm_m32}, U_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(XORPD, MF_SAME_ARG_NO_USE|MF_SYMMETRIC, DU_U)
BEGIN_OPCODES()
    //Note: they're actually 128 bits
    {OpcodeInfo::all,   {0x66, 0x0F, 0x57, _r},   {xmm64, xmm_m64},   DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(XORPS, MF_SAME_ARG_NO_USE|MF_SYMMETRIC, DU_U)
BEGIN_OPCODES()
    //Note: they're actually 128 bits
    {OpcodeInfo::all,   {0x0F, 0x57, _r},   {xmm32, xmm_m32},       DU_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CVTDQ2PD, MF_NONE, D_U )
BEGIN_OPCODES()
    //Note: they're actually 128 bits
    {OpcodeInfo::all,   {0xF3, 0x0F, 0xE6}, {xmm64, xmm_m64},   D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CVTDQ2PS, MF_NONE, D_U )
BEGIN_OPCODES()
    //Note: they're actually 128 bits
    {OpcodeInfo::all,   {0x0F, 0x5B, _r},   {xmm32, xmm_m32},   D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CVTTPD2DQ, MF_NONE, D_U )
BEGIN_OPCODES()
    //Note: they're actually 128 bits
    {OpcodeInfo::all,   {0x66, 0x0F, 0xE6}, {xmm64, xmm_m64},   D_U },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CVTTPS2DQ, MF_NONE, D_U )
BEGIN_OPCODES()
    //Note: they're actually 128 bits
    {OpcodeInfo::all,   {0xF3, 0x0F, 0x5B, _r},   {xmm32, xmm_m32},   D_U },
END_OPCODES()
END_MNEMONIC()

//
// String operations
//
BEGIN_MNEMONIC(STD, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xFD},         {},     N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CLD, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xFC},         {},     N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(SCAS, MF_AFFECTS_FLAGS, N)
// to be symmetric, this mnemonic must have either m32 or RegName_EAX
// but as long, as Jitrino's CG does not use the mnemonic, leaving it
// in its natural form
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xAF},         {},     N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(STOS, MF_AFFECTS_FLAGS, DU_DU_U)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0xAB},         {EDI, ECX, EAX},   DU_DU_U },
    {OpcodeInfo::all,   {0xAA},         {EDI, ECX, AL},    DU_DU_U },
    {OpcodeInfo::em64t, {REX_W, 0xAB},  {RDI, RCX, RAX},   DU_DU_U },
END_OPCODES()
END_MNEMONIC()

/*
MOVS and CMPS are the special cases.
Most the code in both CG and Encoder do not expect 2 memory operands.
Also, they are not supposed to setup constrains on which register the
memory reference must reside - m8,m8 or m32,m32 is not the choice.
We can't use r8,r8 either - will have problem with 8bit EDI, ESI.
So, as the workaround we do r32,r32 and specify size of the operand through
the specific mnemonic - the same is in the codegen.
*/
BEGIN_MNEMONIC(MOVS8, MF_NONE, DU_DU_DU)
BEGIN_OPCODES()
    {OpcodeInfo::ia32,  {0xA4},         {r32,r32,ECX},    DU_DU_DU },
    {OpcodeInfo::em64t, {0xA4},         {r64,r64,RCX},    DU_DU_DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MOVS16, MF_NONE, DU_DU_DU)
BEGIN_OPCODES()
    {OpcodeInfo::ia32,  {Size16, 0xA5}, {r32,r32,ECX},  DU_DU_DU },
    {OpcodeInfo::em64t, {Size16, 0xA5}, {r64,r64,RCX},  DU_DU_DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MOVS32, MF_NONE, DU_DU_DU)
BEGIN_OPCODES()
    {OpcodeInfo::ia32,  {0xA5},         {r32,r32,ECX},  DU_DU_DU },
    {OpcodeInfo::em64t, {0xA5},         {r64,r64,RCX},  DU_DU_DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(MOVS64, MF_NONE, DU_DU_DU)
BEGIN_OPCODES()
    {OpcodeInfo::em64t, {REX_W,0xA5},   {r64,r64,RCX},  DU_DU_DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CMPSB, MF_AFFECTS_FLAGS, DU_DU_DU)
BEGIN_OPCODES()
    {OpcodeInfo::ia32,  {0xA6},         {ESI,EDI,ECX},    DU_DU_DU },
    {OpcodeInfo::em64t, {0xA6},         {RSI,RDI,RCX},    DU_DU_DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CMPSW, MF_AFFECTS_FLAGS, DU_DU_DU)
BEGIN_OPCODES()
    {OpcodeInfo::ia32,  {Size16, 0xA7}, {ESI,EDI,ECX},  DU_DU_DU },
    {OpcodeInfo::em64t, {Size16, 0xA7}, {RSI,RDI,RCX},  DU_DU_DU },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(CMPSD, MF_AFFECTS_FLAGS, DU_DU_DU)
BEGIN_OPCODES()
    {OpcodeInfo::ia32,  {0xA7},         {ESI,EDI,ECX},  DU_DU_DU },
    {OpcodeInfo::em64t, {0xA7},         {RSI,RDI,RCX},  DU_DU_DU },
END_OPCODES()
END_MNEMONIC()


BEGIN_MNEMONIC(WAIT, MF_AFFECTS_FLAGS, N)
BEGIN_OPCODES()
    {OpcodeInfo::all,   {0x9B},         {},       N },
END_OPCODES()
END_MNEMONIC()

//
// ~String operations
//

//
//Note: the instructions below added for the sake of disassembling routine.
// They need to have flags, params and params usage to be defined more precisely.
//
BEGIN_MNEMONIC(LEAVE, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::decoder,   {0xC9},         {},       N },
END_OPCODES()
END_MNEMONIC()

BEGIN_MNEMONIC(ENTER, MF_NONE, N)
BEGIN_OPCODES()
    {OpcodeInfo::decoder,   {0xC8, iw, ib},           {imm16, imm8},  N },
END_OPCODES()
END_MNEMONIC()

};      // ~masterEncodingTable[]

ENCODER_NAMESPACE_END

//#include <algorithm>

ENCODER_NAMESPACE_START

static bool mnemonic_info_comparator(const MnemonicInfo& one,
                                     const MnemonicInfo& two)
{
    return one.mn < two.mn;
}


static int compareMnemonicInfo(const void* info1, const void* info2)
{
    Mnemonic id1, id2;

    id1 = ((const MnemonicInfo*) info1)->mn;
    id2 = ((const MnemonicInfo*) info2)->mn;
    if (id1 < id2)
        return -1;
    if (id1 > id2)
        return 1;
    return 0;
}

int EncoderBase::buildTable(void)
{
    // A check: all mnemonics must be covered
    assert(COUNTOF(masterEncodingTable) == Mnemonic_Count);
    // sort out the mnemonics so the list become ordered
#if 0 //Android x86
    std::sort(masterEncodingTable, masterEncodingTable+Mnemonic_Count,
              mnemonic_info_comparator);
#else
    qsort(masterEncodingTable, Mnemonic_Count, sizeof(MnemonicInfo), compareMnemonicInfo);
#endif
    //
    // clear the things
    //
    memset(opcodesHashMap, NOHASH, sizeof(opcodesHashMap));
    memset(opcodes, 0, sizeof(opcodes));
    //
    // and, finally, build it
    for (unsigned i=0; i<Mnemonic_Count; i++) {
        assert((Mnemonic)i == (masterEncodingTable + i)->mn);
        buildMnemonicDesc(masterEncodingTable+i);
    }
    return 0;
}

void EncoderBase::buildMnemonicDesc(const MnemonicInfo * minfo)
{
    MnemonicDesc& mdesc = mnemonics[minfo->mn];
    mdesc.mn = minfo->mn;
    mdesc.flags = minfo->flags;
    mdesc.roles = minfo->roles;
    mdesc.name = minfo->name;

    //
    // fill the used opcodes
    //
    for (unsigned i=0, oindex=0; i<COUNTOF(minfo->opcodes); i++) {

        const OpcodeInfo& oinfo = minfo->opcodes[i];
        OpcodeDesc& odesc = opcodes[minfo->mn][oindex];
        // last opcode ?
        if (oinfo.opcode[0] == OpcodeByteKind_LAST) {
            // mark the opcode 'last', exit
            odesc.opcode_len = 0;
            odesc.last = 1;
            break;
        }
        odesc.last = 0;
#ifdef _EM64T_
        if (oinfo.platf == OpcodeInfo::ia32) { continue; }
        if (oinfo.platf == OpcodeInfo::decoder32) { continue; }
#else
        if (oinfo.platf == OpcodeInfo::em64t) { continue; }
        if (oinfo.platf == OpcodeInfo::decoder64) { continue; }
#endif
        if (oinfo.platf == OpcodeInfo::decoder64 ||
            oinfo.platf == OpcodeInfo::decoder32) {
             odesc.platf = OpcodeInfo::decoder;
        }
        else {
            odesc.platf = (char)oinfo.platf;
        }
        //
        // fill out opcodes
        //
        unsigned j = 0;
        odesc.opcode_len = 0;
        for(; oinfo.opcode[j]; j++) {
            unsigned opcod = oinfo.opcode[j];
            unsigned kind = opcod&OpcodeByteKind_KindMask;
            if (kind == OpcodeByteKind_REX_W) {
                odesc.opcode[odesc.opcode_len++] = (unsigned char)0x48;
                continue;
            }
            else if(kind != 0 && kind != OpcodeByteKind_ZeroOpcodeByte) {
                break;
            }
            unsigned lowByte = (opcod & OpcodeByteKind_OpcodeMask);
            odesc.opcode[odesc.opcode_len++] = (unsigned char)lowByte;
        }
        assert(odesc.opcode_len<5);
        odesc.aux0 = odesc.aux1 = 0;
        if (oinfo.opcode[j] != 0) {
            odesc.aux0 = oinfo.opcode[j];
            assert((odesc.aux0 & OpcodeByteKind_KindMask) != 0);
            ++j;
            if(oinfo.opcode[j] != 0) {
                odesc.aux1 = oinfo.opcode[j];
                assert((odesc.aux1 & OpcodeByteKind_KindMask) != 0);
            }
        }
        else if (oinfo.roles.count>=2) {
            if (((oinfo.opnds[0].kind&OpndKind_Mem) &&
                 (isRegKind(oinfo.opnds[1].kind))) ||
                ((oinfo.opnds[1].kind&OpndKind_Mem) &&
                 (isRegKind(oinfo.opnds[0].kind)))) {
                 // Example: MOVQ xmm1, xmm/m64 has only opcodes
                 // same with SHRD
                 // Adding fake /r
                 odesc.aux0 = _r;
            }
        }
        else if (oinfo.roles.count==1) {
            if (oinfo.opnds[0].kind&OpndKind_Mem) {
                 // Example: SETcc r/m8, adding fake /0
                 odesc.aux0 = _0;
            }
        }
        // check imm
        if (oinfo.roles.count > 0 &&
            (oinfo.opnds[0].kind == OpndKind_Imm ||
            oinfo.opnds[oinfo.roles.count-1].kind == OpndKind_Imm)) {
            // Example: CALL cd, PUSH imm32 - they fit both opnds[0] and
            // opnds[oinfo.roles.count-1].
            // The A3 opcode fits only opnds[0] - it's currently have
            // MOV imm32, EAX. Looks ridiculous, but this is how the
            // moffset is currently implemented. Will need to fix together
            // with other usages of moff.
            // adding fake /cd or fake /id
            unsigned imm_opnd_index =
                oinfo.opnds[0].kind == OpndKind_Imm ? 0 : oinfo.roles.count-1;
            OpndSize sz = oinfo.opnds[imm_opnd_index].size;
            unsigned imm_encode, coff_encode;
            if (sz==OpndSize_8) {imm_encode = ib; coff_encode=cb; }
            else if (sz==OpndSize_16) {imm_encode = iw; coff_encode=cw;}
            else if (sz==OpndSize_32) {imm_encode = id; coff_encode=cd; }
            else if (sz==OpndSize_64) {imm_encode = io; coff_encode=0xCC; }
            else { assert(false); imm_encode=0xCC; coff_encode=0xCC; }
            if (odesc.aux1 == 0) {
                if (odesc.aux0==0) {
                    odesc.aux0 = imm_encode;
                }
                else {
                    if (odesc.aux0 != imm_encode && odesc.aux0 != coff_encode) {
                        odesc.aux1 = imm_encode;
                    }
                }
            }
            else {
                assert(odesc.aux1==imm_encode);
            }

        }

        assert(sizeof(odesc.opnds) == sizeof(oinfo.opnds));
        memcpy(odesc.opnds, oinfo.opnds, sizeof(odesc.opnds));
        odesc.roles = oinfo.roles;
        odesc.first_opnd = 0;
        if (odesc.opnds[0].reg != RegName_Null) {
            ++odesc.first_opnd;
            if (odesc.opnds[1].reg != RegName_Null) {
                ++odesc.first_opnd;
            }
        }

        if (odesc.platf == OpcodeInfo::decoder) {
            // if the opcode is only for decoding info, then do not hash it.
            ++oindex;
            continue;
        }

        //
        // check whether the operand info is a mask (i.e. r_m*).
        // in this case, split the info to have separate entries for 'r'
        // and for 'm'.
        // the good news is that there can be only one such operand.
        //
        int opnd2split = -1;
        for (unsigned k=0; k<oinfo.roles.count; k++) {
            if ((oinfo.opnds[k].kind & OpndKind_Mem) &&
                (OpndKind_Mem != oinfo.opnds[k].kind)) {
                opnd2split = k;
                break;
            }
        };

        if (opnd2split == -1) {
            // not a mask, hash it, store it, continue.
            unsigned short hash = getHash(&oinfo);
            opcodesHashMap[minfo->mn][hash] = (unsigned char)oindex;
            ++oindex;
            continue;
        };

        OpcodeInfo storeItem = oinfo;
        unsigned short hash;

        // remove the memory part of the mask, and store only 'r' part
        storeItem.opnds[opnd2split].kind = (OpndKind)(storeItem.opnds[opnd2split].kind & ~OpndKind_Mem);
        hash = getHash(&storeItem);
        if (opcodesHashMap[minfo->mn][hash] == NOHASH) {
            opcodesHashMap[minfo->mn][hash] = (unsigned char)oindex;
        }
        // else {
        // do not overwrite if there is something there, just check that operands match
        // the reason is that for some instructions there are several possibilities:
        // say 'DEC r' may be encode as either '48+r' or 'FF /1', and I believe
        // the first one is better for 'dec r'.
        // as we're currently processing an opcode with memory part in operand,
        // leave already filled items intact, so if there is 'OP reg' there, this
        // better choice will be left in the table instead of 'OP r_m'
        // }

        // compute hash of memory-based operand, 'm' part in 'r_m'
        storeItem.opnds[opnd2split].kind = OpndKind_Mem;
        hash = getHash(&storeItem);
        // should not happen: for the r_m opcodes, there is a possibility
        // that hash value of 'r' part intersects with 'OP r' value, but it's
        // impossible for 'm' part.
        assert(opcodesHashMap[minfo->mn][hash] == NOHASH);
        opcodesHashMap[minfo->mn][hash] = (unsigned char)oindex;

        ++oindex;
    }
}

ENCODER_NAMESPACE_END
