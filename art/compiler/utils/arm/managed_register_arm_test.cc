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
#include "managed_register_arm.h"
#include "gtest/gtest.h"

namespace art {
namespace arm {

TEST(ArmManagedRegister, NoRegister) {
  ArmManagedRegister reg = ManagedRegister::NoRegister().AsArm();
  EXPECT_TRUE(reg.IsNoRegister());
  EXPECT_TRUE(!reg.Overlaps(reg));
}

TEST(ArmManagedRegister, CoreRegister) {
  ArmManagedRegister reg = ArmManagedRegister::FromCoreRegister(R0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(R0, reg.AsCoreRegister());

  reg = ArmManagedRegister::FromCoreRegister(R1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(R1, reg.AsCoreRegister());

  reg = ArmManagedRegister::FromCoreRegister(R8);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(R8, reg.AsCoreRegister());

  reg = ArmManagedRegister::FromCoreRegister(R15);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(R15, reg.AsCoreRegister());
}


TEST(ArmManagedRegister, SRegister) {
  ArmManagedRegister reg = ArmManagedRegister::FromSRegister(S0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(S0, reg.AsSRegister());

  reg = ArmManagedRegister::FromSRegister(S1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(S1, reg.AsSRegister());

  reg = ArmManagedRegister::FromSRegister(S3);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(S3, reg.AsSRegister());

  reg = ArmManagedRegister::FromSRegister(S15);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(S15, reg.AsSRegister());

  reg = ArmManagedRegister::FromSRegister(S30);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(S30, reg.AsSRegister());

  reg = ArmManagedRegister::FromSRegister(S31);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(S31, reg.AsSRegister());
}


TEST(ArmManagedRegister, DRegister) {
  ArmManagedRegister reg = ArmManagedRegister::FromDRegister(D0);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D0, reg.AsDRegister());
  EXPECT_EQ(S0, reg.AsOverlappingDRegisterLow());
  EXPECT_EQ(S1, reg.AsOverlappingDRegisterHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromSRegisterPair(S0)));

  reg = ArmManagedRegister::FromDRegister(D1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D1, reg.AsDRegister());
  EXPECT_EQ(S2, reg.AsOverlappingDRegisterLow());
  EXPECT_EQ(S3, reg.AsOverlappingDRegisterHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromSRegisterPair(S2)));

  reg = ArmManagedRegister::FromDRegister(D6);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D6, reg.AsDRegister());
  EXPECT_EQ(S12, reg.AsOverlappingDRegisterLow());
  EXPECT_EQ(S13, reg.AsOverlappingDRegisterHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromSRegisterPair(S12)));

  reg = ArmManagedRegister::FromDRegister(D14);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D14, reg.AsDRegister());
  EXPECT_EQ(S28, reg.AsOverlappingDRegisterLow());
  EXPECT_EQ(S29, reg.AsOverlappingDRegisterHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromSRegisterPair(S28)));

  reg = ArmManagedRegister::FromDRegister(D15);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D15, reg.AsDRegister());
  EXPECT_EQ(S30, reg.AsOverlappingDRegisterLow());
  EXPECT_EQ(S31, reg.AsOverlappingDRegisterHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromSRegisterPair(S30)));

#ifdef VFPv3_D32
  reg = ArmManagedRegister::FromDRegister(D16);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D16, reg.AsDRegister());

  reg = ArmManagedRegister::FromDRegister(D18);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D18, reg.AsDRegister());

  reg = ArmManagedRegister::FromDRegister(D30);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D30, reg.AsDRegister());

  reg = ArmManagedRegister::FromDRegister(D31);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(!reg.IsRegisterPair());
  EXPECT_EQ(D31, reg.AsDRegister());
#endif  // VFPv3_D32
}


TEST(ArmManagedRegister, Pair) {
  ArmManagedRegister reg = ArmManagedRegister::FromRegisterPair(R0_R1);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(R0_R1, reg.AsRegisterPair());
  EXPECT_EQ(R0, reg.AsRegisterPairLow());
  EXPECT_EQ(R1, reg.AsRegisterPairHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromCoreRegisterPair(R0)));

  reg = ArmManagedRegister::FromRegisterPair(R1_R2);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(R1_R2, reg.AsRegisterPair());
  EXPECT_EQ(R1, reg.AsRegisterPairLow());
  EXPECT_EQ(R2, reg.AsRegisterPairHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromCoreRegisterPair(R1)));

  reg = ArmManagedRegister::FromRegisterPair(R2_R3);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(R2_R3, reg.AsRegisterPair());
  EXPECT_EQ(R2, reg.AsRegisterPairLow());
  EXPECT_EQ(R3, reg.AsRegisterPairHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromCoreRegisterPair(R2)));

  reg = ArmManagedRegister::FromRegisterPair(R4_R5);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(R4_R5, reg.AsRegisterPair());
  EXPECT_EQ(R4, reg.AsRegisterPairLow());
  EXPECT_EQ(R5, reg.AsRegisterPairHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromCoreRegisterPair(R4)));

  reg = ArmManagedRegister::FromRegisterPair(R6_R7);
  EXPECT_TRUE(!reg.IsNoRegister());
  EXPECT_TRUE(!reg.IsCoreRegister());
  EXPECT_TRUE(!reg.IsSRegister());
  EXPECT_TRUE(!reg.IsDRegister());
  EXPECT_TRUE(!reg.IsOverlappingDRegister());
  EXPECT_TRUE(reg.IsRegisterPair());
  EXPECT_EQ(R6_R7, reg.AsRegisterPair());
  EXPECT_EQ(R6, reg.AsRegisterPairLow());
  EXPECT_EQ(R7, reg.AsRegisterPairHigh());
  EXPECT_TRUE(reg.Equals(ArmManagedRegister::FromCoreRegisterPair(R6)));
}


TEST(ArmManagedRegister, Equals) {
  ManagedRegister no_reg = ManagedRegister::NoRegister();
  EXPECT_TRUE(no_reg.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!no_reg.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!no_reg.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!no_reg.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!no_reg.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!no_reg.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_R0 = ArmManagedRegister::FromCoreRegister(R0);
  EXPECT_TRUE(!reg_R0.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(reg_R0.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_R0.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_R0.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_R0.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_R0.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_R1 = ArmManagedRegister::FromCoreRegister(R1);
  EXPECT_TRUE(!reg_R1.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_R1.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(reg_R1.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_R1.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_R1.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_R1.Equals(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg_R1.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_R1.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_R8 = ArmManagedRegister::FromCoreRegister(R8);
  EXPECT_TRUE(!reg_R8.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_R8.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(reg_R8.Equals(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg_R8.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_R8.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_R8.Equals(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg_R8.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_R8.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_S0 = ArmManagedRegister::FromSRegister(S0);
  EXPECT_TRUE(!reg_S0.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_S0.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_S0.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(reg_S0.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_S0.Equals(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg_S0.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_S0.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_S0.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_S1 = ArmManagedRegister::FromSRegister(S1);
  EXPECT_TRUE(!reg_S1.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_S1.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_S1.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_S1.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg_S1.Equals(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg_S1.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_S1.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_S1.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_S31 = ArmManagedRegister::FromSRegister(S31);
  EXPECT_TRUE(!reg_S31.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_S31.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_S31.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_S31.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg_S31.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_S31.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_S31.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_S31.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_D0 = ArmManagedRegister::FromDRegister(D0);
  EXPECT_TRUE(!reg_D0.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_D0.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_D0.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_D0.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D0.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(reg_D0.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_D0.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_D0.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_D15 = ArmManagedRegister::FromDRegister(D15);
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(reg_D15.Equals(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg_D15.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

#ifdef VFPv3_D32
  ArmManagedRegister reg_D16 = ArmManagedRegister::FromDRegister(D16);
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(reg_D16.Equals(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg_D16.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_D30 = ArmManagedRegister::FromDRegister(D30);
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(reg_D30.Equals(ArmManagedRegister::FromDRegister(D30)));
  EXPECT_TRUE(!reg_D30.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));

  ArmManagedRegister reg_D31 = ArmManagedRegister::FromDRegister(D30);
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromDRegister(D30)));
  EXPECT_TRUE(reg_D31.Equals(ArmManagedRegister::FromDRegister(D31)));
  EXPECT_TRUE(!reg_D31.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));
#endif  // VFPv3_D32

  ArmManagedRegister reg_R0R1 = ArmManagedRegister::FromRegisterPair(R0_R1);
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(reg_R0R1.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg_R0R1.Equals(ArmManagedRegister::FromRegisterPair(R2_R3)));

  ArmManagedRegister reg_R4R5 = ArmManagedRegister::FromRegisterPair(R4_R5);
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(reg_R4R5.Equals(ArmManagedRegister::FromRegisterPair(R4_R5)));
  EXPECT_TRUE(!reg_R4R5.Equals(ArmManagedRegister::FromRegisterPair(R6_R7)));

  ArmManagedRegister reg_R6R7 = ArmManagedRegister::FromRegisterPair(R6_R7);
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::NoRegister()));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg_R6R7.Equals(ArmManagedRegister::FromRegisterPair(R4_R5)));
  EXPECT_TRUE(reg_R6R7.Equals(ArmManagedRegister::FromRegisterPair(R6_R7)));
}


TEST(ArmManagedRegister, Overlaps) {
  ArmManagedRegister reg = ArmManagedRegister::FromCoreRegister(R0);
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromCoreRegister(R1);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromCoreRegister(R7);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromSRegister(S0);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromSRegister(S1);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromSRegister(S15);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromSRegister(S31);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromDRegister(D0);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromDRegister(D7);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromDRegister(D15);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

#ifdef VFPv3_D32
  reg = ArmManagedRegister::FromDRegister(D16);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromDRegister(D31);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));
#endif  // VFPv3_D32

  reg = ArmManagedRegister::FromRegisterPair(R0_R1);
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));

  reg = ArmManagedRegister::FromRegisterPair(R4_R5);
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromCoreRegister(R8)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S2)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S15)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S30)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromSRegister(S31)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D0)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D1)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D7)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D15)));
#ifdef VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D16)));
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromDRegister(D31)));
#endif  // VFPv3_D32
  EXPECT_TRUE(!reg.Overlaps(ArmManagedRegister::FromRegisterPair(R0_R1)));
  EXPECT_TRUE(reg.Overlaps(ArmManagedRegister::FromRegisterPair(R4_R5)));
}

}  // namespace arm
}  // namespace art
