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
#include <stdio.h>
#include <assert.h>
#include <limits.h>

extern const RegName map_of_regno_2_regname[];
extern const OpndSize map_of_EncoderOpndSize_2_RealOpndSize[];
extern const Mnemonic map_of_alu_opcode_2_mnemonic[];
extern const Mnemonic map_of_shift_opcode_2_mnemonic[];

// S_ stands for 'Signed'
extern const Mnemonic S_map_of_condition_code_2_branch_mnemonic[];
// U_ stands for 'Unsigned'
extern const Mnemonic U_map_of_condition_code_2_branch_mnemonic[];

inline static RegName map_reg(Reg_No r) {
    assert(r >= 0 && r <= n_reg);
    return map_of_regno_2_regname[r];
}

inline static OpndSize map_size(Opnd_Size o_size) {
    assert(o_size >= 0 && o_size <= n_size);
    return map_of_EncoderOpndSize_2_RealOpndSize[o_size];
}

inline static Mnemonic map_alu(ALU_Opcode alu) {
    assert(alu >= 0 && alu < n_alu);
    return map_of_alu_opcode_2_mnemonic[alu];
}

inline static Mnemonic map_shift(Shift_Opcode shc) {
    assert(shc >= 0 && shc < n_shift);
    return map_of_shift_opcode_2_mnemonic[shc];
}

inline bool fit8(int64 val) {
    return (CHAR_MIN <= val) && (val <= CHAR_MAX);
}

inline bool fit32(int64 val) {
    return (INT_MIN <= val) && (val <= INT_MAX);
}

inline static void add_r(EncoderBase::Operands & args, const R_Opnd & r, Opnd_Size sz, OpndExt ext = OpndExt_None) {
    RegName reg = map_reg(r.reg_no());
    if (sz != n_size) {
        OpndSize size = map_size(sz);
        if (size != getRegSize(reg)) {
            reg = getAliasReg(reg, size);
        }
    }
    args.add(EncoderBase::Operand(reg, ext));
}

inline static void add_m(EncoderBase::Operands & args, const M_Opnd & m, Opnd_Size sz, OpndExt ext = OpndExt_None) {
        assert(n_size != sz);
        args.add(EncoderBase::Operand(map_size(sz),
            map_reg(m.base().reg_no()), map_reg(m.index().reg_no()),
            (unsigned)m.scale().get_value(), (int)m.disp().get_value(), ext));
}

inline static void add_rm(EncoderBase::Operands & args, const RM_Opnd & rm, Opnd_Size sz, OpndExt ext = OpndExt_None) {
    rm.is_reg() ? add_r(args, (R_Opnd &)rm, sz, ext) : add_m(args, (M_Opnd &)rm, sz, ext);
}

inline static void add_xmm(EncoderBase::Operands & args, const XMM_Opnd & xmm, bool dbl) {
    // Gregory -
    // XMM registers indexes in Reg_No enum are shifted by xmm0_reg, their indexes
    // don't start with 0, so it is necessary to subtract xmm0_reg index from
    // xmm.get_idx() value
    assert(xmm.get_idx() >= xmm0_reg);
    return args.add((RegName)( (dbl ? RegName_XMM0D : RegName_XMM0S) + xmm.get_idx() -
            xmm0_reg));
}

inline static void add_fp(EncoderBase::Operands & args, unsigned i, bool dbl) {
    return args.add((RegName)( (dbl ? RegName_FP0D : RegName_FP0S) + i));
}

inline static void add_imm(EncoderBase::Operands & args, const Imm_Opnd & imm) {
    assert(n_size != imm.get_size());
    args.add(EncoderBase::Operand(map_size(imm.get_size()), imm.get_value(),
        imm.is_signed() ? OpndExt_Signed : OpndExt_Zero));
}

ENCODER_DECLARE_EXPORT char * prefix(char * stream, InstrPrefix p) {
    *stream = (char)p;
    return stream + 1;
}

// stack push and pop instructions
ENCODER_DECLARE_EXPORT char * push(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_PUSH, args);
}

ENCODER_DECLARE_EXPORT char * push(char * stream, const Imm_Opnd & imm) {
    EncoderBase::Operands args;
#ifdef _EM64T_
    add_imm(args, imm);
#else
    // we need this workaround to be compatible with the former ia32 encoder implementation
    add_imm(args, Imm_Opnd(size_32, imm.get_value()));
#endif
    return EncoderBase::encode(stream, Mnemonic_PUSH, args);
}

ENCODER_DECLARE_EXPORT char * pop(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_POP, args);
}

// cmpxchg or xchg
ENCODER_DECLARE_EXPORT char * cmpxchg(char * stream, const RM_Opnd & rm, const R_Opnd & r, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    add_r(args, r, sz);
    RegName implicitReg = getAliasReg(RegName_EAX, map_size(sz));
    args.add(implicitReg);
    return (char*)EncoderBase::encode(stream, Mnemonic_CMPXCHG, args);
}

ENCODER_DECLARE_EXPORT char * xchg(char * stream, const RM_Opnd & rm, const R_Opnd & r, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    add_r(args, r, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_XCHG, args);
}

// inc(rement), dec(rement), not, neg(ate) instructions
ENCODER_DECLARE_EXPORT char * inc(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_INC, args);
}

ENCODER_DECLARE_EXPORT char * dec(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_DEC, args);
}

ENCODER_DECLARE_EXPORT char * _not(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_NOT, args);
}

ENCODER_DECLARE_EXPORT char * neg(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_NEG, args);
}

ENCODER_DECLARE_EXPORT char * nop(char * stream) {
    EncoderBase::Operands args;
    return (char*)EncoderBase::encode(stream, Mnemonic_NOP, args);
}

ENCODER_DECLARE_EXPORT char * int3(char * stream) {
    EncoderBase::Operands args;
    return (char*)EncoderBase::encode(stream, Mnemonic_INT3, args);
}

// alu instructions: add, or, adc, sbb, and, sub, xor, cmp
ENCODER_DECLARE_EXPORT char * alu(char * stream, ALU_Opcode opc, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, map_alu(opc), args);
};

ENCODER_DECLARE_EXPORT char * alu(char * stream, ALU_Opcode opc, const M_Opnd & m, const R_Opnd & r, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, m, sz);
    add_rm(args, r, sz);
    return (char*)EncoderBase::encode(stream, map_alu(opc), args);
}

ENCODER_DECLARE_EXPORT char * alu(char * stream, ALU_Opcode opc, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, r, sz);
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, map_alu(opc), args);
}

// test instruction
ENCODER_DECLARE_EXPORT char * test(char * stream, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    assert(imm.get_size() <= sz);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_TEST, args);
}

ENCODER_DECLARE_EXPORT char * test(char * stream, const RM_Opnd & rm, const R_Opnd & r, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    add_r(args, r, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_TEST, args);
}

// shift instructions: shl, shr, sar, shld, shrd
ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode shc, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, map_shift(shc), args);
}

ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode shc, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    args.add(RegName_CL);
    return (char*)EncoderBase::encode(stream, map_shift(shc), args);
}

ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode shc, const RM_Opnd & rm,
                            const R_Opnd & r, const Imm_Opnd & imm, Opnd_Size sz) {
    EncoderBase::Operands args;
    assert(shc == shld_opc || shc == shrd_opc);
    add_rm(args, rm, sz);
    add_r(args, r, sz);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, map_shift(shc), args);
}

ENCODER_DECLARE_EXPORT char * shift(char * stream, Shift_Opcode shc, const RM_Opnd & rm,
                            const R_Opnd & r, Opnd_Size sz) {
    EncoderBase::Operands args;
    assert(shc == shld_opc || shc == shrd_opc);
    add_rm(args, rm, sz);
    add_r(args, r, sz);
    args.add(RegName_CL);
    return (char*)EncoderBase::encode(stream, map_shift(shc), args);
}

// multiply instructions: mul, imul
ENCODER_DECLARE_EXPORT char * mul(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    args.add(RegName_EDX);
    args.add(RegName_EAX);
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_MUL, args);
}

ENCODER_DECLARE_EXPORT char * imul(char * stream, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, sz);
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_IMUL, args);
}

ENCODER_DECLARE_EXPORT char * imul(char * stream, const R_Opnd & r, const Imm_Opnd & imm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, sz);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_IMUL, args);
}

ENCODER_DECLARE_EXPORT char * imul(char * stream, const R_Opnd & r, const RM_Opnd & rm,
                           const Imm_Opnd & imm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, sz);
    add_rm(args, rm, sz);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_IMUL, args);
}

// divide instructions: div, idiv
ENCODER_DECLARE_EXPORT char * idiv(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
#ifdef _EM64T_
    add_r(args, rdx_opnd, sz);
    add_r(args, rax_opnd, sz);
#else
    add_r(args, edx_opnd, sz);
    add_r(args, eax_opnd, sz);
#endif
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_IDIV, args);
}

// data movement: mov
ENCODER_DECLARE_EXPORT char * mov(char * stream, const M_Opnd & m, const R_Opnd & r, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_m(args, m, sz);
    add_r(args, r, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOV, args);
}

ENCODER_DECLARE_EXPORT char * mov(char * stream, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, sz);
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOV, args);
}

ENCODER_DECLARE_EXPORT char * mov(char * stream, const RM_Opnd & rm, const Imm_Opnd & imm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOV, args);
}

ENCODER_DECLARE_EXPORT char * movd(char * stream, const RM_Opnd & rm, const XMM_Opnd & xmm) {
    EncoderBase::Operands args;
    add_rm(args, rm, size_32);
    add_xmm(args, xmm, false);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOVD, args);
}

ENCODER_DECLARE_EXPORT char * movd(char * stream, const XMM_Opnd & xmm, const RM_Opnd & rm) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, false);
    add_rm(args, rm, size_32);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOVD, args);
}

ENCODER_DECLARE_EXPORT char * movq(char * stream, const RM_Opnd & rm, const XMM_Opnd & xmm) {
    EncoderBase::Operands args;
    add_rm(args, rm, size_64);
    add_xmm(args, xmm, true);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOVQ, args);
}

ENCODER_DECLARE_EXPORT char * movq(char * stream, const XMM_Opnd & xmm, const RM_Opnd & rm) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, true);
    add_rm(args, rm, size_64);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOVQ, args);
}

ENCODER_DECLARE_EXPORT char * movsx(char * stream, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, n_size);
    add_rm(args, rm, sz, OpndExt_Signed);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOVSX, args);
}

ENCODER_DECLARE_EXPORT char * movzx(char * stream, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, n_size);
    // movzx r64, r/m32 is not available on em64t
    // mov r32, r/m32 should zero out upper bytes
    assert(sz <= size_16);
    add_rm(args, rm, sz, OpndExt_Zero);
    return (char*)EncoderBase::encode(stream, Mnemonic_MOVZX, args);
}

// sse mov
ENCODER_DECLARE_EXPORT char * sse_mov(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, dbl);
    add_m(args, mem, dbl ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_MOVSD : Mnemonic_MOVSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_mov(char * stream, const M_Opnd &  mem, const XMM_Opnd & xmm, bool dbl) {
    EncoderBase::Operands args;
    add_m(args, mem, dbl ? size_64 : size_32);
    add_xmm(args, xmm, dbl);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_MOVSD : Mnemonic_MOVSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_mov(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, dbl);
    add_xmm(args, xmm1, dbl);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_MOVSD : Mnemonic_MOVSS, args );
}

// sse add, sub, mul, div
ENCODER_DECLARE_EXPORT char * sse_add(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, dbl);
    add_m(args, mem, dbl ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_ADDSD : Mnemonic_ADDSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_add(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, dbl);
    add_xmm(args, xmm1, dbl);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_ADDSD : Mnemonic_ADDSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_sub(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, dbl);
    add_m(args, mem, dbl ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_SUBSD : Mnemonic_SUBSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_sub(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, dbl);
    add_xmm(args, xmm1, dbl);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_SUBSD : Mnemonic_SUBSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_mul( char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, dbl);
    add_m(args, mem, dbl ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_MULSD : Mnemonic_MULSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_mul(char * stream, const XMM_Opnd& xmm0, const XMM_Opnd& xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args,  xmm0, dbl);
    add_xmm(args,  xmm1, dbl);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_MULSD : Mnemonic_MULSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_div(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, dbl);
    add_m(args, mem, dbl ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_DIVSD : Mnemonic_DIVSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_div(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, dbl);
    add_xmm(args, xmm1, dbl);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_DIVSD : Mnemonic_DIVSS, args);
}

ENCODER_DECLARE_EXPORT char * sse_xor(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, true);
    add_xmm(args, xmm1, true);
    return (char*)EncoderBase::encode(stream, Mnemonic_PXOR, args);
}

ENCODER_DECLARE_EXPORT char * sse_compare(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, true);
    add_xmm(args, xmm1, true);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_COMISD : Mnemonic_COMISS, args);
}

ENCODER_DECLARE_EXPORT char * sse_compare(char * stream, const XMM_Opnd & xmm0, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, dbl);
    add_m(args, mem, dbl ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_COMISD : Mnemonic_COMISS, args);
}

// sse conversions
ENCODER_DECLARE_EXPORT char * sse_cvt_si(char * stream, const XMM_Opnd & xmm, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm, dbl);
    add_m(args, mem, size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_CVTSI2SD : Mnemonic_CVTSI2SS, args);
}

ENCODER_DECLARE_EXPORT char * sse_cvtt2si(char * stream, const R_Opnd & reg, const M_Opnd & mem, bool dbl) {
    EncoderBase::Operands args;
    add_rm(args, reg, size_32);
    add_m(args, mem, dbl ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_CVTTSD2SI : Mnemonic_CVTTSS2SI, args);
}

ENCODER_DECLARE_EXPORT char * sse_cvtt2si(char * stream, const R_Opnd & reg, const XMM_Opnd & xmm, bool dbl) {
    EncoderBase::Operands args;
    add_rm(args, reg, size_32);
    add_xmm(args, xmm, dbl);
    return (char*)EncoderBase::encode(stream, dbl ? Mnemonic_CVTTSD2SI : Mnemonic_CVTTSS2SI, args);
}

ENCODER_DECLARE_EXPORT char * sse_cvt_fp2dq(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, dbl);
    add_xmm(args, xmm1, dbl);
    return (char*)EncoderBase::encode(stream, dbl ?  Mnemonic_CVTTPD2DQ : Mnemonic_CVTTPS2DQ, args);
}

ENCODER_DECLARE_EXPORT char * sse_cvt_dq2fp(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1, bool dbl) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, dbl);
    add_xmm(args, xmm1, dbl);
    return (char*)EncoderBase::encode(stream, dbl ?  Mnemonic_CVTDQ2PD : Mnemonic_CVTDQ2PS, args);
}

ENCODER_DECLARE_EXPORT char * sse_d2s(char * stream, const XMM_Opnd & xmm0, const M_Opnd & mem64) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, false);
    add_m(args, mem64, size_64);
    return (char*)EncoderBase::encode(stream, Mnemonic_CVTSD2SS, args);
}

ENCODER_DECLARE_EXPORT char * sse_d2s(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, false);
    add_xmm(args, xmm1, true);
    return (char*)EncoderBase::encode(stream, Mnemonic_CVTSD2SS, args);
}

ENCODER_DECLARE_EXPORT char * sse_s2d(char * stream, const XMM_Opnd & xmm0, const M_Opnd & mem32) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, true);
    add_m(args, mem32, size_32);
    return (char*)EncoderBase::encode(stream, Mnemonic_CVTSS2SD, args);
}

ENCODER_DECLARE_EXPORT char * sse_s2d(char * stream, const XMM_Opnd & xmm0, const XMM_Opnd & xmm1) {
    EncoderBase::Operands args;
    add_xmm(args, xmm0, true);
    add_xmm(args, xmm1, false);
    return (char*)EncoderBase::encode(stream, Mnemonic_CVTSS2SD, args);
}

// condition operations
ENCODER_DECLARE_EXPORT char *cmov(char * stream, ConditionCode cc, const R_Opnd & r, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, sz);
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, (Mnemonic)(Mnemonic_CMOVcc + cc), args);
}

ENCODER_DECLARE_EXPORT char * setcc(char * stream, ConditionCode cc, const RM_Opnd & rm8) {
    EncoderBase::Operands args;
    add_rm(args, rm8, size_8);
    return (char*)EncoderBase::encode(stream, (Mnemonic)(Mnemonic_SETcc + cc), args);
}

// load effective address: lea
ENCODER_DECLARE_EXPORT char * lea(char * stream, const R_Opnd & r, const M_Opnd & m, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_r(args, r, sz);
    add_m(args, m, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_LEA, args);
}

ENCODER_DECLARE_EXPORT char * cdq(char * stream) {
    EncoderBase::Operands args;
    args.add(RegName_EDX);
    args.add(RegName_EAX);
    return (char*)EncoderBase::encode(stream, Mnemonic_CDQ, args);
}

ENCODER_DECLARE_EXPORT char * wait(char * stream) {
    return (char*)EncoderBase::encode(stream, Mnemonic_WAIT, EncoderBase::Operands());
}

// control-flow instructions

// loop
ENCODER_DECLARE_EXPORT char * loop(char * stream, const Imm_Opnd & imm) {
    EncoderBase::Operands args;
    assert(imm.get_size() == size_8);
    args.add(RegName_ECX);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_LOOP, args);
}

// jump
ENCODER_DECLARE_EXPORT char * jump8(char * stream, const Imm_Opnd & imm) {
    EncoderBase::Operands args;
    assert(imm.get_size() == size_8);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_JMP, args);
}

ENCODER_DECLARE_EXPORT char * jump32(char * stream, const Imm_Opnd & imm) {
    EncoderBase::Operands args;
    assert(imm.get_size() == size_32);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_JMP, args);
}

ENCODER_DECLARE_EXPORT char * jump(char * stream, const RM_Opnd & rm, Opnd_Size sz) {
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_JMP, args);
}

/**
 * @note On EM64T: if target lies beyond 2G (does not fit into 32 bit
 *       offset) then generates indirect jump using RAX (whose content is
 *       destroyed).
 */
ENCODER_DECLARE_EXPORT char * jump(char * stream, char * target) {
#ifdef _EM64T_
    int64 offset = target - stream;
    // sub 2 bytes for the short version
    offset -= 2;
    if (fit8(offset)) {
        // use 8-bit signed relative form
        return jump8(stream, Imm_Opnd(size_8, offset));
    } else if (fit32(offset)) {
        // sub 5 (3 + 2)bytes for the long version
        offset -= 3;
        // use 32-bit signed relative form
        return jump32(stream, Imm_Opnd(size_32, offset));
    }
    // need to use absolute indirect jump
    stream = mov(stream, rax_opnd, Imm_Opnd(size_64, (int64)target), size_64);
    return jump(stream, rax_opnd, size_64);
#else
    I_32 offset = target - stream;
    // sub 2 bytes for the short version
    offset -= 2;
    if (fit8(offset)) {
        // use 8-bit signed relative form
        return jump8(stream, Imm_Opnd(size_8, offset));
    }
    // sub 5 (3 + 2) bytes for the long version
    offset -= 3;
    // use 32-bit signed relative form
    return jump32(stream, Imm_Opnd(size_32, offset));
#endif
}

// branch
ENCODER_DECLARE_EXPORT char * branch8(char * stream, ConditionCode cond,
                                      const Imm_Opnd & imm,
                                      InstrPrefix pref)
{
    if (pref != no_prefix) {
        assert(pref == hint_branch_taken_prefix || pref == hint_branch_taken_prefix);
        stream = prefix(stream, pref);
    }
    Mnemonic m = (Mnemonic)(Mnemonic_Jcc + cond);
    EncoderBase::Operands args;
    assert(imm.get_size() == size_8);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, m, args);
}

ENCODER_DECLARE_EXPORT char * branch32(char * stream, ConditionCode cond,
                                       const Imm_Opnd & imm,
                                       InstrPrefix pref)
{
    if (pref != no_prefix) {
        assert(pref == hint_branch_taken_prefix || pref == hint_branch_taken_prefix);
        stream = prefix(stream, pref);
    }
    Mnemonic m = (Mnemonic)(Mnemonic_Jcc + cond);
    EncoderBase::Operands args;
    assert(imm.get_size() == size_32);
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, m, args);
}

/*
ENCODER_DECLARE_EXPORT char * branch(char * stream, ConditionCode cc, const char * target, InstrPrefix prefix) {
// sub 2 bytes for the short version
int64 offset = stream-target-2;
if( fit8(offset) ) {
return branch8(stream, cc, Imm_Opnd(size_8, (char)offset), is_signed);
}
return branch32(stream, cc, Imm_Opnd(size_32, (int)offset), is_signed);
}
*/

// call
ENCODER_DECLARE_EXPORT char * call(char * stream, const Imm_Opnd & imm)
{
    EncoderBase::Operands args;
    add_imm(args, imm);
    return (char*)EncoderBase::encode(stream, Mnemonic_CALL, args);
}

ENCODER_DECLARE_EXPORT char * call(char * stream, const RM_Opnd & rm,
                                   Opnd_Size sz)
{
    EncoderBase::Operands args;
    add_rm(args, rm, sz);
    return (char*)EncoderBase::encode(stream, Mnemonic_CALL, args);
}

/**
* @note On EM64T: if target lies beyond 2G (does not fit into 32 bit
*       offset) then generates indirect jump using RAX (whose content is
*       destroyed).
*/
ENCODER_DECLARE_EXPORT char * call(char * stream, const char * target)
{
#ifdef _EM64T_
    int64 offset = target - stream;
    if (fit32(offset)) {
        offset -= 5; // sub 5 bytes for this instruction
        Imm_Opnd imm(size_32, offset);
        return call(stream, imm);
    }
    // need to use absolute indirect call
    stream = mov(stream, rax_opnd, Imm_Opnd(size_64, (int64)target), size_64);
    return call(stream, rax_opnd, size_64);
#else
    I_32 offset = target - stream;
    offset -= 5; // sub 5 bytes for this instruction
    Imm_Opnd imm(size_32, offset);
    return call(stream, imm);
#endif
}

// return instruction
ENCODER_DECLARE_EXPORT char * ret(char * stream)
{
    EncoderBase::Operands args;
    return (char*)EncoderBase::encode(stream, Mnemonic_RET, args);
}

ENCODER_DECLARE_EXPORT char * ret(char * stream, const Imm_Opnd & imm)
{
    EncoderBase::Operands args;
    // TheManual says imm can be 16-bit only
    //assert(imm.get_size() <= size_16);
    args.add(EncoderBase::Operand(map_size(size_16), imm.get_value()));
    return (char*)EncoderBase::encode(stream, Mnemonic_RET, args);
}

ENCODER_DECLARE_EXPORT char * ret(char * stream, unsigned short pop)
{
    // TheManual says it can only be imm16
    EncoderBase::Operands args(EncoderBase::Operand(OpndSize_16, pop, OpndExt_Zero));
    return (char*)EncoderBase::encode(stream, Mnemonic_RET, args);
}

// floating-point instructions
ENCODER_DECLARE_EXPORT char * fld(char * stream, const M_Opnd & m,
                                  bool is_double) {
    EncoderBase::Operands args;
    // a fake FP register as operand
    add_fp(args, 0, is_double);
    add_m(args, m, is_double ? size_64 : size_32);
    return (char*)EncoderBase::encode(stream, Mnemonic_FLD, args);
}

ENCODER_DECLARE_EXPORT char * fist(char * stream, const M_Opnd & mem,
                                   bool is_long, bool pop_stk)
{
    EncoderBase::Operands args;
    if (pop_stk) {
        add_m(args, mem, is_long ? size_64 : size_32);
        // a fake FP register as operand
        add_fp(args, 0, is_long);
        return (char*)EncoderBase::encode(stream,  Mnemonic_FISTP, args);
    }
    // only 32-bit operands are supported
    assert(is_long == false);
    add_m(args, mem, size_32);
    add_fp(args, 0, false);
    return (char*)EncoderBase::encode(stream,  Mnemonic_FIST, args);
}

ENCODER_DECLARE_EXPORT char * fst(char * stream, const M_Opnd & m,
                                  bool is_double, bool pop_stk)
{
    EncoderBase::Operands args;
    add_m(args, m, is_double ? size_64 : size_32);
    // a fake FP register as operand
    add_fp(args, 0, is_double);
    return (char*)EncoderBase::encode(stream,
                                    pop_stk ? Mnemonic_FSTP : Mnemonic_FST,
                                    args);
}

ENCODER_DECLARE_EXPORT char * fst(char * stream, unsigned i, bool pop_stk)
{
    EncoderBase::Operands args;
    add_fp(args, i, true);
    return (char*)EncoderBase::encode(stream,
                                    pop_stk ? Mnemonic_FSTP : Mnemonic_FST,
                                    args);
}

ENCODER_DECLARE_EXPORT char * fldcw(char * stream, const M_Opnd & mem) {
    EncoderBase::Operands args;
    add_m(args, mem, size_16);
    return (char*)EncoderBase::encode(stream, Mnemonic_FLDCW, args);
}

ENCODER_DECLARE_EXPORT char * fnstcw(char * stream, const M_Opnd & mem) {
    EncoderBase::Operands args;
    add_m(args, mem, size_16);
    return (char*)EncoderBase::encode(stream, Mnemonic_FNSTCW, args);
}

ENCODER_DECLARE_EXPORT char * fnstsw(char * stream)
{
    return (char*)EncoderBase::encode(stream, Mnemonic_FNSTCW,
                                      EncoderBase::Operands());
}

// string operations
ENCODER_DECLARE_EXPORT char * set_d(char * stream, bool set) {
    EncoderBase::Operands args;
    return (char*)EncoderBase::encode(stream,
                                      set ? Mnemonic_STD : Mnemonic_CLD,
                                      args);
}

ENCODER_DECLARE_EXPORT char * scas(char * stream, unsigned char prefix)
{
	EncoderBase::Operands args;
    if (prefix != no_prefix) {
        assert(prefix == prefix_repnz || prefix == prefix_repz);
        *stream = prefix;
        ++stream;
    }
    return (char*)EncoderBase::encode(stream, Mnemonic_SCAS, args);
}

ENCODER_DECLARE_EXPORT char * stos(char * stream, unsigned char prefix)
{
    if (prefix != no_prefix) {
        assert(prefix == prefix_rep);
        *stream = prefix;
        ++stream;
    }

	EncoderBase::Operands args;
	return (char*)EncoderBase::encode(stream, Mnemonic_STOS, args);
}

// Intrinsic FP math functions

ENCODER_DECLARE_EXPORT char * fprem(char * stream) {
    return (char*)EncoderBase::encode(stream, Mnemonic_FPREM,
                                      EncoderBase::Operands());
}

ENCODER_DECLARE_EXPORT char * fprem1(char * stream) {
    return (char*)EncoderBase::encode(stream, Mnemonic_FPREM1,
                                      EncoderBase::Operands());
}
