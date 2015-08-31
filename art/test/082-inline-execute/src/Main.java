/*
 * Copyright (C) 2007 The Android Open Source Project
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

import junit.framework.Assert;

public class Main {
  public static void main(String args[]) {
    test_Double_doubleToRawLongBits();
    test_Double_longBitsToDouble();
    test_Float_floatToRawIntBits();
    test_Float_intBitsToFloat();
    test_Math_abs_I();
    test_Math_abs_J();
    test_Math_min();
    test_Math_max();
    test_StrictMath_abs_I();
    test_StrictMath_abs_J();
    test_StrictMath_min();
    test_StrictMath_max();
    test_String_charAt();
    test_String_compareTo();
    test_String_indexOf();
    test_String_isEmpty();
    test_String_length();
  }

  public static void test_String_length() {
    String str0 = "";
    String str1 = "x";
    String str80 = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";

    Assert.assertEquals(str0.length(), 0);
    Assert.assertEquals(str1.length(), 1);
    Assert.assertEquals(str80.length(), 80);

    String strNull = null;
    try {
      strNull.length();
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  public static void test_String_isEmpty() {
    String str0 = "";
    String str1 = "x";

    Assert.assertTrue(str0.isEmpty());
    Assert.assertFalse(str1.isEmpty());

    String strNull = null;
    try {
      strNull.isEmpty();
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  public static void test_String_charAt() {
    String testStr = "Now is the time";

    Assert.assertEquals('N', testStr.charAt(0));
    Assert.assertEquals('o', testStr.charAt(1));
    Assert.assertEquals(' ', testStr.charAt(10));
    Assert.assertEquals('e', testStr.charAt(testStr.length()-1));

    try {
      testStr.charAt(-1);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }
    try {
      testStr.charAt(80);
      Assert.fail();
    } catch (StringIndexOutOfBoundsException expected) {
    }

    String strNull = null;
    try {
      strNull.charAt(0);
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  public static void test_String_indexOf() {
    String str0 = "";
    String str3 = "abc";
    String str10 = "abcdefghij";
    String str40 = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabc";

    int supplementaryChar = 0x20b9f;
    String surrogatePair = "\ud842\udf9f";
    String stringWithSurrogates = "hello " + surrogatePair + " world";

    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar), "hello ".length());
    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar, 2), "hello ".length());
    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar, 6), 6);
    Assert.assertEquals(stringWithSurrogates.indexOf(supplementaryChar, 7), -1);

    Assert.assertEquals(str0.indexOf('a'), -1);
    Assert.assertEquals(str3.indexOf('a'), 0);
    Assert.assertEquals(str3.indexOf('b'), 1);
    Assert.assertEquals(str3.indexOf('c'), 2);
    Assert.assertEquals(str10.indexOf('j'), 9);
    Assert.assertEquals(str40.indexOf('a'), 0);
    Assert.assertEquals(str40.indexOf('b'), 38);
    Assert.assertEquals(str40.indexOf('c'), 39);
    Assert.assertEquals(str0.indexOf('a',20), -1);
    Assert.assertEquals(str0.indexOf('a',0), -1);
    Assert.assertEquals(str0.indexOf('a',-1), -1);
    Assert.assertEquals(str3.indexOf('a',0), 0);
    Assert.assertEquals(str3.indexOf('a',1), -1);
    Assert.assertEquals(str3.indexOf('a',1234), -1);
    Assert.assertEquals(str3.indexOf('b',0), 1);
    Assert.assertEquals(str3.indexOf('b',1), 1);
    Assert.assertEquals(str3.indexOf('c',2), 2);
    Assert.assertEquals(str10.indexOf('j',5), 9);
    Assert.assertEquals(str10.indexOf('j',9), 9);
    Assert.assertEquals(str40.indexOf('a',10), 10);
    Assert.assertEquals(str40.indexOf('b',40), -1);

    String strNull = null;
    try {
      strNull.indexOf('a');
      Assert.fail();
    } catch (NullPointerException expected) {
    }
    try {
      strNull.indexOf('a', 0);
      Assert.fail();
    } catch (NullPointerException expected) {
    }
    try {
      strNull.indexOf('a', -1);
      Assert.fail();
    } catch (NullPointerException expected) {
    }
  }

  public static void test_String_compareTo() {
    String test = "0123456789";
    String test1 = new String("0123456789");    // different object
    String test2 = new String("0123456780");    // different value
    String offset = new String("xxx0123456789yyy");
    String sub = offset.substring(3, 13);
    String str32 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    String str33 = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxy";
    String lc = "abcdefg";
    String uc = "ABCDEFG";
    Object blah = new Object();

    Assert.assertTrue(lc.toUpperCase().equals(uc));

    Assert.assertEquals(str32.compareTo(str33), -1);
    Assert.assertEquals(str33.compareTo(str32), 1);

    Assert.assertTrue(test.equals(test));
    Assert.assertTrue(test.equals(test1));
    Assert.assertFalse(test.equals(test2));

    Assert.assertEquals(test.compareTo(test1), 0);
    Assert.assertTrue(test1.compareTo(test2) > 0);
    Assert.assertTrue(test2.compareTo(test1) < 0);

    // Compare string with a nonzero offset, in left/right side.
    Assert.assertEquals(test.compareTo(sub), 0);
    Assert.assertEquals(sub.compareTo(test), 0);
    Assert.assertTrue(test.equals(sub));
    Assert.assertTrue(sub.equals(test));
    // Same base, one is a substring.
    Assert.assertFalse(offset.equals(sub));
    Assert.assertFalse(sub.equals(offset));
    // Wrong class.
    Assert.assertFalse(test.equals(blah));

    // Null lhs - throw.
    try {
      test.compareTo(null);
      Assert.fail("didn't get expected npe");
    } catch (NullPointerException npe) {
    }
    // Null rhs - okay.
    Assert.assertFalse(test.equals(null));

    test = test.substring(1);
    Assert.assertTrue(test.equals("123456789"));
    Assert.assertFalse(test.equals(test1));

    test = test.substring(1);
    Assert.assertTrue(test.equals("23456789"));

    test = test.substring(1);
    Assert.assertTrue(test.equals("3456789"));

    test = test.substring(1);
    Assert.assertTrue(test.equals("456789"));

    test = test.substring(3,5);
    Assert.assertTrue(test.equals("78"));

    test = "this/is/a/path";
    String[] strings = test.split("/");
    Assert.assertEquals(4, strings.length);

    Assert.assertEquals("this is a path", test.replaceAll("/", " "));
    Assert.assertEquals("this is a path", test.replace("/", " "));
  }

  public static void test_Math_abs_I() {
    Assert.assertEquals(Math.abs(0), 0);
    Assert.assertEquals(Math.abs(123), 123);
    Assert.assertEquals(Math.abs(-123), 123);
    Assert.assertEquals(Math.abs(Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(Math.abs(Integer.MIN_VALUE), Integer.MIN_VALUE);
    Assert.assertEquals(Math.abs(Integer.MIN_VALUE - 1), Integer.MAX_VALUE);
    Assert.assertEquals(Math.abs(Integer.MIN_VALUE + 1), Integer.MAX_VALUE);
  }

  public static void test_Math_abs_J() {
    Assert.assertEquals(Math.abs(0L), 0L);
    Assert.assertEquals(Math.abs(123L), 123L);
    Assert.assertEquals(Math.abs(-123L), 123L);
    Assert.assertEquals(Math.abs(Long.MAX_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(Math.abs(Long.MIN_VALUE), Long.MIN_VALUE);
    Assert.assertEquals(Math.abs(Long.MIN_VALUE - 1), Long.MAX_VALUE);
  }

  public static void test_Math_min() {
    Assert.assertEquals(Math.min(0, 0), 0);
    Assert.assertEquals(Math.min(1, 0), 0);
    Assert.assertEquals(Math.min(0, 1), 0);
    Assert.assertEquals(Math.min(0, Integer.MAX_VALUE), 0);
    Assert.assertEquals(Math.min(Integer.MIN_VALUE, 0), Integer.MIN_VALUE);
    Assert.assertEquals(Math.min(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MIN_VALUE);
  }

  public static void test_Math_max() {
    Assert.assertEquals(Math.max(0, 0), 0);
    Assert.assertEquals(Math.max(1, 0), 1);
    Assert.assertEquals(Math.max(0, 1), 1);
    Assert.assertEquals(Math.max(0, Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(Math.max(Integer.MIN_VALUE, 0), 0);
    Assert.assertEquals(Math.max(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MAX_VALUE);
  }

  public static void test_StrictMath_abs_I() {
    Assert.assertEquals(StrictMath.abs(0), 0);
    Assert.assertEquals(StrictMath.abs(123), 123);
    Assert.assertEquals(StrictMath.abs(-123), 123);
    Assert.assertEquals(StrictMath.abs(Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.abs(Integer.MIN_VALUE), Integer.MIN_VALUE);
    Assert.assertEquals(StrictMath.abs(Integer.MIN_VALUE - 1), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.abs(Integer.MIN_VALUE + 1), Integer.MAX_VALUE);
  }

  public static void test_StrictMath_abs_J() {
    Assert.assertEquals(StrictMath.abs(0L), 0L);
    Assert.assertEquals(StrictMath.abs(123L), 123L);
    Assert.assertEquals(StrictMath.abs(-123L), 123L);
    Assert.assertEquals(StrictMath.abs(Long.MAX_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(StrictMath.abs(Long.MIN_VALUE), Long.MIN_VALUE);
    Assert.assertEquals(StrictMath.abs(Long.MIN_VALUE - 1), Long.MAX_VALUE);
  }

  public static void test_StrictMath_min() {
    Assert.assertEquals(StrictMath.min(0, 0), 0);
    Assert.assertEquals(StrictMath.min(1, 0), 0);
    Assert.assertEquals(StrictMath.min(0, 1), 0);
    Assert.assertEquals(StrictMath.min(0, Integer.MAX_VALUE), 0);
    Assert.assertEquals(StrictMath.min(Integer.MIN_VALUE, 0), Integer.MIN_VALUE);
    Assert.assertEquals(StrictMath.min(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MIN_VALUE);
  }

  public static void test_StrictMath_max() {
    Assert.assertEquals(StrictMath.max(0, 0), 0);
    Assert.assertEquals(StrictMath.max(1, 0), 1);
    Assert.assertEquals(StrictMath.max(0, 1), 1);
    Assert.assertEquals(StrictMath.max(0, Integer.MAX_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(StrictMath.max(Integer.MIN_VALUE, 0), 0);
    Assert.assertEquals(StrictMath.max(Integer.MIN_VALUE, Integer.MAX_VALUE), Integer.MAX_VALUE);
  }

  public static void test_Float_floatToRawIntBits() {
    Assert.assertEquals(Float.floatToRawIntBits(-1.0f), 0xbf800000);
    Assert.assertEquals(Float.floatToRawIntBits(0.0f), 0);
    Assert.assertEquals(Float.floatToRawIntBits(1.0f), 0x3f800000);
    Assert.assertEquals(Float.floatToRawIntBits(Float.NaN), 0x7fc00000);
    Assert.assertEquals(Float.floatToRawIntBits(Float.POSITIVE_INFINITY), 0x7f800000);
    Assert.assertEquals(Float.floatToRawIntBits(Float.NEGATIVE_INFINITY), 0xff800000);
  }

  public static void test_Float_intBitsToFloat() {
    Assert.assertEquals(Float.intBitsToFloat(0xbf800000), -1.0f);
    Assert.assertEquals(Float.intBitsToFloat(0x00000000), 0.0f);
    Assert.assertEquals(Float.intBitsToFloat(0x3f800000), 1.0f);
    Assert.assertEquals(Float.intBitsToFloat(0x7fc00000), Float.NaN);
    Assert.assertEquals(Float.intBitsToFloat(0x7f800000), Float.POSITIVE_INFINITY);
    Assert.assertEquals(Float.intBitsToFloat(0xff800000), Float.NEGATIVE_INFINITY);
  }

  public static void test_Double_doubleToRawLongBits() {
    Assert.assertEquals(Double.doubleToRawLongBits(-1.0), 0xbff0000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(0.0), 0x0000000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(1.0), 0x3ff0000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(Double.NaN), 0x7ff8000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(Double.POSITIVE_INFINITY), 0x7ff0000000000000L);
    Assert.assertEquals(Double.doubleToRawLongBits(Double.NEGATIVE_INFINITY), 0xfff0000000000000L);
  }

  public static void test_Double_longBitsToDouble() {
    Assert.assertEquals(Double.longBitsToDouble(0xbff0000000000000L), -1.0);
    Assert.assertEquals(Double.longBitsToDouble(0x0000000000000000L), 0.0);
    Assert.assertEquals(Double.longBitsToDouble(0x3ff0000000000000L), 1.0);
    Assert.assertEquals(Double.longBitsToDouble(0x7ff8000000000000L), Double.NaN);
    Assert.assertEquals(Double.longBitsToDouble(0x7ff0000000000000L), Double.POSITIVE_INFINITY);
    Assert.assertEquals(Double.longBitsToDouble(0xfff0000000000000L), Double.NEGATIVE_INFINITY);
  }
}
