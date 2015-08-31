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

#include "reg_type.h"
#include "reg_type_cache-inl.h"

#include "base/casts.h"
#include "common_test.h"
#include <set>

namespace art {
namespace verifier {

class RegTypeTest : public CommonTest {};

TEST_F(RegTypeTest, ConstLoHi) {
  // Tests creating primitive types types.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  const RegType& ref_type_const_0 = cache.FromCat1Const(10, true);
  const RegType& ref_type_const_1 = cache.FromCat1Const(10, true);
  const RegType& ref_type_const_2 = cache.FromCat1Const(30, true);
  const RegType& ref_type_const_3 = cache.FromCat1Const(30, false);
  EXPECT_TRUE(ref_type_const_0.Equals(ref_type_const_1));
  EXPECT_FALSE(ref_type_const_0.Equals(ref_type_const_2));
  EXPECT_FALSE(ref_type_const_0.Equals(ref_type_const_3));

  const RegType& ref_type_const_wide_0 = cache.FromCat2ConstHi(50, true);
  const RegType& ref_type_const_wide_1 = cache.FromCat2ConstHi(50, true);
  EXPECT_TRUE(ref_type_const_wide_0.Equals(ref_type_const_wide_1));

  const RegType& ref_type_const_wide_2 = cache.FromCat2ConstLo(50, true);
  const RegType& ref_type_const_wide_3 = cache.FromCat2ConstLo(50, true);
  const RegType& ref_type_const_wide_4 = cache.FromCat2ConstLo(55, true);
  EXPECT_TRUE(ref_type_const_wide_2.Equals(ref_type_const_wide_3));
  EXPECT_FALSE(ref_type_const_wide_2.Equals(ref_type_const_wide_4));
}

TEST_F(RegTypeTest, Pairs) {
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  int64_t val = static_cast<int32_t>(1234);
  const RegType& precise_lo = cache.FromCat2ConstLo(static_cast<int32_t>(val), true);
  const RegType& precise_hi = cache.FromCat2ConstHi(static_cast<int32_t>(val >> 32), true);
  const RegType& precise_const = cache.FromCat1Const(static_cast<int32_t>(val >> 32), true);
  const RegType& long_lo = cache.LongLo();
  const RegType& long_hi = cache.LongHi();
  // Check sanity of types.
  EXPECT_TRUE(precise_lo.IsLowHalf());
  EXPECT_FALSE(precise_hi.IsLowHalf());
  EXPECT_FALSE(precise_lo.IsHighHalf());
  EXPECT_TRUE(precise_hi.IsHighHalf());
  EXPECT_TRUE(long_hi.IsLongHighTypes());
  EXPECT_TRUE(precise_hi.IsLongHighTypes());
  // Check Pairing.
  EXPECT_FALSE(precise_lo.CheckWidePair(precise_const));
  EXPECT_TRUE(precise_lo.CheckWidePair(precise_hi));
  // Test Merging.
  EXPECT_TRUE((long_lo.Merge(precise_lo, &cache)).IsLongTypes());
  EXPECT_TRUE((long_hi.Merge(precise_hi, &cache)).IsLongHighTypes());
}

TEST_F(RegTypeTest, Primitives) {
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);

  const RegType& bool_reg_type = cache.Boolean();
  EXPECT_FALSE(bool_reg_type.IsUndefined());
  EXPECT_FALSE(bool_reg_type.IsConflict());
  EXPECT_FALSE(bool_reg_type.IsZero());
  EXPECT_FALSE(bool_reg_type.IsOne());
  EXPECT_FALSE(bool_reg_type.IsLongConstant());
  EXPECT_TRUE(bool_reg_type.IsBoolean());
  EXPECT_FALSE(bool_reg_type.IsByte());
  EXPECT_FALSE(bool_reg_type.IsChar());
  EXPECT_FALSE(bool_reg_type.IsShort());
  EXPECT_FALSE(bool_reg_type.IsInteger());
  EXPECT_FALSE(bool_reg_type.IsLong());
  EXPECT_FALSE(bool_reg_type.IsFloat());
  EXPECT_FALSE(bool_reg_type.IsDouble());
  EXPECT_FALSE(bool_reg_type.IsReference());
  EXPECT_FALSE(bool_reg_type.IsLowHalf());
  EXPECT_FALSE(bool_reg_type.IsHighHalf());
  EXPECT_FALSE(bool_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(bool_reg_type.IsReferenceTypes());
  EXPECT_TRUE(bool_reg_type.IsCategory1Types());
  EXPECT_FALSE(bool_reg_type.IsCategory2Types());
  EXPECT_TRUE(bool_reg_type.IsBooleanTypes());
  EXPECT_TRUE(bool_reg_type.IsByteTypes());
  EXPECT_TRUE(bool_reg_type.IsShortTypes());
  EXPECT_TRUE(bool_reg_type.IsCharTypes());
  EXPECT_TRUE(bool_reg_type.IsIntegralTypes());
  EXPECT_FALSE(bool_reg_type.IsFloatTypes());
  EXPECT_FALSE(bool_reg_type.IsLongTypes());
  EXPECT_FALSE(bool_reg_type.IsDoubleTypes());
  EXPECT_TRUE(bool_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(bool_reg_type.IsNonZeroReferenceTypes());

  const RegType& byte_reg_type = cache.Byte();
  EXPECT_FALSE(byte_reg_type.IsUndefined());
  EXPECT_FALSE(byte_reg_type.IsConflict());
  EXPECT_FALSE(byte_reg_type.IsZero());
  EXPECT_FALSE(byte_reg_type.IsOne());
  EXPECT_FALSE(byte_reg_type.IsLongConstant());
  EXPECT_FALSE(byte_reg_type.IsBoolean());
  EXPECT_TRUE(byte_reg_type.IsByte());
  EXPECT_FALSE(byte_reg_type.IsChar());
  EXPECT_FALSE(byte_reg_type.IsShort());
  EXPECT_FALSE(byte_reg_type.IsInteger());
  EXPECT_FALSE(byte_reg_type.IsLong());
  EXPECT_FALSE(byte_reg_type.IsFloat());
  EXPECT_FALSE(byte_reg_type.IsDouble());
  EXPECT_FALSE(byte_reg_type.IsReference());
  EXPECT_FALSE(byte_reg_type.IsLowHalf());
  EXPECT_FALSE(byte_reg_type.IsHighHalf());
  EXPECT_FALSE(byte_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(byte_reg_type.IsReferenceTypes());
  EXPECT_TRUE(byte_reg_type.IsCategory1Types());
  EXPECT_FALSE(byte_reg_type.IsCategory2Types());
  EXPECT_FALSE(byte_reg_type.IsBooleanTypes());
  EXPECT_TRUE(byte_reg_type.IsByteTypes());
  EXPECT_TRUE(byte_reg_type.IsShortTypes());
  EXPECT_FALSE(byte_reg_type.IsCharTypes());
  EXPECT_TRUE(byte_reg_type.IsIntegralTypes());
  EXPECT_FALSE(byte_reg_type.IsFloatTypes());
  EXPECT_FALSE(byte_reg_type.IsLongTypes());
  EXPECT_FALSE(byte_reg_type.IsDoubleTypes());
  EXPECT_TRUE(byte_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(byte_reg_type.IsNonZeroReferenceTypes());

  const RegType& char_reg_type = cache.Char();
  EXPECT_FALSE(char_reg_type.IsUndefined());
  EXPECT_FALSE(char_reg_type.IsConflict());
  EXPECT_FALSE(char_reg_type.IsZero());
  EXPECT_FALSE(char_reg_type.IsOne());
  EXPECT_FALSE(char_reg_type.IsLongConstant());
  EXPECT_FALSE(char_reg_type.IsBoolean());
  EXPECT_FALSE(char_reg_type.IsByte());
  EXPECT_TRUE(char_reg_type.IsChar());
  EXPECT_FALSE(char_reg_type.IsShort());
  EXPECT_FALSE(char_reg_type.IsInteger());
  EXPECT_FALSE(char_reg_type.IsLong());
  EXPECT_FALSE(char_reg_type.IsFloat());
  EXPECT_FALSE(char_reg_type.IsDouble());
  EXPECT_FALSE(char_reg_type.IsReference());
  EXPECT_FALSE(char_reg_type.IsLowHalf());
  EXPECT_FALSE(char_reg_type.IsHighHalf());
  EXPECT_FALSE(char_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(char_reg_type.IsReferenceTypes());
  EXPECT_TRUE(char_reg_type.IsCategory1Types());
  EXPECT_FALSE(char_reg_type.IsCategory2Types());
  EXPECT_FALSE(char_reg_type.IsBooleanTypes());
  EXPECT_FALSE(char_reg_type.IsByteTypes());
  EXPECT_FALSE(char_reg_type.IsShortTypes());
  EXPECT_TRUE(char_reg_type.IsCharTypes());
  EXPECT_TRUE(char_reg_type.IsIntegralTypes());
  EXPECT_FALSE(char_reg_type.IsFloatTypes());
  EXPECT_FALSE(char_reg_type.IsLongTypes());
  EXPECT_FALSE(char_reg_type.IsDoubleTypes());
  EXPECT_TRUE(char_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(char_reg_type.IsNonZeroReferenceTypes());

  const RegType& short_reg_type = cache.Short();
  EXPECT_FALSE(short_reg_type.IsUndefined());
  EXPECT_FALSE(short_reg_type.IsConflict());
  EXPECT_FALSE(short_reg_type.IsZero());
  EXPECT_FALSE(short_reg_type.IsOne());
  EXPECT_FALSE(short_reg_type.IsLongConstant());
  EXPECT_FALSE(short_reg_type.IsBoolean());
  EXPECT_FALSE(short_reg_type.IsByte());
  EXPECT_FALSE(short_reg_type.IsChar());
  EXPECT_TRUE(short_reg_type.IsShort());
  EXPECT_FALSE(short_reg_type.IsInteger());
  EXPECT_FALSE(short_reg_type.IsLong());
  EXPECT_FALSE(short_reg_type.IsFloat());
  EXPECT_FALSE(short_reg_type.IsDouble());
  EXPECT_FALSE(short_reg_type.IsReference());
  EXPECT_FALSE(short_reg_type.IsLowHalf());
  EXPECT_FALSE(short_reg_type.IsHighHalf());
  EXPECT_FALSE(short_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(short_reg_type.IsReferenceTypes());
  EXPECT_TRUE(short_reg_type.IsCategory1Types());
  EXPECT_FALSE(short_reg_type.IsCategory2Types());
  EXPECT_FALSE(short_reg_type.IsBooleanTypes());
  EXPECT_FALSE(short_reg_type.IsByteTypes());
  EXPECT_TRUE(short_reg_type.IsShortTypes());
  EXPECT_FALSE(short_reg_type.IsCharTypes());
  EXPECT_TRUE(short_reg_type.IsIntegralTypes());
  EXPECT_FALSE(short_reg_type.IsFloatTypes());
  EXPECT_FALSE(short_reg_type.IsLongTypes());
  EXPECT_FALSE(short_reg_type.IsDoubleTypes());
  EXPECT_TRUE(short_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(short_reg_type.IsNonZeroReferenceTypes());

  const RegType& int_reg_type = cache.Integer();
  EXPECT_FALSE(int_reg_type.IsUndefined());
  EXPECT_FALSE(int_reg_type.IsConflict());
  EXPECT_FALSE(int_reg_type.IsZero());
  EXPECT_FALSE(int_reg_type.IsOne());
  EXPECT_FALSE(int_reg_type.IsLongConstant());
  EXPECT_FALSE(int_reg_type.IsBoolean());
  EXPECT_FALSE(int_reg_type.IsByte());
  EXPECT_FALSE(int_reg_type.IsChar());
  EXPECT_FALSE(int_reg_type.IsShort());
  EXPECT_TRUE(int_reg_type.IsInteger());
  EXPECT_FALSE(int_reg_type.IsLong());
  EXPECT_FALSE(int_reg_type.IsFloat());
  EXPECT_FALSE(int_reg_type.IsDouble());
  EXPECT_FALSE(int_reg_type.IsReference());
  EXPECT_FALSE(int_reg_type.IsLowHalf());
  EXPECT_FALSE(int_reg_type.IsHighHalf());
  EXPECT_FALSE(int_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(int_reg_type.IsReferenceTypes());
  EXPECT_TRUE(int_reg_type.IsCategory1Types());
  EXPECT_FALSE(int_reg_type.IsCategory2Types());
  EXPECT_FALSE(int_reg_type.IsBooleanTypes());
  EXPECT_FALSE(int_reg_type.IsByteTypes());
  EXPECT_FALSE(int_reg_type.IsShortTypes());
  EXPECT_FALSE(int_reg_type.IsCharTypes());
  EXPECT_TRUE(int_reg_type.IsIntegralTypes());
  EXPECT_FALSE(int_reg_type.IsFloatTypes());
  EXPECT_FALSE(int_reg_type.IsLongTypes());
  EXPECT_FALSE(int_reg_type.IsDoubleTypes());
  EXPECT_TRUE(int_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(int_reg_type.IsNonZeroReferenceTypes());

  const RegType& long_reg_type = cache.LongLo();
  EXPECT_FALSE(long_reg_type.IsUndefined());
  EXPECT_FALSE(long_reg_type.IsConflict());
  EXPECT_FALSE(long_reg_type.IsZero());
  EXPECT_FALSE(long_reg_type.IsOne());
  EXPECT_FALSE(long_reg_type.IsLongConstant());
  EXPECT_FALSE(long_reg_type.IsBoolean());
  EXPECT_FALSE(long_reg_type.IsByte());
  EXPECT_FALSE(long_reg_type.IsChar());
  EXPECT_FALSE(long_reg_type.IsShort());
  EXPECT_FALSE(long_reg_type.IsInteger());
  EXPECT_TRUE(long_reg_type.IsLong());
  EXPECT_FALSE(long_reg_type.IsFloat());
  EXPECT_FALSE(long_reg_type.IsDouble());
  EXPECT_FALSE(long_reg_type.IsReference());
  EXPECT_TRUE(long_reg_type.IsLowHalf());
  EXPECT_FALSE(long_reg_type.IsHighHalf());
  EXPECT_TRUE(long_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(long_reg_type.IsReferenceTypes());
  EXPECT_FALSE(long_reg_type.IsCategory1Types());
  EXPECT_TRUE(long_reg_type.IsCategory2Types());
  EXPECT_FALSE(long_reg_type.IsBooleanTypes());
  EXPECT_FALSE(long_reg_type.IsByteTypes());
  EXPECT_FALSE(long_reg_type.IsShortTypes());
  EXPECT_FALSE(long_reg_type.IsCharTypes());
  EXPECT_FALSE(long_reg_type.IsIntegralTypes());
  EXPECT_FALSE(long_reg_type.IsFloatTypes());
  EXPECT_TRUE(long_reg_type.IsLongTypes());
  EXPECT_FALSE(long_reg_type.IsDoubleTypes());
  EXPECT_FALSE(long_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(long_reg_type.IsNonZeroReferenceTypes());

  const RegType& float_reg_type = cache.Float();
  EXPECT_FALSE(float_reg_type.IsUndefined());
  EXPECT_FALSE(float_reg_type.IsConflict());
  EXPECT_FALSE(float_reg_type.IsZero());
  EXPECT_FALSE(float_reg_type.IsOne());
  EXPECT_FALSE(float_reg_type.IsLongConstant());
  EXPECT_FALSE(float_reg_type.IsBoolean());
  EXPECT_FALSE(float_reg_type.IsByte());
  EXPECT_FALSE(float_reg_type.IsChar());
  EXPECT_FALSE(float_reg_type.IsShort());
  EXPECT_FALSE(float_reg_type.IsInteger());
  EXPECT_FALSE(float_reg_type.IsLong());
  EXPECT_TRUE(float_reg_type.IsFloat());
  EXPECT_FALSE(float_reg_type.IsDouble());
  EXPECT_FALSE(float_reg_type.IsReference());
  EXPECT_FALSE(float_reg_type.IsLowHalf());
  EXPECT_FALSE(float_reg_type.IsHighHalf());
  EXPECT_FALSE(float_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(float_reg_type.IsReferenceTypes());
  EXPECT_TRUE(float_reg_type.IsCategory1Types());
  EXPECT_FALSE(float_reg_type.IsCategory2Types());
  EXPECT_FALSE(float_reg_type.IsBooleanTypes());
  EXPECT_FALSE(float_reg_type.IsByteTypes());
  EXPECT_FALSE(float_reg_type.IsShortTypes());
  EXPECT_FALSE(float_reg_type.IsCharTypes());
  EXPECT_FALSE(float_reg_type.IsIntegralTypes());
  EXPECT_TRUE(float_reg_type.IsFloatTypes());
  EXPECT_FALSE(float_reg_type.IsLongTypes());
  EXPECT_FALSE(float_reg_type.IsDoubleTypes());
  EXPECT_FALSE(float_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(float_reg_type.IsNonZeroReferenceTypes());

  const RegType& double_reg_type = cache.DoubleLo();
  EXPECT_FALSE(double_reg_type.IsUndefined());
  EXPECT_FALSE(double_reg_type.IsConflict());
  EXPECT_FALSE(double_reg_type.IsZero());
  EXPECT_FALSE(double_reg_type.IsOne());
  EXPECT_FALSE(double_reg_type.IsLongConstant());
  EXPECT_FALSE(double_reg_type.IsBoolean());
  EXPECT_FALSE(double_reg_type.IsByte());
  EXPECT_FALSE(double_reg_type.IsChar());
  EXPECT_FALSE(double_reg_type.IsShort());
  EXPECT_FALSE(double_reg_type.IsInteger());
  EXPECT_FALSE(double_reg_type.IsLong());
  EXPECT_FALSE(double_reg_type.IsFloat());
  EXPECT_TRUE(double_reg_type.IsDouble());
  EXPECT_FALSE(double_reg_type.IsReference());
  EXPECT_TRUE(double_reg_type.IsLowHalf());
  EXPECT_FALSE(double_reg_type.IsHighHalf());
  EXPECT_TRUE(double_reg_type.IsLongOrDoubleTypes());
  EXPECT_FALSE(double_reg_type.IsReferenceTypes());
  EXPECT_FALSE(double_reg_type.IsCategory1Types());
  EXPECT_TRUE(double_reg_type.IsCategory2Types());
  EXPECT_FALSE(double_reg_type.IsBooleanTypes());
  EXPECT_FALSE(double_reg_type.IsByteTypes());
  EXPECT_FALSE(double_reg_type.IsShortTypes());
  EXPECT_FALSE(double_reg_type.IsCharTypes());
  EXPECT_FALSE(double_reg_type.IsIntegralTypes());
  EXPECT_FALSE(double_reg_type.IsFloatTypes());
  EXPECT_FALSE(double_reg_type.IsLongTypes());
  EXPECT_TRUE(double_reg_type.IsDoubleTypes());
  EXPECT_FALSE(double_reg_type.IsArrayIndexTypes());
  EXPECT_FALSE(double_reg_type.IsNonZeroReferenceTypes());
}


class RegTypeReferenceTest : public CommonTest {};

TEST_F(RegTypeReferenceTest, JavalangObjectImprecise) {
  // Tests matching precisions. A reference type that was created precise doesn't
  // match the one that is imprecise.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  const RegType& imprecise_obj = cache.JavaLangObject(false);
  const RegType& precise_obj = cache.JavaLangObject(true);
  const RegType& precise_obj_2 = cache.FromDescriptor(NULL, "Ljava/lang/Object;", true);

  EXPECT_TRUE(precise_obj.Equals(precise_obj_2));
  EXPECT_FALSE(imprecise_obj.Equals(precise_obj));
  EXPECT_FALSE(imprecise_obj.Equals(precise_obj));
  EXPECT_FALSE(imprecise_obj.Equals(precise_obj_2));
}

TEST_F(RegTypeReferenceTest, UnresolvedType) {
  // Tests creating unresolved types. Miss for the first time asking the cache and
  // a hit second time.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  const RegType& ref_type_0 = cache.FromDescriptor(NULL, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.IsUnresolvedReference());
  EXPECT_TRUE(ref_type_0.IsNonZeroReferenceTypes());

  const RegType& ref_type_1 = cache.FromDescriptor(NULL, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.Equals(ref_type_1));

  const RegType& unresolved_super_class =  cache.FromUnresolvedSuperClass(ref_type_0);
  EXPECT_TRUE(unresolved_super_class.IsUnresolvedSuperClass());
  EXPECT_TRUE(unresolved_super_class.IsNonZeroReferenceTypes());
}

TEST_F(RegTypeReferenceTest, UnresolvedUnintializedType) {
  // Tests creating types uninitialized types from unresolved types.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  const RegType& ref_type_0 = cache.FromDescriptor(NULL, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.IsUnresolvedReference());
  const RegType& ref_type = cache.FromDescriptor(NULL, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.Equals(ref_type));
  // Create an uninitialized type of this unresolved type
  const RegType& unresolved_unintialised = cache.Uninitialized(ref_type, 1101ull);
  EXPECT_TRUE(unresolved_unintialised.IsUnresolvedAndUninitializedReference());
  EXPECT_TRUE(unresolved_unintialised.IsUninitializedTypes());
  EXPECT_TRUE(unresolved_unintialised.IsNonZeroReferenceTypes());
  // Create an uninitialized type of this unresolved type with different  PC
  const RegType& ref_type_unresolved_unintialised_1 =  cache.Uninitialized(ref_type, 1102ull);
  EXPECT_TRUE(unresolved_unintialised.IsUnresolvedAndUninitializedReference());
  EXPECT_FALSE(unresolved_unintialised.Equals(ref_type_unresolved_unintialised_1));
  // Create an uninitialized type of this unresolved type with the same PC
  const RegType& unresolved_unintialised_2 = cache.Uninitialized(ref_type, 1101ull);
  EXPECT_TRUE(unresolved_unintialised.Equals(unresolved_unintialised_2));
}

TEST_F(RegTypeReferenceTest, Dump) {
  // Tests types for proper Dump messages.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  const RegType& unresolved_ref = cache.FromDescriptor(NULL, "Ljava/lang/DoesNotExist;", true);
  const RegType& unresolved_ref_another = cache.FromDescriptor(NULL, "Ljava/lang/DoesNotExistEither;", true);
  const RegType& resolved_ref = cache.JavaLangString();
  const RegType& resolved_unintialiesd = cache.Uninitialized(resolved_ref, 10);
  const RegType& unresolved_unintialized = cache.Uninitialized(unresolved_ref, 12);
  const RegType& unresolved_merged = cache.FromUnresolvedMerge(unresolved_ref, unresolved_ref_another);

  std::string expected = "Unresolved Reference: java.lang.DoesNotExist";
  EXPECT_EQ(expected, unresolved_ref.Dump());
  expected = "Precise Reference: java.lang.String";
  EXPECT_EQ(expected, resolved_ref.Dump());
  expected ="Uninitialized Reference: java.lang.String Allocation PC: 10";
  EXPECT_EQ(expected, resolved_unintialiesd.Dump());
  expected = "Unresolved And Uninitialized Reference: java.lang.DoesNotExist Allocation PC: 12";
  EXPECT_EQ(expected, unresolved_unintialized.Dump());
  expected = "UnresolvedMergedReferences(Unresolved Reference: java.lang.DoesNotExist, Unresolved Reference: java.lang.DoesNotExistEither)";
  EXPECT_EQ(expected, unresolved_merged.Dump());
}

TEST_F(RegTypeReferenceTest, JavalangString) {
  // Add a class to the cache then look for the same class and make sure it is  a
  // Hit the second time. Then check for the same effect when using
  // The JavaLangObject method instead of FromDescriptor. String class is final.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  const RegType& ref_type = cache.JavaLangString();
  const RegType& ref_type_2 = cache.JavaLangString();
  const RegType& ref_type_3 = cache.FromDescriptor(NULL, "Ljava/lang/String;", true);

  EXPECT_TRUE(ref_type.Equals(ref_type_2));
  EXPECT_TRUE(ref_type_2.Equals(ref_type_3));
  EXPECT_TRUE(ref_type.IsPreciseReference());

  // Create an uninitialized type out of this:
  const RegType& ref_type_unintialized = cache.Uninitialized(ref_type, 0110ull);
  EXPECT_TRUE(ref_type_unintialized.IsUninitializedReference());
  EXPECT_FALSE(ref_type_unintialized.IsUnresolvedAndUninitializedReference());
}

TEST_F(RegTypeReferenceTest, JavalangObject) {
  // Add a class to the cache then look for the same class and make sure it is  a
  // Hit the second time. Then I am checking for the same effect when using
  // The JavaLangObject method instead of FromDescriptor. Object Class in not final.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache(true);
  const RegType& ref_type = cache.JavaLangObject(true);
  const RegType& ref_type_2 = cache.JavaLangObject(true);
  const RegType& ref_type_3 = cache.FromDescriptor(NULL, "Ljava/lang/Object;", true);

  EXPECT_TRUE(ref_type.Equals(ref_type_2));
  EXPECT_TRUE(ref_type_3.Equals(ref_type_2));
  EXPECT_EQ(ref_type.GetId(), ref_type_3.GetId());
}
TEST_F(RegTypeReferenceTest, Merging) {
  // Tests merging logic
  // String and object , LUB is object.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache_new(true);
  const RegType& string = cache_new.JavaLangString();
  const RegType& Object = cache_new.JavaLangObject(true);
  EXPECT_TRUE(string.Merge(Object, &cache_new).IsJavaLangObject());
  // Merge two unresolved types.
  const RegType& ref_type_0 = cache_new.FromDescriptor(NULL, "Ljava/lang/DoesNotExist;", true);
  EXPECT_TRUE(ref_type_0.IsUnresolvedReference());
  const RegType& ref_type_1 = cache_new.FromDescriptor(NULL, "Ljava/lang/DoesNotExistToo;", true);
  EXPECT_FALSE(ref_type_0.Equals(ref_type_1));

  const RegType& merged = ref_type_1.Merge(ref_type_0, &cache_new);
  EXPECT_TRUE(merged.IsUnresolvedMergedReference());
  RegType& merged_nonconst = const_cast<RegType&>(merged);

  std::set<uint16_t> merged_ids = (down_cast<UnresolvedMergedType*>(&merged_nonconst))->GetMergedTypes();
  EXPECT_EQ(ref_type_0.GetId(), *(merged_ids.begin()));
  EXPECT_EQ(ref_type_1.GetId(), *((++merged_ids.begin())));
}


TEST_F(RegTypeTest, ConstPrecision) {
  // Tests creating primitive types types.
  ScopedObjectAccess soa(Thread::Current());
  RegTypeCache cache_new(true);
  const RegType& imprecise_const = cache_new.FromCat1Const(10, false);
  const RegType& precise_const = cache_new.FromCat1Const(10, true);

  EXPECT_TRUE(imprecise_const.IsImpreciseConstant());
  EXPECT_TRUE(precise_const.IsPreciseConstant());
  EXPECT_FALSE(imprecise_const.Equals(precise_const));
}

}  // namespace verifier
}  // namespace art
