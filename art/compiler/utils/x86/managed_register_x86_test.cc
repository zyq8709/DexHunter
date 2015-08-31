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

#include "globals.h"
#include "managed_register_x86.h"
#include "gtest/gtest.h"

namespace art {
namespace x86 {

TEST(X86ManagedRegister, NoRegister) {
  X86ManagedRegister reg = ManagedRegister::NoRegister().AsX86();
  EXPECT_TRUE(reg.IsNoRegister());
  EXPECT_TRUE(!reg.Overlaps(reg));
}

TEST(X86ManagedRegister, CpuRegister) {
  X86ManagedRegister reg = X86ManagedRegister::FromCpuRegister(EAX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(EAX, reg.AsCpuRegister());

  reg = X86ManagedRegister::FromCpuRegister(EBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(EBX, reg.AsCpuRegister());

  reg = X86ManagedRegister::FromCpuRegister(ECX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(ECX, reg.AsCpuRegister());

  reg = X86ManagedRegister::FromCpuRegister(EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(EDI, reg.AsCpuRegister());
}

TEST(X86ManagedRegister, XmmRegister) {
  X86ManagedRegister reg = X86ManagedRegister::FromXmmRegister(XMM0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(XMM0, reg.AsXmmRegister());

  reg = X86ManagedRegister::FromXmmRegister(XMM1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(XMM1, reg.AsXmmRegister());

  reg = X86ManagedRegister::FromXmmRegister(XMM7);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(XMM7, reg.AsXmmRegister());
}

TEST(X86ManagedRegister, X87Register) {
  X86ManagedRegister reg = X86ManagedRegister::FromX87Register(ST0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(ST0, reg.AsX87Register());

  reg = X86ManagedRegister::FromX87Register(ST1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(ST1, reg.AsX87Register());

  reg = X86ManagedRegister::FromX87Register(ST7);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(reg.IsX87Register());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(ST7, reg.AsX87Register());
}

TEST(X86ManagedRegister, RegisterPair) {
  X86ManagedRegister reg = X86ManagedRegister::FromRegisterPair(EAX_EDX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EAX, reg.AsRegisterPairLow());
  EXPECT_EQ(EDX, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(EAX_ECX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EAX, reg.AsRegisterPairLow());
  EXPECT_EQ(ECX, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(EAX_EBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EAX, reg.AsRegisterPairLow());
  EXPECT_EQ(EBX, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(EAX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EAX, reg.AsRegisterPairLow());
  EXPECT_EQ(EDI, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(EDX_ECX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EDX, reg.AsRegisterPairLow());
  EXPECT_EQ(ECX, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(EDX_EBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EDX, reg.AsRegisterPairLow());
  EXPECT_EQ(EBX, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(EDX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EDX, reg.AsRegisterPairLow());
  EXPECT_EQ(EDI, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(ECX_EBX);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(ECX, reg.AsRegisterPairLow());
  EXPECT_EQ(EBX, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(ECX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(ECX, reg.AsRegisterPairLow());
  EXPECT_EQ(EDI, reg.AsRegisterPairHigh());

  reg = X86ManagedRegister::FromRegisterPair(EBX_EDI);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCpuRegister());
  EXPECT_TRUE(!reg.IsXmmRegister());
  EXPECT_TRUE(!reg.IsX87Register());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(EBX, reg.AsRegisterPairLow());
  EXPECT_EQ(EDI, reg.AsRegisterPairHigh());
}

TEST(X86ManagedRegister, Equals) {
  X86ManagedRegister reg_eax = X86ManagedRegister::FromCpuRegister(EAX);
  EXPECT_TRUE(reg_eax.Equals(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_eax.Equals(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  X86ManagedRegister reg_xmm0 = X86ManagedRegister::FromXmmRegister(XMM0);
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(reg_xmm0.Equals(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_xmm0.Equals(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  X86ManagedRegister reg_st0 = X86ManagedRegister::FromX87Register(ST0);
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(reg_st0.Equals(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_st0.Equals(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  X86ManagedRegister reg_pair = X86ManagedRegister::FromRegisterPair(EAX_EDX);
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg_pair.Equals(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg_pair.Equals(X86ManagedRegister::FromRegisterPair(EBX_EDI)));
}

TEST(X86ManagedRegister, Overlaps) {
  X86ManagedRegister reg = X86ManagedRegister::FromCpuRegister(EAX);
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86ManagedRegister::FromCpuRegister(EDX);
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86ManagedRegister::FromCpuRegister(EDI);
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86ManagedRegister::FromCpuRegister(EBX);
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86ManagedRegister::FromXmmRegister(XMM0);
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86ManagedRegister::FromX87Register(ST0);
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86ManagedRegister::FromRegisterPair(EAX_EDX);
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EDX_ECX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));

  reg = X86ManagedRegister::FromRegisterPair(EBX_EDI);
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EDX_EBX)));

  reg = X86ManagedRegister::FromRegisterPair(EDX_ECX);
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EAX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EBX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromCpuRegister(EDI)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromXmmRegister(XMM7)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST0)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromX87Register(ST7)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EAX_EDX)));
  EXPECT_TRUE(!reg.Overlaps(X86ManagedRegister::FromRegisterPair(EBX_EDI)));
  EXPECT_TRUE(reg.Overlaps(X86ManagedRegister::FromRegisterPair(EDX_EBX)));
}

}  // namespace x86
}  // namespace art
