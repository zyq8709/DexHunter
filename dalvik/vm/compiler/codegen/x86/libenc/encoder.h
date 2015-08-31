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
/**
 * @file
 * @brief Simple interface for generating processor instructions.
 *
 * The interface works for both IA32 and EM64T. By default, only IA32
 * capabilities are presented. To enable EM64T feature, the _EM64T_ macro
 * must be defined (and, of course, a proper library version to be used).
 *
 * The interface is based on the original ia32.h encoder interface,
 * with some simplifications and add-ons - EM64T-specific, SSE and SSE2.
 *
 * The interface mostly intended for existing legacy code like LIL code
 * generator. From the implementation point of view, it's just a wrapper
 * around the EncoderBase functionality.
 */

#ifndef _VM_ENCODER_H_
#define _VM_ENCODER_H_

#include <limits.h>
#include "enc_base.h"
//#include "open/types.h"

#ifdef _EM64T_
// size of general-purpose value on the stack in bytes
#define GR_STACK_SIZE 8
// size of floating-point value on the stack in bytes
#define FR_STACK_SIZE 8

#if defined(WIN32) || defined(_WIN64)
    // maximum number of GP registers for inputs
    const int MAX_GR = 4;
    // maximum number of FP registers for inputs
    const int MAX_FR = 4;
    // WIN64 reserves 4 words for shadow space
    const int SHADOW = 4 * GR_STACK_SIZE;
#else
    // maximum number of GP registers for inputs
    const int MAX_GR = 6;
    // maximum number of FP registers for inputs
    const int MAX_FR = 8;
    // Linux x64 doesn't reserve shadow space
    const int SHADOW = 0;
#endif

#else
// size of general-purpose value on the stack in bytes
#define GR_STACK_SIZE 4
// size of general-purpose value on the stack in bytes
#define FR_STACK_SIZE 8

// maximum number of GP registers for inputs
const int MAX_GR = 0;
// maximum number of FP registers for inputs
const int MAX_FR = 0;
#endif

typedef enum Reg_No {
#ifdef _EM64T_
    rax_reg = 0,rbx_reg,    rcx_reg,    rdx_reg,
    rdi_reg,    rsi_reg,    rsp_reg,    rbp_reg,
    r8_reg,     r9_reg,     r10_reg,    r11_reg,
    r12_reg,    r13_reg,    r14_reg,    r15_reg,
    xmm0_reg,   xmm1_reg,   xmm2_reg,   xmm3_reg,
    xmm4_reg,   xmm5_reg,   xmm6_reg,   xmm7_reg,
    xmm8_reg,   xmm9_reg,   xmm10_reg,  xmm11_reg,
    xmm12_reg,  xmm13_reg,  xmm14_reg,  xmm15_reg,

#else   // !defined(_EM64T_)

    eax_reg = 0,ebx_reg,    ecx_reg,    edx_reg,
    edi_reg,    esi_reg,    esp_reg,    ebp_reg,
    xmm0_reg,   xmm1_reg,   xmm2_reg,   xmm3_reg,
    xmm4_reg,   xmm5_reg,   xmm6_reg,   xmm7_reg,
    fs_reg,
#endif
    /** @brief Total number of registers.*/
    n_reg
} Reg_No;
//
// instruction operand sizes: 8,16,32,64 bits
//
typedef enum Opnd_Size {
    size_8 = 0,
    size_16,
    size_32,
    size_64,
    n_size,
#ifdef _EM64T_
    size_platf = size_64
#else
    size_platf = size_32
#endif
} Opnd_Size;

//
// opcodes for alu instructions
//
typedef enum ALU_Opcode {
    add_opc = 0,or_opc,     adc_opc,    sbb_opc,
    and_opc,    sub_opc,    xor_opc,    cmp_opc,
    n_alu
} ALU_Opcode;

//
// opcodes for shift instructions
//
typedef enum Shift_Opcode {
    shld_opc,   shrd_opc,   shl_opc,    shr_opc,
    sar_opc,    ror_opc, max_shift_opcode=6,     n_shift = 6
} Shift_Opcode;

typedef enum ConditionCode {
    Condition_O     = 0,
    Condition_NO    = 1,
    Condition_B     = 2,
    Condition_NAE   = Condition_B,
    Condition_C     = Condition_B,
    Condition_NB    = 3,
    Condition_AE    = Condition_NB,
    Condition_NC    = Condition_NB,
    Condition_Z     = 4,
    Condition_E     = Condition_Z,
    Condition_NZ    = 5,
    Condition_NE    = Condition_NZ,
    Condition_BE    = 6,
    Condition_NA    = Condition_BE,
    Condition_NBE   = 7,
    Condition_A     = Condition_NBE,

    Condition_S     = 8,
    Condition_NS    = 9,
    Condition_P     = 10,
    Condition_PE    = Condition_P,
    Condition_NP    = 11,
    Condition_PO    = Condition_NP,
    Condition_L     = 12,
    Condition_NGE   = Condition_L,
    Condition_NL    = 13,
    Condition_GE    = Condition_NL,
    Condition_LE    = 14,
    Condition_NG    = Condition_LE,
    Condition_NLE   = 15,
    Condition_G     = Condition_NLE,
    Condition_Count = 16
} ConditionCode;

//
// prefix code
//
typedef enum InstrPrefix {
    no_prefix,
    lock_prefix                     = 0xF0,
    hint_branch_taken_prefix        = 0x2E,
    hint_branch_not_taken_prefix    = 0x3E,
    prefix_repne                    = 0xF2,
    prefix_repnz                    = prefix_repne,
    prefix_repe                     = 0xF3,
    prefix_repz                     = prefix_repe,
    prefix_rep                      = 0xF3,
    prefix_cs                       = 0x2E,
    prefix_ss                       = 0x36,
    prefix_ds                       = 0x3E,
    prefix_es                       = 0x26,
    prefix_fs                       = 0x64,
    prefix_gs                       = 0x65
} InstrPrefix;


//
// an instruction operand
//
class Opnd {

protected:
    enum Tag { SignedImm, UnsignedImm, Reg, Mem, FP, XMM };

    const Tag  tag;

    Opnd(Tag t): tag(t) {}

public:
    void * operator new(size_t, void * mem) {
        return mem;
    }

    void operator delete(void *) {}

    void operator delete(void *, void *) {}

private:
    // disallow copying
    Opnd(const Opnd &): tag(Mem) { assert(false); }
    Opnd& operator=(const Opnd &) { assert(false); return *this; }
};
typedef int I_32;
class Imm_Opnd: public Opnd {

protected:
    union {
#ifdef _EM64T_
        int64           value;
        unsigned char   bytes[8];
#else
        I_32           value;
        unsigned char   bytes[4];
#endif
    };
    Opnd_Size           size;

public:
    Imm_Opnd(I_32 val, bool isSigned = true):
        Opnd(isSigned ? SignedImm : UnsignedImm), value(val), size(size_32) {
        if (isSigned) {
            if (CHAR_MIN <= val && val <= CHAR_MAX) {
                size = size_8;
            } else if (SHRT_MIN <= val && val <= SHRT_MAX) {
                size = size_16;
            }
        } else {
            assert(val >= 0);
            if (val <= UCHAR_MAX) {
                size = size_8;
            } else if (val <= USHRT_MAX) {
                size = size_16;
            }
        }
    }
    Imm_Opnd(const Imm_Opnd& that): Opnd(that.tag), value(that.value), size(that.size) {};

#ifdef _EM64T_
    Imm_Opnd(Opnd_Size sz, int64 val, bool isSigned = true):
        Opnd(isSigned ? SignedImm : UnsignedImm), value(val), size(sz) {
#ifndef NDEBUG
        switch (size) {
        case size_8:
            assert(val == (int64)(I_8)val);
            break;
        case size_16:
            assert(val == (int64)(int16)val);
            break;
        case size_32:
            assert(val == (int64)(I_32)val);
            break;
        case size_64:
            break;
        case n_size:
            assert(false);
            break;
        }
#endif // NDEBUG
    }

    int64 get_value() const { return value; }

#else

    Imm_Opnd(Opnd_Size sz, I_32 val, int isSigned = true):
        Opnd(isSigned ? SignedImm : UnsignedImm), value(val), size(sz) {
#ifndef NDEBUG
        switch (size) {
        case size_8:
            assert((I_32)val == (I_32)(I_8)val);
            break;
        case size_16:
            assert((I_32)val == (I_32)(int16)val);
            break;
        case size_32:
            break;
        case size_64:
        case n_size:
            assert(false);
            break;
        }
#endif // NDEBUG
    }

    I_32 get_value() const { return value; }

#endif
    Opnd_Size get_size() const { return size; }
    bool      is_signed() const { return tag == SignedImm; }
};

class RM_Opnd: public Opnd {

public:
    bool is_reg() const { return tag != SignedImm && tag != UnsignedImm && tag != Mem; }

protected:
    RM_Opnd(Tag t): Opnd(t) {}

private:
    // disallow copying
    RM_Opnd(const RM_Opnd &): Opnd(Reg) { assert(false); }
};

class R_Opnd: public RM_Opnd {

protected:
    Reg_No      _reg_no;

public:
    R_Opnd(Reg_No r): RM_Opnd(Reg), _reg_no(r) {}
    Reg_No  reg_no() const { return _reg_no; }

private:
    // disallow copying
    R_Opnd(const R_Opnd &): RM_Opnd(Reg) { assert(false); }
};

//
// a memory operand with displacement
// Can also serve as a full memory operand with base,index, displacement and scale.
// Use n_reg to specify 'no register', say, for index.
class M_Opnd: public RM_Opnd {

protected:
    Imm_Opnd        m_disp;
    Imm_Opnd        m_scale;
    R_Opnd          m_index;
    R_Opnd          m_base;

public:
    //M_Opnd(Opnd_Size sz): RM_Opnd(Mem, K_M, sz), m_disp(0), m_scale(0), m_index(n_reg), m_base(n_reg) {}
    M_Opnd(I_32 disp):
        RM_Opnd(Mem), m_disp(disp), m_scale(0), m_index(n_reg), m_base(n_reg) {}
    M_Opnd(Reg_No rbase, I_32 rdisp):
        RM_Opnd(Mem), m_disp(rdisp), m_scale(0), m_index(n_reg), m_base(rbase) {}
    M_Opnd(I_32 disp, Reg_No rbase, Reg_No rindex, unsigned scale):
        RM_Opnd(Mem), m_disp(disp), m_scale(scale), m_index(rindex), m_base(rbase) {}
    M_Opnd(const M_Opnd & that) : RM_Opnd(Mem),
        m_disp((int)that.m_disp.get_value()), m_scale((int)that.m_scale.get_value()),
        m_index(that.m_index.reg_no()), m_base(that.m_base.reg_no())
        {}
    //
    inline const R_Opnd & base(void) const { return m_base; }
    inline const R_Opnd & index(void) const { return m_index; }
    inline const Imm_Opnd & scale(void) const { return m_scale; }
    inline const Imm_Opnd & disp(void) const { return m_disp; }
};

//
//  a memory operand with base register and displacement
//
class M_Base_Opnd: public M_Opnd {

public:
    M_Base_Opnd(Reg_No base, I_32 disp) : M_Opnd(disp, base, n_reg, 0) {}

private:
    // disallow copying - but it leads to ICC errors #734 in encoder.inl
    // M_Base_Opnd(const M_Base_Opnd &): M_Opnd(0) { assert(false); }
};

//
//  a memory operand with base register, scaled index register
//  and displacement.
//
class M_Index_Opnd : public M_Opnd {

public:
    M_Index_Opnd(Reg_No base, Reg_No index, I_32 disp, unsigned scale):
        M_Opnd(disp, base, index, scale) {}

private:
    // disallow copying - but it leads to ICC errors #734 in encoder.inl
    // M_Index_Opnd(const M_Index_Opnd &): M_Opnd(0) { assert(false); }
};

class XMM_Opnd : public Opnd {

protected:
    unsigned        m_idx;

public:
    XMM_Opnd(unsigned _idx): Opnd(XMM), m_idx(_idx) {};
    unsigned get_idx( void ) const { return m_idx; };

private:
    // disallow copying
    XMM_Opnd(const XMM_Opnd &): Opnd(XMM) { assert(false); }
};

//
// operand structures for ia32 registers
//
#ifdef _EM64T_

extern R_Opnd rax_opnd;
extern R_Opnd rcx_opnd;
extern R_Opnd rdx_opnd;
extern R_Opnd rbx_opnd;
extern R_Opnd rdi_opnd;
extern R_Opnd rsi_opnd;
extern R_Opnd rsp_opnd;
extern R_Opnd rbp_opnd;

extern R_Opnd r8_opnd;
extern R_Opnd r9_opnd;
extern R_Opnd r10_opnd;
extern R_Opnd r11_opnd;
extern R_Opnd r12_opnd;
extern R_Opnd r13_opnd;
extern R_Opnd r14_opnd;
extern R_Opnd r15_opnd;

extern XMM_Opnd xmm8_opnd;
extern XMM_Opnd xmm9_opnd;
extern XMM_Opnd xmm10_opnd;
extern XMM_Opnd xmm11_opnd;
extern XMM_Opnd xmm12_opnd;
extern XMM_Opnd xmm13_opnd;
extern XMM_Opnd xmm14_opnd;
extern XMM_Opnd xmm15_opnd;
#else

extern R_Opnd eax_opnd;
extern R_Opnd ecx_opnd;
extern R_Opnd edx_opnd;
extern R_Opnd ebx_opnd;
extern R_Opnd esp_opnd;
extern R_Opnd ebp_opnd;
extern R_Opnd esi_opnd;
extern R_Opnd edi_opnd;

#endif // _EM64T_

extern XMM_Opnd xmm0_opnd;
extern XMM_Opnd xmm1_opnd;
extern XMM_Opnd xmm2_opnd;
extern XMM_Opnd xmm3_opnd;
extern XMM_Opnd xmm4_opnd;
extern XMM_Opnd xmm5_opnd;
extern XMM_Opnd xmm6_opnd;
extern XMM_Opnd xmm7_opnd;

#ifdef NO_ENCODER_INLINE
    #define ENCODER_DECLARE_EXPORT
#else
    #define ENCODER_DECLARE_EXPORT inline
    #include "encoder.inl"
#endif

// prefix
ENCODER_DECLARE_EXPORT char * prefix(char * stream, InstrPrefix p);

// stack push and pop instructions
ENCODER_DECLARE_EXPORT char * push(char * stream, const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * push(char * stream, const Imm_Opnd & imm);
ENCODER_DECLARE_EXPORT char * pop(char * stream,  const RM_Opnd & rm, Opnd_Size sz = size_platf);

// cmpxchg or xchg
ENCODER_DECLARE_EXPORT char * cmpxchg(char * stream, const RM_Opnd & rm, const R_Opnd & r, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * xchg(char * stream, const RM_Opnd & rm, const R_Opnd & r, Opnd_Size sz = size_platf);

// inc(rement), dec(rement), not, neg(ate) instructions
ENCODER_DECLARE_EXPORT char * inc(char * stream,  const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * dec(char * stream,  const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * _not(char * stream, const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * neg(char * stream,  const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * nop(char * stream);
ENCODER_DECLARE_EXPORT char * int3(char * stream);

// alu instructions: add, or, adc, sbb, and, sub, xor, cmp
ENCODER_DECLARE_EXPORT char * alu(char * stream, ALU_Opcode opc, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * alu(char * stream, ALU_Opcode opc, const M_Opnd & m, const R_Opnd & r, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * alu(char * stream, ALU_Opcode opc, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz = size_platf);

// test instruction
ENCODER_DECLARE_EXPORT char * test(char * stream, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * test(char * stream, const RM_Opnd & rm, const R_Opnd & r, Opnd_Size sz = size_platf);

// shift instructions: shl, shr, sar, shld, shrd, ror
ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode opc, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode opc, const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode opc, const RM_Opnd & rm, const R_Opnd & r, const Imm_Opnd & imm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode opc, const RM_Opnd & rm, const R_Opnd & r, Opnd_Size sz = size_platf);

// multiply instructions: mul, imul
ENCODER_DECLARE_EXPORT char * mul(char * stream, const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * imul(char * stream, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * imul(char * stream, const R_Opnd & r, const Imm_Opnd & imm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * imul(char * stream, const R_Opnd & r, const RM_Opnd & rm, const Imm_Opnd& imm, Opnd_Size sz = size_platf);

// divide instructions: div, idiv
ENCODER_DECLARE_EXPORT char * idiv(char * stream, const RM_Opnd & rm, Opnd_Size sz = size_platf);

// data movement: mov
ENCODER_DECLARE_EXPORT char * mov(char * stream, const M_Opnd & m,  const R_Opnd & r, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * mov(char * stream, const R_Opnd & r,  const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * mov(char * stream, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz = size_platf);

ENCODER_DECLARE_EXPORT char * movsx( char * stream, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * movzx( char * stream, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz = size_platf);

ENCODER_DECLARE_EXPORT char * movd(char * stream, const RM_Opnd & rm, const XMM_Opnd & xmm);
ENCODER_DECLARE_EXPORT char * movd(char * stream, const XMM_Opnd & xmm, const RM_Opnd & rm);
ENCODER_DECLARE_EXPORT char * movq(char * stream, const RM_Opnd & rm, const XMM_Opnd & xmm);
ENCODER_DECLARE_EXPORT char * movq(char * stream, const XMM_Opnd & xmm, const RM_Opnd & rm);

// sse mov
ENCODER_DECLARE_EXPORT char * sse_mov(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_mov(char * stream, const M_Opnd & mem, const XMM_Opnd & xmm, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_mov(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);

// sse add, sub, mul, div
ENCODER_DECLARE_EXPORT char * sse_add(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_add(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);

ENCODER_DECLARE_EXPORT char * sse_sub(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_sub(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);

ENCODER_DECLARE_EXPORT char * sse_mul(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_mul(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);

ENCODER_DECLARE_EXPORT char * sse_div(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_div(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);

// xor, compare
ENCODER_DECLARE_EXPORT char * sse_xor(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1);

ENCODER_DECLARE_EXPORT char * sse_compare(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_compare(char * stream, const XMM_Opnd & xmm0, const M_Opnd & mem, bool dbl);

// sse conversions
ENCODER_DECLARE_EXPORT char * sse_cvt_si(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_cvtt2si(char * stream, const R_Opnd & reg, const M_Opnd & mem, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_cvtt2si(char * stream, const R_Opnd & reg, const XMM_Opnd & xmm, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_cvt_fp2dq(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_cvt_dq2fp(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl);
ENCODER_DECLARE_EXPORT char * sse_d2s(char * stream, const XMM_Opnd & xmm0, const M_Opnd & mem64);
ENCODER_DECLARE_EXPORT char * sse_d2s(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1);
ENCODER_DECLARE_EXPORT char * sse_s2d(char * stream, const XMM_Opnd & xmm0, const M_Opnd & mem32);
ENCODER_DECLARE_EXPORT char * sse_s2d(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1);

// condition operations
ENCODER_DECLARE_EXPORT char * cmov(char * stream, ConditionCode cc, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * setcc(char * stream, ConditionCode cc, const RM_Opnd & rm8);

// load effective address: lea
ENCODER_DECLARE_EXPORT char * lea(char * stream, const R_Opnd & r, const M_Opnd & m, Opnd_Size sz = size_platf);
ENCODER_DECLARE_EXPORT char * cdq(char * stream);
ENCODER_DECLARE_EXPORT char * wait(char * stream);

// control-flow instructions
ENCODER_DECLARE_EXPORT char * loop(char * stream, const Imm_Opnd & imm);

// jump with 8-bit relative
ENCODER_DECLARE_EXPORT char * jump8(char * stream, const Imm_Opnd & imm);

// jump with 32-bit relative
ENCODER_DECLARE_EXPORT char * jump32(char * stream, const Imm_Opnd & imm);

// register indirect jump
ENCODER_DECLARE_EXPORT char * jump(char * stream, const RM_Opnd & rm, Opnd_Size sz = size_platf);

// jump to target address
ENCODER_DECLARE_EXPORT char *jump(char * stream, char *target);

// jump with displacement
//char * jump(char * stream, I_32 disp);

// conditional branch with 8-bit branch offset
ENCODER_DECLARE_EXPORT char * branch8(char * stream, ConditionCode cc, const Imm_Opnd & imm, InstrPrefix prefix = no_prefix);

// conditional branch with 32-bit branch offset
ENCODER_DECLARE_EXPORT char * branch32(char * stream, ConditionCode cc, const Imm_Opnd & imm, InstrPrefix prefix = no_prefix);

// conditional branch with target label address
//char * branch(char * stream, ConditionCode cc, const char * target, InstrPrefix prefix = no_prefix);

// conditional branch with displacement immediate
ENCODER_DECLARE_EXPORT char * branch(char * stream, ConditionCode cc, I_32 disp, InstrPrefix prefix = no_prefix);

// call with displacement
ENCODER_DECLARE_EXPORT char * call(char * stream, const Imm_Opnd & imm);

// indirect call through register or memory location
ENCODER_DECLARE_EXPORT char * call(char * stream, const RM_Opnd & rm, Opnd_Size sz = size_platf);

// call target address
ENCODER_DECLARE_EXPORT char * call(char * stream, const char * target);

// return instruction
ENCODER_DECLARE_EXPORT char * ret(char * stream);
ENCODER_DECLARE_EXPORT char * ret(char * stream, unsigned short pop);
ENCODER_DECLARE_EXPORT char * ret(char * stream, const Imm_Opnd & imm);

// string operations
ENCODER_DECLARE_EXPORT char * set_d(char * stream, bool set);
ENCODER_DECLARE_EXPORT char * scas(char * stream, unsigned char prefix);
ENCODER_DECLARE_EXPORT char * stos(char * stream, unsigned char prefix);

// floating-point instructions

// st(0) = st(0) fp_op m{32,64}real
//!char * fp_op_mem(char * stream, FP_Opcode opc,const M_Opnd& mem,int is_double);

// st(0) = st(0) fp_op st(i)
//!char *fp_op(char * stream, FP_Opcode opc,unsigned i);

// st(i) = st(i) fp_op st(0)    ; optionally pop stack
//!char * fp_op(char * stream, FP_Opcode opc,unsigned i,unsigned pop_stk);

// compare st(0),st(1) and pop stack twice
//!char * fcompp(char * stream);
ENCODER_DECLARE_EXPORT char * fldcw(char * stream, const M_Opnd & mem);
ENCODER_DECLARE_EXPORT char * fnstcw(char * stream, const M_Opnd & mem);
ENCODER_DECLARE_EXPORT char * fnstsw(char * stream);
//!char * fchs(char * stream);
//!char * frem(char * stream);
//!char * fxch(char * stream,unsigned i);
//!char * fcomip(char * stream, unsigned i);

// load from memory (as fp) into fp register stack
ENCODER_DECLARE_EXPORT char * fld(char * stream, const M_Opnd & m, bool is_double);
//!char *fld80(char * stream,const M_Opnd& mem);

// load from memory (as int) into fp register stack
//!char * fild(char * stream,const M_Opnd& mem,int is_long);

// push st(i) onto fp register stack
//!char * fld(char * stream,unsigned i);

// push the constants 0.0 and 1.0 onto the fp register stack
//!char * fldz(char * stream);
//!char * fld1(char * stream);

// store stack to memory (as int), always popping the stack
ENCODER_DECLARE_EXPORT char * fist(char * stream, const M_Opnd & mem, bool is_long, bool pop_stk);
// store stack to to memory (as fp), optionally popping the stack
ENCODER_DECLARE_EXPORT char * fst(char * stream, const M_Opnd & m, bool is_double, bool pop_stk);
// store ST(0) to ST(i), optionally popping the stack. Takes 1 clock
ENCODER_DECLARE_EXPORT char * fst(char * stream, unsigned i, bool pop_stk);

//!char * pushad(char * stream);
//!char * pushfd(char * stream);
//!char * popad(char * stream);
//!char * popfd(char * stream);

// stack frame allocation instructions: enter & leave
//
//    enter frame_size
//
//    is equivalent to:
//
//    push    ebp
//    mov     ebp,esp
//    sub     esp,frame_size
//
//!char *enter(char * stream,const Imm_Opnd& imm);

// leave
// is equivalent to:
//
// mov        esp,ebp
// pop        ebp
//!char *leave(char * stream);

// sahf  loads SF, ZF, AF, PF, and CF flags from eax
//!char *sahf(char * stream);

// Intrinsic FP math functions

//!char *math_fsin(char * stream);
//!char *math_fcos(char * stream);
//!char *math_fabs(char * stream);
//!char *math_fpatan(char * stream);
ENCODER_DECLARE_EXPORT char * fprem(char * stream);
ENCODER_DECLARE_EXPORT char * fprem1(char * stream);
//!char *math_frndint(char * stream);
//!char *math_fptan(char * stream);

//
// Add 1-7 bytes padding, with as few instructions as possible,
// with no effect on the processor state (e.g., registers, flags)
//
//!char *padding(char * stream, unsigned num);

// prolog and epilog code generation
//- char *prolog(char * stream,unsigned frame_size,unsigned reg_save_mask);
//- char *epilog(char * stream,unsigned reg_save_mask);

//!extern R_Opnd reg_operand_array[];

// fsave and frstor
//!char *fsave(char * stream);
//!char *frstor(char * stream);

// lahf : Load Status Flags into AH Register
//!char *lahf(char * stream);

// mfence : Memory Fence
//!char *mfence(char * stream);

#endif // _VM_ENCODER_H_
