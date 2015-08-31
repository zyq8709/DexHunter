/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "assembler_x86.h"

#include "base/casts.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "memory_region.h"
#include "thread.h"

namespace art {
namespace x86 {

class DirectCallRelocation : public AssemblerFixup {
 public:
  void Process(const MemoryRegion& region, int position) {
    // Direct calls are relative to the following instruction on x86.
    int32_t pointer = region.Load<int32_t>(position);
    int32_t start = reinterpret_cast<int32_t>(region.start());
    int32_t delta = start + position + sizeof(int32_t);
    region.Store<int32_t>(position, pointer - delta);
  }
};

std::ostream& operator<<(std::ostream& os, const XmmRegister& reg) {
  return os << "XMM" << static_cast<int>(reg);
}

std::ostream& operator<<(std::ostream& os, const X87Register& reg) {
  return os << "ST" << static_cast<int>(reg);
}

void X86Assembler::call(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xFF);
  EmitRegisterOperand(2, reg);
}


void X86Assembler::call(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xFF);
  EmitOperand(2, address);
}


void X86Assembler::call(Label* label) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xE8);
  static const int kSize = 5;
  EmitLabel(label, kSize);
}


void X86Assembler::pushl(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x50 + reg);
}


void X86Assembler::pushl(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xFF);
  EmitOperand(6, address);
}


void X86Assembler::pushl(const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (imm.is_int8()) {
    EmitUint8(0x6A);
    EmitUint8(imm.value() & 0xFF);
  } else {
    EmitUint8(0x68);
    EmitImmediate(imm);
  }
}


void X86Assembler::popl(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x58 + reg);
}


void X86Assembler::popl(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x8F);
  EmitOperand(0, address);
}


void X86Assembler::movl(Register dst, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xB8 + dst);
  EmitImmediate(imm);
}


void X86Assembler::movl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x89);
  EmitRegisterOperand(src, dst);
}


void X86Assembler::movl(Register dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x8B);
  EmitOperand(dst, src);
}


void X86Assembler::movl(const Address& dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x89);
  EmitOperand(src, dst);
}


void X86Assembler::movl(const Address& dst, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC7);
  EmitOperand(0, dst);
  EmitImmediate(imm);
}

void X86Assembler::movl(const Address& dst, Label* lbl) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC7);
  EmitOperand(0, dst);
  EmitLabel(lbl, dst.length_ + 5);
}

void X86Assembler::movzxb(Register dst, ByteRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xB6);
  EmitRegisterOperand(dst, src);
}


void X86Assembler::movzxb(Register dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xB6);
  EmitOperand(dst, src);
}


void X86Assembler::movsxb(Register dst, ByteRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xBE);
  EmitRegisterOperand(dst, src);
}


void X86Assembler::movsxb(Register dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xBE);
  EmitOperand(dst, src);
}


void X86Assembler::movb(Register /*dst*/, const Address& /*src*/) {
  LOG(FATAL) << "Use movzxb or movsxb instead.";
}


void X86Assembler::movb(const Address& dst, ByteRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x88);
  EmitOperand(src, dst);
}


void X86Assembler::movb(const Address& dst, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC6);
  EmitOperand(EAX, dst);
  CHECK(imm.is_int8());
  EmitUint8(imm.value() & 0xFF);
}


void X86Assembler::movzxw(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xB7);
  EmitRegisterOperand(dst, src);
}


void X86Assembler::movzxw(Register dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xB7);
  EmitOperand(dst, src);
}


void X86Assembler::movsxw(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xBF);
  EmitRegisterOperand(dst, src);
}


void X86Assembler::movsxw(Register dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xBF);
  EmitOperand(dst, src);
}


void X86Assembler::movw(Register /*dst*/, const Address& /*src*/) {
  LOG(FATAL) << "Use movzxw or movsxw instead.";
}


void X86Assembler::movw(const Address& dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandSizeOverride();
  EmitUint8(0x89);
  EmitOperand(src, dst);
}


void X86Assembler::leal(Register dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x8D);
  EmitOperand(dst, src);
}


void X86Assembler::cmovl(Condition condition, Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0x40 + condition);
  EmitRegisterOperand(dst, src);
}


void X86Assembler::setb(Condition condition, Register dst) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0x90 + condition);
  EmitOperand(0, Operand(dst));
}


void X86Assembler::movss(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x10);
  EmitOperand(dst, src);
}


void X86Assembler::movss(const Address& dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x11);
  EmitOperand(src, dst);
}


void X86Assembler::movss(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x11);
  EmitXmmRegisterOperand(src, dst);
}


void X86Assembler::movd(XmmRegister dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x66);
  EmitUint8(0x0F);
  EmitUint8(0x6E);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::movd(Register dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x66);
  EmitUint8(0x0F);
  EmitUint8(0x7E);
  EmitOperand(src, Operand(dst));
}


void X86Assembler::addss(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x58);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::addss(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x58);
  EmitOperand(dst, src);
}


void X86Assembler::subss(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x5C);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::subss(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x5C);
  EmitOperand(dst, src);
}


void X86Assembler::mulss(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x59);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::mulss(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x59);
  EmitOperand(dst, src);
}


void X86Assembler::divss(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x5E);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::divss(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x5E);
  EmitOperand(dst, src);
}


void X86Assembler::flds(const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitOperand(0, src);
}


void X86Assembler::fstps(const Address& dst) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitOperand(3, dst);
}


void X86Assembler::movsd(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x10);
  EmitOperand(dst, src);
}


void X86Assembler::movsd(const Address& dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x11);
  EmitOperand(src, dst);
}


void X86Assembler::movsd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x11);
  EmitXmmRegisterOperand(src, dst);
}


void X86Assembler::addsd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x58);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::addsd(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x58);
  EmitOperand(dst, src);
}


void X86Assembler::subsd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x5C);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::subsd(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x5C);
  EmitOperand(dst, src);
}


void X86Assembler::mulsd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x59);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::mulsd(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x59);
  EmitOperand(dst, src);
}


void X86Assembler::divsd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x5E);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::divsd(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x5E);
  EmitOperand(dst, src);
}


void X86Assembler::cvtsi2ss(XmmRegister dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x2A);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::cvtsi2sd(XmmRegister dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x2A);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::cvtss2si(Register dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x2D);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::cvtss2sd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x5A);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::cvtsd2si(Register dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x2D);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::cvttss2si(Register dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x2C);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::cvttsd2si(Register dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x2C);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::cvtsd2ss(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x5A);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::cvtdq2pd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0xE6);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::comiss(XmmRegister a, XmmRegister b) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0x2F);
  EmitXmmRegisterOperand(a, b);
}


void X86Assembler::comisd(XmmRegister a, XmmRegister b) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x66);
  EmitUint8(0x0F);
  EmitUint8(0x2F);
  EmitXmmRegisterOperand(a, b);
}


void X86Assembler::sqrtsd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF2);
  EmitUint8(0x0F);
  EmitUint8(0x51);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::sqrtss(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF3);
  EmitUint8(0x0F);
  EmitUint8(0x51);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::xorpd(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x66);
  EmitUint8(0x0F);
  EmitUint8(0x57);
  EmitOperand(dst, src);
}


void X86Assembler::xorpd(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x66);
  EmitUint8(0x0F);
  EmitUint8(0x57);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::xorps(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0x57);
  EmitOperand(dst, src);
}


void X86Assembler::xorps(XmmRegister dst, XmmRegister src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0x57);
  EmitXmmRegisterOperand(dst, src);
}


void X86Assembler::andpd(XmmRegister dst, const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x66);
  EmitUint8(0x0F);
  EmitUint8(0x54);
  EmitOperand(dst, src);
}


void X86Assembler::fldl(const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDD);
  EmitOperand(0, src);
}


void X86Assembler::fstpl(const Address& dst) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDD);
  EmitOperand(3, dst);
}


void X86Assembler::fnstcw(const Address& dst) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitOperand(7, dst);
}


void X86Assembler::fldcw(const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitOperand(5, src);
}


void X86Assembler::fistpl(const Address& dst) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDF);
  EmitOperand(7, dst);
}


void X86Assembler::fistps(const Address& dst) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDB);
  EmitOperand(3, dst);
}


void X86Assembler::fildl(const Address& src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDF);
  EmitOperand(5, src);
}


void X86Assembler::fincstp() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitUint8(0xF7);
}


void X86Assembler::ffree(const Immediate& index) {
  CHECK_LT(index.value(), 7);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDD);
  EmitUint8(0xC0 + index.value());
}


void X86Assembler::fsin() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitUint8(0xFE);
}


void X86Assembler::fcos() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitUint8(0xFF);
}


void X86Assembler::fptan() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xD9);
  EmitUint8(0xF2);
}


void X86Assembler::xchgl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x87);
  EmitRegisterOperand(dst, src);
}

void X86Assembler::xchgl(Register reg, const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x87);
  EmitOperand(reg, address);
}


void X86Assembler::cmpl(Register reg, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(7, Operand(reg), imm);
}


void X86Assembler::cmpl(Register reg0, Register reg1) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x3B);
  EmitOperand(reg0, Operand(reg1));
}


void X86Assembler::cmpl(Register reg, const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x3B);
  EmitOperand(reg, address);
}


void X86Assembler::addl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x03);
  EmitRegisterOperand(dst, src);
}


void X86Assembler::addl(Register reg, const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x03);
  EmitOperand(reg, address);
}


void X86Assembler::cmpl(const Address& address, Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x39);
  EmitOperand(reg, address);
}


void X86Assembler::cmpl(const Address& address, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(7, address, imm);
}


void X86Assembler::testl(Register reg1, Register reg2) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x85);
  EmitRegisterOperand(reg1, reg2);
}


void X86Assembler::testl(Register reg, const Immediate& immediate) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  // For registers that have a byte variant (EAX, EBX, ECX, and EDX)
  // we only test the byte register to keep the encoding short.
  if (immediate.is_uint8() && reg < 4) {
    // Use zero-extended 8-bit immediate.
    if (reg == EAX) {
      EmitUint8(0xA8);
    } else {
      EmitUint8(0xF6);
      EmitUint8(0xC0 + reg);
    }
    EmitUint8(immediate.value() & 0xFF);
  } else if (reg == EAX) {
    // Use short form if the destination is EAX.
    EmitUint8(0xA9);
    EmitImmediate(immediate);
  } else {
    EmitUint8(0xF7);
    EmitOperand(0, Operand(reg));
    EmitImmediate(immediate);
  }
}


void X86Assembler::andl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x23);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::andl(Register dst, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(4, Operand(dst), imm);
}


void X86Assembler::orl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0B);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::orl(Register dst, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(1, Operand(dst), imm);
}


void X86Assembler::xorl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x33);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::addl(Register reg, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(0, Operand(reg), imm);
}


void X86Assembler::addl(const Address& address, Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x01);
  EmitOperand(reg, address);
}


void X86Assembler::addl(const Address& address, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(0, address, imm);
}


void X86Assembler::adcl(Register reg, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(2, Operand(reg), imm);
}


void X86Assembler::adcl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x13);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::adcl(Register dst, const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x13);
  EmitOperand(dst, address);
}


void X86Assembler::subl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x2B);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::subl(Register reg, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(5, Operand(reg), imm);
}


void X86Assembler::subl(Register reg, const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x2B);
  EmitOperand(reg, address);
}


void X86Assembler::cdq() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x99);
}


void X86Assembler::idivl(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF7);
  EmitUint8(0xF8 | reg);
}


void X86Assembler::imull(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xAF);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::imull(Register reg, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x69);
  EmitOperand(reg, Operand(reg));
  EmitImmediate(imm);
}


void X86Assembler::imull(Register reg, const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xAF);
  EmitOperand(reg, address);
}


void X86Assembler::imull(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF7);
  EmitOperand(5, Operand(reg));
}


void X86Assembler::imull(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF7);
  EmitOperand(5, address);
}


void X86Assembler::mull(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF7);
  EmitOperand(4, Operand(reg));
}


void X86Assembler::mull(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF7);
  EmitOperand(4, address);
}


void X86Assembler::sbbl(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x1B);
  EmitOperand(dst, Operand(src));
}


void X86Assembler::sbbl(Register reg, const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitComplex(3, Operand(reg), imm);
}


void X86Assembler::sbbl(Register dst, const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x1B);
  EmitOperand(dst, address);
}


void X86Assembler::incl(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x40 + reg);
}


void X86Assembler::incl(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xFF);
  EmitOperand(0, address);
}


void X86Assembler::decl(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x48 + reg);
}


void X86Assembler::decl(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xFF);
  EmitOperand(1, address);
}


void X86Assembler::shll(Register reg, const Immediate& imm) {
  EmitGenericShift(4, reg, imm);
}


void X86Assembler::shll(Register operand, Register shifter) {
  EmitGenericShift(4, operand, shifter);
}


void X86Assembler::shrl(Register reg, const Immediate& imm) {
  EmitGenericShift(5, reg, imm);
}


void X86Assembler::shrl(Register operand, Register shifter) {
  EmitGenericShift(5, operand, shifter);
}


void X86Assembler::sarl(Register reg, const Immediate& imm) {
  EmitGenericShift(7, reg, imm);
}


void X86Assembler::sarl(Register operand, Register shifter) {
  EmitGenericShift(7, operand, shifter);
}


void X86Assembler::shld(Register dst, Register src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xA5);
  EmitRegisterOperand(src, dst);
}


void X86Assembler::negl(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF7);
  EmitOperand(3, Operand(reg));
}


void X86Assembler::notl(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF7);
  EmitUint8(0xD0 | reg);
}


void X86Assembler::enter(const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC8);
  CHECK(imm.is_uint16());
  EmitUint8(imm.value() & 0xFF);
  EmitUint8((imm.value() >> 8) & 0xFF);
  EmitUint8(0x00);
}


void X86Assembler::leave() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC9);
}


void X86Assembler::ret() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC3);
}


void X86Assembler::ret(const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC2);
  CHECK(imm.is_uint16());
  EmitUint8(imm.value() & 0xFF);
  EmitUint8((imm.value() >> 8) & 0xFF);
}



void X86Assembler::nop() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x90);
}


void X86Assembler::int3() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xCC);
}


void X86Assembler::hlt() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF4);
}


void X86Assembler::j(Condition condition, Label* label) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (label->IsBound()) {
    static const int kShortSize = 2;
    static const int kLongSize = 6;
    int offset = label->Position() - buffer_.Size();
    CHECK_LE(offset, 0);
    if (IsInt(8, offset - kShortSize)) {
      EmitUint8(0x70 + condition);
      EmitUint8((offset - kShortSize) & 0xFF);
    } else {
      EmitUint8(0x0F);
      EmitUint8(0x80 + condition);
      EmitInt32(offset - kLongSize);
    }
  } else {
    EmitUint8(0x0F);
    EmitUint8(0x80 + condition);
    EmitLabelLink(label);
  }
}


void X86Assembler::jmp(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xFF);
  EmitRegisterOperand(4, reg);
}

void X86Assembler::jmp(const Address& address) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xFF);
  EmitOperand(4, address);
}

void X86Assembler::jmp(Label* label) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (label->IsBound()) {
    static const int kShortSize = 2;
    static const int kLongSize = 5;
    int offset = label->Position() - buffer_.Size();
    CHECK_LE(offset, 0);
    if (IsInt(8, offset - kShortSize)) {
      EmitUint8(0xEB);
      EmitUint8((offset - kShortSize) & 0xFF);
    } else {
      EmitUint8(0xE9);
      EmitInt32(offset - kLongSize);
    }
  } else {
    EmitUint8(0xE9);
    EmitLabelLink(label);
  }
}


X86Assembler* X86Assembler::lock() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xF0);
  return this;
}


void X86Assembler::cmpxchgl(const Address& address, Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xB1);
  EmitOperand(reg, address);
}

void X86Assembler::mfence() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x0F);
  EmitUint8(0xAE);
  EmitUint8(0xF0);
}

X86Assembler* X86Assembler::fs() {
  // TODO: fs is a prefix and not an instruction
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x64);
  return this;
}

void X86Assembler::AddImmediate(Register reg, const Immediate& imm) {
  int value = imm.value();
  if (value > 0) {
    if (value == 1) {
      incl(reg);
    } else if (value != 0) {
      addl(reg, imm);
    }
  } else if (value < 0) {
    value = -value;
    if (value == 1) {
      decl(reg);
    } else if (value != 0) {
      subl(reg, Immediate(value));
    }
  }
}


void X86Assembler::LoadDoubleConstant(XmmRegister dst, double value) {
  // TODO: Need to have a code constants table.
  int64_t constant = bit_cast<int64_t, double>(value);
  pushl(Immediate(High32Bits(constant)));
  pushl(Immediate(Low32Bits(constant)));
  movsd(dst, Address(ESP, 0));
  addl(ESP, Immediate(2 * kWordSize));
}


void X86Assembler::FloatNegate(XmmRegister f) {
  static const struct {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
  } float_negate_constant __attribute__((aligned(16))) =
      { 0x80000000, 0x00000000, 0x80000000, 0x00000000 };
  xorps(f, Address::Absolute(reinterpret_cast<uword>(&float_negate_constant)));
}


void X86Assembler::DoubleNegate(XmmRegister d) {
  static const struct {
    uint64_t a;
    uint64_t b;
  } double_negate_constant __attribute__((aligned(16))) =
      {0x8000000000000000LL, 0x8000000000000000LL};
  xorpd(d, Address::Absolute(reinterpret_cast<uword>(&double_negate_constant)));
}


void X86Assembler::DoubleAbs(XmmRegister reg) {
  static const struct {
    uint64_t a;
    uint64_t b;
  } double_abs_constant __attribute__((aligned(16))) =
      {0x7FFFFFFFFFFFFFFFLL, 0x7FFFFFFFFFFFFFFFLL};
  andpd(reg, Address::Absolute(reinterpret_cast<uword>(&double_abs_constant)));
}


void X86Assembler::Align(int alignment, int offset) {
  CHECK(IsPowerOfTwo(alignment));
  // Emit nop instruction until the real position is aligned.
  while (((offset + buffer_.GetPosition()) & (alignment-1)) != 0) {
    nop();
  }
}


void X86Assembler::Bind(Label* label) {
  int bound = buffer_.Size();
  CHECK(!label->IsBound());  // Labels can only be bound once.
  while (label->IsLinked()) {
    int position = label->LinkPosition();
    int next = buffer_.Load<int32_t>(position);
    buffer_.Store<int32_t>(position, bound - (position + 4));
    label->position_ = next;
  }
  label->BindTo(bound);
}


void X86Assembler::Stop(const char* message) {
  // Emit the message address as immediate operand in the test rax instruction,
  // followed by the int3 instruction.
  // Execution can be resumed with the 'cont' command in gdb.
  testl(EAX, Immediate(reinterpret_cast<int32_t>(message)));
  int3();
}


void X86Assembler::EmitOperand(int reg_or_opcode, const Operand& operand) {
  CHECK_GE(reg_or_opcode, 0);
  CHECK_LT(reg_or_opcode, 8);
  const int length = operand.length_;
  CHECK_GT(length, 0);
  // Emit the ModRM byte updated with the given reg value.
  CHECK_EQ(operand.encoding_[0] & 0x38, 0);
  EmitUint8(operand.encoding_[0] + (reg_or_opcode << 3));
  // Emit the rest of the encoded operand.
  for (int i = 1; i < length; i++) {
    EmitUint8(operand.encoding_[i]);
  }
}


void X86Assembler::EmitImmediate(const Immediate& imm) {
  EmitInt32(imm.value());
}


void X86Assembler::EmitComplex(int reg_or_opcode,
                               const Operand& operand,
                               const Immediate& immediate) {
  CHECK_GE(reg_or_opcode, 0);
  CHECK_LT(reg_or_opcode, 8);
  if (immediate.is_int8()) {
    // Use sign-extended 8-bit immediate.
    EmitUint8(0x83);
    EmitOperand(reg_or_opcode, operand);
    EmitUint8(immediate.value() & 0xFF);
  } else if (operand.IsRegister(EAX)) {
    // Use short form if the destination is eax.
    EmitUint8(0x05 + (reg_or_opcode << 3));
    EmitImmediate(immediate);
  } else {
    EmitUint8(0x81);
    EmitOperand(reg_or_opcode, operand);
    EmitImmediate(immediate);
  }
}


void X86Assembler::EmitLabel(Label* label, int instruction_size) {
  if (label->IsBound()) {
    int offset = label->Position() - buffer_.Size();
    CHECK_LE(offset, 0);
    EmitInt32(offset - instruction_size);
  } else {
    EmitLabelLink(label);
  }
}


void X86Assembler::EmitLabelLink(Label* label) {
  CHECK(!label->IsBound());
  int position = buffer_.Size();
  EmitInt32(label->position_);
  label->LinkTo(position);
}


void X86Assembler::EmitGenericShift(int reg_or_opcode,
                                    Register reg,
                                    const Immediate& imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  CHECK(imm.is_int8());
  if (imm.value() == 1) {
    EmitUint8(0xD1);
    EmitOperand(reg_or_opcode, Operand(reg));
  } else {
    EmitUint8(0xC1);
    EmitOperand(reg_or_opcode, Operand(reg));
    EmitUint8(imm.value() & 0xFF);
  }
}


void X86Assembler::EmitGenericShift(int reg_or_opcode,
                                    Register operand,
                                    Register shifter) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  CHECK_EQ(shifter, ECX);
  EmitUint8(0xD3);
  EmitOperand(reg_or_opcode, Operand(operand));
}

void X86Assembler::BuildFrame(size_t frame_size, ManagedRegister method_reg,
                              const std::vector<ManagedRegister>& spill_regs,
                              const std::vector<ManagedRegister>& entry_spills) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  for (int i = spill_regs.size() - 1; i >= 0; --i) {
    pushl(spill_regs.at(i).AsX86().AsCpuRegister());
  }
  // return address then method on stack
  addl(ESP, Immediate(-frame_size + (spill_regs.size() * kPointerSize) +
                      kPointerSize /*method*/ + kPointerSize /*return address*/));
  pushl(method_reg.AsX86().AsCpuRegister());
  for (size_t i = 0; i < entry_spills.size(); ++i) {
    movl(Address(ESP, frame_size + kPointerSize + (i * kPointerSize)),
         entry_spills.at(i).AsX86().AsCpuRegister());
  }
}

void X86Assembler::RemoveFrame(size_t frame_size,
                            const std::vector<ManagedRegister>& spill_regs) {
  CHECK_ALIGNED(frame_size, kStackAlignment);
  addl(ESP, Immediate(frame_size - (spill_regs.size() * kPointerSize) - kPointerSize));
  for (size_t i = 0; i < spill_regs.size(); ++i) {
    popl(spill_regs.at(i).AsX86().AsCpuRegister());
  }
  ret();
}

void X86Assembler::IncreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  addl(ESP, Immediate(-adjust));
}

void X86Assembler::DecreaseFrameSize(size_t adjust) {
  CHECK_ALIGNED(adjust, kStackAlignment);
  addl(ESP, Immediate(adjust));
}

void X86Assembler::Store(FrameOffset offs, ManagedRegister msrc, size_t size) {
  X86ManagedRegister src = msrc.AsX86();
  if (src.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (src.IsCpuRegister()) {
    CHECK_EQ(4u, size);
    movl(Address(ESP, offs), src.AsCpuRegister());
  } else if (src.IsRegisterPair()) {
    CHECK_EQ(8u, size);
    movl(Address(ESP, offs), src.AsRegisterPairLow());
    movl(Address(ESP, FrameOffset(offs.Int32Value()+4)),
         src.AsRegisterPairHigh());
  } else if (src.IsX87Register()) {
    if (size == 4) {
      fstps(Address(ESP, offs));
    } else {
      fstpl(Address(ESP, offs));
    }
  } else {
    CHECK(src.IsXmmRegister());
    if (size == 4) {
      movss(Address(ESP, offs), src.AsXmmRegister());
    } else {
      movsd(Address(ESP, offs), src.AsXmmRegister());
    }
  }
}

void X86Assembler::StoreRef(FrameOffset dest, ManagedRegister msrc) {
  X86ManagedRegister src = msrc.AsX86();
  CHECK(src.IsCpuRegister());
  movl(Address(ESP, dest), src.AsCpuRegister());
}

void X86Assembler::StoreRawPtr(FrameOffset dest, ManagedRegister msrc) {
  X86ManagedRegister src = msrc.AsX86();
  CHECK(src.IsCpuRegister());
  movl(Address(ESP, dest), src.AsCpuRegister());
}

void X86Assembler::StoreImmediateToFrame(FrameOffset dest, uint32_t imm,
                                         ManagedRegister) {
  movl(Address(ESP, dest), Immediate(imm));
}

void X86Assembler::StoreImmediateToThread(ThreadOffset dest, uint32_t imm,
                                          ManagedRegister) {
  fs()->movl(Address::Absolute(dest), Immediate(imm));
}

void X86Assembler::StoreStackOffsetToThread(ThreadOffset thr_offs,
                                            FrameOffset fr_offs,
                                            ManagedRegister mscratch) {
  X86ManagedRegister scratch = mscratch.AsX86();
  CHECK(scratch.IsCpuRegister());
  leal(scratch.AsCpuRegister(), Address(ESP, fr_offs));
  fs()->movl(Address::Absolute(thr_offs), scratch.AsCpuRegister());
}

void X86Assembler::StoreStackPointerToThread(ThreadOffset thr_offs) {
  fs()->movl(Address::Absolute(thr_offs), ESP);
}

void X86Assembler::StoreLabelToThread(ThreadOffset thr_offs, Label* lbl) {
  fs()->movl(Address::Absolute(thr_offs), lbl);
}

void X86Assembler::StoreSpanning(FrameOffset /*dst*/, ManagedRegister /*src*/,
                                 FrameOffset /*in_off*/, ManagedRegister /*scratch*/) {
  UNIMPLEMENTED(FATAL);  // this case only currently exists for ARM
}

void X86Assembler::Load(ManagedRegister mdest, FrameOffset src, size_t size) {
  X86ManagedRegister dest = mdest.AsX86();
  if (dest.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (dest.IsCpuRegister()) {
    CHECK_EQ(4u, size);
    movl(dest.AsCpuRegister(), Address(ESP, src));
  } else if (dest.IsRegisterPair()) {
    CHECK_EQ(8u, size);
    movl(dest.AsRegisterPairLow(), Address(ESP, src));
    movl(dest.AsRegisterPairHigh(), Address(ESP, FrameOffset(src.Int32Value()+4)));
  } else if (dest.IsX87Register()) {
    if (size == 4) {
      flds(Address(ESP, src));
    } else {
      fldl(Address(ESP, src));
    }
  } else {
    CHECK(dest.IsXmmRegister());
    if (size == 4) {
      movss(dest.AsXmmRegister(), Address(ESP, src));
    } else {
      movsd(dest.AsXmmRegister(), Address(ESP, src));
    }
  }
}

void X86Assembler::Load(ManagedRegister mdest, ThreadOffset src, size_t size) {
  X86ManagedRegister dest = mdest.AsX86();
  if (dest.IsNoRegister()) {
    CHECK_EQ(0u, size);
  } else if (dest.IsCpuRegister()) {
    CHECK_EQ(4u, size);
    fs()->movl(dest.AsCpuRegister(), Address::Absolute(src));
  } else if (dest.IsRegisterPair()) {
    CHECK_EQ(8u, size);
    fs()->movl(dest.AsRegisterPairLow(), Address::Absolute(src));
    fs()->movl(dest.AsRegisterPairHigh(), Address::Absolute(ThreadOffset(src.Int32Value()+4)));
  } else if (dest.IsX87Register()) {
    if (size == 4) {
      fs()->flds(Address::Absolute(src));
    } else {
      fs()->fldl(Address::Absolute(src));
    }
  } else {
    CHECK(dest.IsXmmRegister());
    if (size == 4) {
      fs()->movss(dest.AsXmmRegister(), Address::Absolute(src));
    } else {
      fs()->movsd(dest.AsXmmRegister(), Address::Absolute(src));
    }
  }
}

void X86Assembler::LoadRef(ManagedRegister mdest, FrameOffset  src) {
  X86ManagedRegister dest = mdest.AsX86();
  CHECK(dest.IsCpuRegister());
  movl(dest.AsCpuRegister(), Address(ESP, src));
}

void X86Assembler::LoadRef(ManagedRegister mdest, ManagedRegister base,
                           MemberOffset offs) {
  X86ManagedRegister dest = mdest.AsX86();
  CHECK(dest.IsCpuRegister() && dest.IsCpuRegister());
  movl(dest.AsCpuRegister(), Address(base.AsX86().AsCpuRegister(), offs));
}

void X86Assembler::LoadRawPtr(ManagedRegister mdest, ManagedRegister base,
                              Offset offs) {
  X86ManagedRegister dest = mdest.AsX86();
  CHECK(dest.IsCpuRegister() && dest.IsCpuRegister());
  movl(dest.AsCpuRegister(), Address(base.AsX86().AsCpuRegister(), offs));
}

void X86Assembler::LoadRawPtrFromThread(ManagedRegister mdest,
                                        ThreadOffset offs) {
  X86ManagedRegister dest = mdest.AsX86();
  CHECK(dest.IsCpuRegister());
  fs()->movl(dest.AsCpuRegister(), Address::Absolute(offs));
}

void X86Assembler::SignExtend(ManagedRegister mreg, size_t size) {
  X86ManagedRegister reg = mreg.AsX86();
  CHECK(size == 1 || size == 2) << size;
  CHECK(reg.IsCpuRegister()) << reg;
  if (size == 1) {
    movsxb(reg.AsCpuRegister(), reg.AsByteRegister());
  } else {
    movsxw(reg.AsCpuRegister(), reg.AsCpuRegister());
  }
}

void X86Assembler::ZeroExtend(ManagedRegister mreg, size_t size) {
  X86ManagedRegister reg = mreg.AsX86();
  CHECK(size == 1 || size == 2) << size;
  CHECK(reg.IsCpuRegister()) << reg;
  if (size == 1) {
    movzxb(reg.AsCpuRegister(), reg.AsByteRegister());
  } else {
    movzxw(reg.AsCpuRegister(), reg.AsCpuRegister());
  }
}

void X86Assembler::Move(ManagedRegister mdest, ManagedRegister msrc, size_t size) {
  X86ManagedRegister dest = mdest.AsX86();
  X86ManagedRegister src = msrc.AsX86();
  if (!dest.Equals(src)) {
    if (dest.IsCpuRegister() && src.IsCpuRegister()) {
      movl(dest.AsCpuRegister(), src.AsCpuRegister());
    } else if (src.IsX87Register() && dest.IsXmmRegister()) {
      // Pass via stack and pop X87 register
      subl(ESP, Immediate(16));
      if (size == 4) {
        CHECK_EQ(src.AsX87Register(), ST0);
        fstps(Address(ESP, 0));
        movss(dest.AsXmmRegister(), Address(ESP, 0));
      } else {
        CHECK_EQ(src.AsX87Register(), ST0);
        fstpl(Address(ESP, 0));
        movsd(dest.AsXmmRegister(), Address(ESP, 0));
      }
      addl(ESP, Immediate(16));
    } else {
      // TODO: x87, SSE
      UNIMPLEMENTED(FATAL) << ": Move " << dest << ", " << src;
    }
  }
}

void X86Assembler::CopyRef(FrameOffset dest, FrameOffset src,
                           ManagedRegister mscratch) {
  X86ManagedRegister scratch = mscratch.AsX86();
  CHECK(scratch.IsCpuRegister());
  movl(scratch.AsCpuRegister(), Address(ESP, src));
  movl(Address(ESP, dest), scratch.AsCpuRegister());
}

void X86Assembler::CopyRawPtrFromThread(FrameOffset fr_offs,
                                        ThreadOffset thr_offs,
                                        ManagedRegister mscratch) {
  X86ManagedRegister scratch = mscratch.AsX86();
  CHECK(scratch.IsCpuRegister());
  fs()->movl(scratch.AsCpuRegister(), Address::Absolute(thr_offs));
  Store(fr_offs, scratch, 4);
}

void X86Assembler::CopyRawPtrToThread(ThreadOffset thr_offs,
                                      FrameOffset fr_offs,
                                      ManagedRegister mscratch) {
  X86ManagedRegister scratch = mscratch.AsX86();
  CHECK(scratch.IsCpuRegister());
  Load(scratch, fr_offs, 4);
  fs()->movl(Address::Absolute(thr_offs), scratch.AsCpuRegister());
}

void X86Assembler::Copy(FrameOffset dest, FrameOffset src,
                        ManagedRegister mscratch,
                        size_t size) {
  X86ManagedRegister scratch = mscratch.AsX86();
  if (scratch.IsCpuRegister() && size == 8) {
    Load(scratch, src, 4);
    Store(dest, scratch, 4);
    Load(scratch, FrameOffset(src.Int32Value() + 4), 4);
    Store(FrameOffset(dest.Int32Value() + 4), scratch, 4);
  } else {
    Load(scratch, src, size);
    Store(dest, scratch, size);
  }
}

void X86Assembler::Copy(FrameOffset /*dst*/, ManagedRegister /*src_base*/, Offset /*src_offset*/,
                        ManagedRegister /*scratch*/, size_t /*size*/) {
  UNIMPLEMENTED(FATAL);
}

void X86Assembler::Copy(ManagedRegister dest_base, Offset dest_offset, FrameOffset src,
                        ManagedRegister scratch, size_t size) {
  CHECK(scratch.IsNoRegister());
  CHECK_EQ(size, 4u);
  pushl(Address(ESP, src));
  popl(Address(dest_base.AsX86().AsCpuRegister(), dest_offset));
}

void X86Assembler::Copy(FrameOffset dest, FrameOffset src_base, Offset src_offset,
                        ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsX86().AsCpuRegister();
  CHECK_EQ(size, 4u);
  movl(scratch, Address(ESP, src_base));
  movl(scratch, Address(scratch, src_offset));
  movl(Address(ESP, dest), scratch);
}

void X86Assembler::Copy(ManagedRegister dest, Offset dest_offset,
                        ManagedRegister src, Offset src_offset,
                        ManagedRegister scratch, size_t size) {
  CHECK_EQ(size, 4u);
  CHECK(scratch.IsNoRegister());
  pushl(Address(src.AsX86().AsCpuRegister(), src_offset));
  popl(Address(dest.AsX86().AsCpuRegister(), dest_offset));
}

void X86Assembler::Copy(FrameOffset dest, Offset dest_offset, FrameOffset src, Offset src_offset,
                        ManagedRegister mscratch, size_t size) {
  Register scratch = mscratch.AsX86().AsCpuRegister();
  CHECK_EQ(size, 4u);
  CHECK_EQ(dest.Int32Value(), src.Int32Value());
  movl(scratch, Address(ESP, src));
  pushl(Address(scratch, src_offset));
  popl(Address(scratch, dest_offset));
}

void X86Assembler::MemoryBarrier(ManagedRegister) {
#if ANDROID_SMP != 0
  mfence();
#endif
}

void X86Assembler::CreateSirtEntry(ManagedRegister mout_reg,
                                   FrameOffset sirt_offset,
                                   ManagedRegister min_reg, bool null_allowed) {
  X86ManagedRegister out_reg = mout_reg.AsX86();
  X86ManagedRegister in_reg = min_reg.AsX86();
  CHECK(in_reg.IsCpuRegister());
  CHECK(out_reg.IsCpuRegister());
  VerifyObject(in_reg, null_allowed);
  if (null_allowed) {
    Label null_arg;
    if (!out_reg.Equals(in_reg)) {
      xorl(out_reg.AsCpuRegister(), out_reg.AsCpuRegister());
    }
    testl(in_reg.AsCpuRegister(), in_reg.AsCpuRegister());
    j(kZero, &null_arg);
    leal(out_reg.AsCpuRegister(), Address(ESP, sirt_offset));
    Bind(&null_arg);
  } else {
    leal(out_reg.AsCpuRegister(), Address(ESP, sirt_offset));
  }
}

void X86Assembler::CreateSirtEntry(FrameOffset out_off,
                                   FrameOffset sirt_offset,
                                   ManagedRegister mscratch,
                                   bool null_allowed) {
  X86ManagedRegister scratch = mscratch.AsX86();
  CHECK(scratch.IsCpuRegister());
  if (null_allowed) {
    Label null_arg;
    movl(scratch.AsCpuRegister(), Address(ESP, sirt_offset));
    testl(scratch.AsCpuRegister(), scratch.AsCpuRegister());
    j(kZero, &null_arg);
    leal(scratch.AsCpuRegister(), Address(ESP, sirt_offset));
    Bind(&null_arg);
  } else {
    leal(scratch.AsCpuRegister(), Address(ESP, sirt_offset));
  }
  Store(out_off, scratch, 4);
}

// Given a SIRT entry, load the associated reference.
void X86Assembler::LoadReferenceFromSirt(ManagedRegister mout_reg,
                                         ManagedRegister min_reg) {
  X86ManagedRegister out_reg = mout_reg.AsX86();
  X86ManagedRegister in_reg = min_reg.AsX86();
  CHECK(out_reg.IsCpuRegister());
  CHECK(in_reg.IsCpuRegister());
  Label null_arg;
  if (!out_reg.Equals(in_reg)) {
    xorl(out_reg.AsCpuRegister(), out_reg.AsCpuRegister());
  }
  testl(in_reg.AsCpuRegister(), in_reg.AsCpuRegister());
  j(kZero, &null_arg);
  movl(out_reg.AsCpuRegister(), Address(in_reg.AsCpuRegister(), 0));
  Bind(&null_arg);
}

void X86Assembler::VerifyObject(ManagedRegister /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void X86Assembler::VerifyObject(FrameOffset /*src*/, bool /*could_be_null*/) {
  // TODO: not validating references
}

void X86Assembler::Call(ManagedRegister mbase, Offset offset, ManagedRegister) {
  X86ManagedRegister base = mbase.AsX86();
  CHECK(base.IsCpuRegister());
  call(Address(base.AsCpuRegister(), offset.Int32Value()));
  // TODO: place reference map on call
}

void X86Assembler::Call(FrameOffset base, Offset offset, ManagedRegister mscratch) {
  Register scratch = mscratch.AsX86().AsCpuRegister();
  movl(scratch, Address(ESP, base));
  call(Address(scratch, offset));
}

void X86Assembler::Call(ThreadOffset offset, ManagedRegister /*mscratch*/) {
  fs()->call(Address::Absolute(offset));
}

void X86Assembler::GetCurrentThread(ManagedRegister tr) {
  fs()->movl(tr.AsX86().AsCpuRegister(),
             Address::Absolute(Thread::SelfOffset()));
}

void X86Assembler::GetCurrentThread(FrameOffset offset,
                                    ManagedRegister mscratch) {
  X86ManagedRegister scratch = mscratch.AsX86();
  fs()->movl(scratch.AsCpuRegister(), Address::Absolute(Thread::SelfOffset()));
  movl(Address(ESP, offset), scratch.AsCpuRegister());
}

void X86Assembler::ExceptionPoll(ManagedRegister /*scratch*/, size_t stack_adjust) {
  X86ExceptionSlowPath* slow = new X86ExceptionSlowPath(stack_adjust);
  buffer_.EnqueueSlowPath(slow);
  fs()->cmpl(Address::Absolute(Thread::ExceptionOffset()), Immediate(0));
  j(kNotEqual, slow->Entry());
}

void X86ExceptionSlowPath::Emit(Assembler *sasm) {
  X86Assembler* sp_asm = down_cast<X86Assembler*>(sasm);
#define __ sp_asm->
  __ Bind(&entry_);
  // Note: the return value is dead
  if (stack_adjust_ != 0) {  // Fix up the frame.
    __ DecreaseFrameSize(stack_adjust_);
  }
  // Pass exception as argument in EAX
  __ fs()->movl(EAX, Address::Absolute(Thread::ExceptionOffset()));
  __ fs()->call(Address::Absolute(QUICK_ENTRYPOINT_OFFSET(pDeliverException)));
  // this call should never return
  __ int3();
#undef __
}

}  // namespace x86
}  // namespace art
