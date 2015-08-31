/*
 * Copyright (C) 2006 The Android Open Source Project
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

/**
 * Test arithmetic operations.
 */
public class IntMath {

    static void shiftTest1() {
        System.out.println("IntMath.shiftTest1");

        final int[] mBytes = {
            0x11, 0x22, 0x33, 0x44, 0x88, 0x99, 0xaa, 0xbb
        };
        long l;
        int i1, i2;

        i1 = mBytes[0] | mBytes[1] << 8 | mBytes[2] << 16 | mBytes[3] << 24;
        i2 = mBytes[4] | mBytes[5] << 8 | mBytes[6] << 16 | mBytes[7] << 24;
        l = i1 | ((long)i2 << 32);

        Main.assertTrue(i1 == 0x44332211);
        Main.assertTrue(i2 == 0xbbaa9988);
        Main.assertTrue(l == 0xbbaa998844332211L);

        l = (long)mBytes[0]
            | (long)mBytes[1] << 8
            | (long)mBytes[2] << 16
            | (long)mBytes[3] << 24
            | (long)mBytes[4] << 32
            | (long)mBytes[5] << 40
            | (long)mBytes[6] << 48
            | (long)mBytes[7] << 56;

        Main.assertTrue(l == 0xbbaa998844332211L);
    }

    static void shiftTest2() {
        System.out.println("IntMath.shiftTest2");

        long    a = 0x11;
        long    b = 0x22;
        long    c = 0x33;
        long    d = 0x44;
        long    e = 0x55;
        long    f = 0x66;
        long    g = 0x77;
        long    h = 0x88;

        long    result = ((a << 56) | (b << 48) | (c << 40) | (d << 32) |
                         (e << 24) | (f << 16) | (g <<  8) | h);

        Main.assertTrue(result == 0x1122334455667788L);
    }

    static void unsignedShiftTest() {
        System.out.println("IntMath.unsignedShiftTest");

        byte b = -4;
        short s = -4;
        char c = 0xfffc;
        int i = -4;

        b >>>= 4;
        s >>>= 4;
        c >>>= 4;
        i >>>= 4;

        Main.assertTrue((int) b == -1);
        Main.assertTrue((int) s == -1);
        Main.assertTrue((int) c == 0x0fff);
        Main.assertTrue(i == 268435455);
    }

    static void shiftTest3(int thirtyTwo) {
        System.out.println("IntMath.shiftTest3");

        int one = thirtyTwo / 32;
        int sixteen = thirtyTwo / 2;
        int thirtyThree = thirtyTwo + 1;
        int sixtyFour = thirtyTwo * 2;

        Main.assertTrue(1 << thirtyTwo == 1);
        Main.assertTrue((1 << sixteen) << sixteen == 0);
        Main.assertTrue(1 << thirtyThree == 2);
        Main.assertTrue(1 << -one == -2147483648);
        Main.assertTrue(1 << -thirtyTwo == 1);
        Main.assertTrue(1 << -thirtyThree == -2147483648);
        Main.assertTrue(1 << thirtyThree == 2);

        Main.assertTrue(1 >> thirtyTwo == 1);
        Main.assertTrue((1 >> sixteen) >> sixteen == 0);
        Main.assertTrue(1 >> thirtyThree == 0);
        Main.assertTrue(1 >> -one == 0);
        Main.assertTrue(1 >> -thirtyTwo == 1);
        Main.assertTrue(1 >> -thirtyThree == 0);
        Main.assertTrue(-4 >> thirtyThree == -2);

        Main.assertTrue(1 >>> thirtyTwo == 1);
        Main.assertTrue((1 >>> sixteen) >>> sixteen == 0);
        Main.assertTrue(1 >>> thirtyThree == 0);
        Main.assertTrue(1 >>> -one == 0);
        Main.assertTrue(1 >>> -thirtyTwo == 1);
        Main.assertTrue(1 >>> -thirtyThree == 0);
        Main.assertTrue(-4 >>> thirtyThree == 2147483646);
    }

    static void convTest() {
        System.out.println("IntMath.convTest");

        float f;
        double d;
        int i;
        long l;

        /* int --> long */
        i = 7654;
        l = (long) i;
        Main.assertTrue(l == 7654L);

        i = -7654;
        l = (long) i;
        Main.assertTrue(l == -7654L);

        /* long --> int (with truncation) */
        l = 5678956789L;
        i = (int) l;
        Main.assertTrue(i == 1383989493);

        l = -5678956789L;
        i = (int) l;
        Main.assertTrue(i == -1383989493);
    }

    static void charSubTest() {
        System.out.println("IntMath.charSubTest");

        char char1 = 0x00e9;
        char char2 = 0xffff;
        int i;

        /* chars are unsigned-expanded to ints before subtraction */
        i = char1 - char2;
        Main.assertTrue(i == 0xffff00ea);
    }

    /*
     * We pass in the arguments and return the results so the compiler
     * doesn't do the math for us.  (x=70000, y=-3)
     */
    static int[] intOperTest(int x, int y) {
        System.out.println("IntMath.intOperTest");

        int[] results = new int[10];

        /* this seems to generate "op-int" instructions */
        results[0] = x + y;
        results[1] = x - y;
        results[2] = x * y;
        results[3] = x * x;
        results[4] = x / y;
        results[5] = x % -y;
        results[6] = x & y;
        results[7] = x | y;
        results[8] = x ^ y;

        /* this seems to generate "op-int/2addr" instructions */
        results[9] = x + ((((((((x + y) - y) * y) / y) % y) & y) | y) ^ y);

        return results;
    }
    static void intOperCheck(int[] results) {
        System.out.println("IntMath.intOperCheck");

        /* check this edge case while we're here (div-int/2addr) */
        int minInt = -2147483648;
        int negOne = -results[5];
        int plusOne = 1;
        int result = (((minInt + plusOne) - plusOne) / negOne) / negOne;
        Main.assertTrue(result == minInt);

        Main.assertTrue(results[0] == 69997);
        Main.assertTrue(results[1] == 70003);
        Main.assertTrue(results[2] == -210000);
        Main.assertTrue(results[3] == 605032704);    // overflow / truncate
        Main.assertTrue(results[4] == -23333);
        Main.assertTrue(results[5] == 1);
        Main.assertTrue(results[6] == 70000);
        Main.assertTrue(results[7] == -3);
        Main.assertTrue(results[8] == -70003);
        Main.assertTrue(results[9] == 70000);
    }

    /*
     * More operations, this time with 16-bit constants.  (x=77777)
     */
    static int[] lit16Test(int x) {
        System.out.println("IntMath.lit16Test");

        int[] results = new int[8];

        /* try to generate op-int/lit16" instructions */
        results[0] = x + 1000;
        results[1] = 1000 - x;
        results[2] = x * 1000;
        results[3] = x / 1000;
        results[4] = x % 1000;
        results[5] = x & 1000;
        results[6] = x | -1000;
        results[7] = x ^ -1000;
        return results;
    }
    static void lit16Check(int[] results) {
        Main.assertTrue(results[0] == 78777);
        Main.assertTrue(results[1] == -76777);
        Main.assertTrue(results[2] == 77777000);
        Main.assertTrue(results[3] == 77);
        Main.assertTrue(results[4] == 777);
        Main.assertTrue(results[5] == 960);
        Main.assertTrue(results[6] == -39);
        Main.assertTrue(results[7] == -76855);
    }

    /*
     * More operations, this time with 8-bit constants.  (x=-55555)
     */
    static int[] lit8Test(int x) {
        System.out.println("IntMath.lit8Test");

        int[] results = new int[8];

        /* try to generate op-int/lit8" instructions */
        results[0] = x + 10;
        results[1] = 10 - x;
        results[2] = x * 10;
        results[3] = x / 10;
        results[4] = x % 10;
        results[5] = x & 10;
        results[6] = x | -10;
        results[7] = x ^ -10;
        return results;
    }
    static void lit8Check(int[] results) {
        //for (int i = 0; i < results.length; i++)
        //    System.out.println(" " + i + ": " + results[i]);

        /* check this edge case while we're here (div-int/lit8) */
        int minInt = -2147483648;
        int result = minInt / -1;
        Main.assertTrue(result == minInt);

        Main.assertTrue(results[0] == -55545);
        Main.assertTrue(results[1] == 55565);
        Main.assertTrue(results[2] == -555550);
        Main.assertTrue(results[3] == -5555);
        Main.assertTrue(results[4] == -5);
        Main.assertTrue(results[5] == 8);
        Main.assertTrue(results[6] == -1);
        Main.assertTrue(results[7] == 55563);
    }

    /*
     * Make sure special-cased literal division matches
     * normal division.
     */
    static void divLiteralTestBody(int start, int count) {
       int normal = 0;
       int special = 0;
       for (int i = 0; i < count; i++) {
           for (int j = 3; j < 16; j++) {
               switch(j) {
                   case 3:
                       normal = (start+i) / j;
                       special = (start+i) / 3;
                       break;
                   case 4:
                       normal = (start+i) / j;
                       special = (start+i) / 4;
                       break;
                   case 5:
                       normal = (start+i) / j;
                       special = (start+i) / 5;
                       break;
                   case 6:
                       normal = (start+i) / j;
                       special = (start+i) / 6;
                       break;
                   case 7:
                       normal = (start+i) / j;
                       special = (start+i) / 7;
                       break;
                   case 8:
                       normal = (start+i) / j;
                       special = (start+i) / 8;
                       break;
                   case 9:
                       normal = (start+i) / j;
                       special = (start+i) / 9;
                       break;
                   case 10:
                       normal = (start+i) / j;
                       special = (start+i) / 10;
                       break;
                   case 11:
                       normal = (start+i) / j;
                       special = (start+i) / 11;
                       break;
                   case 12:
                       normal = (start+i) / j;
                       special = (start+i) / 12;
                       break;
                   case 13:
                       normal = (start+i) / j;
                       special = (start+i) / 13;
                       break;
                   case 14:
                       normal = (start+i) / j;
                       special = (start+i) / 14;
                       break;
                   case 15:
                       normal = (start+i) / j;
                       special = (start+i) / 15;
                       break;
               }
           }
           Main.assertTrue(normal == special);
       }
    }

    static void divLiteralTest() {
       System.out.println("IntMath.divLiteralTest");
       divLiteralTestBody(-1000, 2000);
       divLiteralTestBody(0x7fffffff-2000, 2000);
       divLiteralTestBody(0xfff0ffff, 2000);
    }

    /*
     * Shift some data.  (value=0xff00aa01, dist=8)
     */
    static int[] intShiftTest(int value, int dist) {
        System.out.println("IntMath.intShiftTest");

        int results[] = new int[4];

        results[0] = value << dist;
        results[1] = value >> dist;
        results[2] = value >>> dist;

        results[3] = (((value << dist) >> dist) >>> dist) << dist;
        return results;
    }
    static void intShiftCheck(int[] results) {
        System.out.println("IntMath.intShiftCheck");

        Main.assertTrue(results[0] == 0x00aa0100);
        Main.assertTrue(results[1] == 0xffff00aa);
        Main.assertTrue(results[2] == 0x00ff00aa);
        Main.assertTrue(results[3] == 0xaa00);
    }

    /*
     * We pass in the arguments and return the results so the compiler
     * doesn't do the math for us.  (x=70000000000, y=-3)
     */
    static long[] longOperTest(long x, long y) {
        System.out.println("IntMath.longOperTest");

        long[] results = new long[10];

        /* this seems to generate "op-long" instructions */
        results[0] = x + y;
        results[1] = x - y;
        results[2] = x * y;
        results[3] = x * x;
        results[4] = x / y;
        results[5] = x % -y;
        results[6] = x & y;
        results[7] = x | y;
        results[8] = x ^ y;

        /* this seems to generate "op-long/2addr" instructions */
        results[9] = x + ((((((((x + y) - y) * y) / y) % y) & y) | y) ^ y);

        return results;
    }
    static void longOperCheck(long[] results) {
        System.out.println("IntMath.longOperCheck");

        /* check this edge case while we're here (div-long/2addr) */
        long minLong = -9223372036854775808L;
        long negOne = -results[5];
        long plusOne = 1;
        long result = (((minLong + plusOne) - plusOne) / negOne) / negOne;
        Main.assertTrue(result == minLong);

        Main.assertTrue(results[0] == 69999999997L);
        Main.assertTrue(results[1] == 70000000003L);
        Main.assertTrue(results[2] == -210000000000L);
        Main.assertTrue(results[3] == -6833923606740729856L);    // overflow
        Main.assertTrue(results[4] == -23333333333L);
        Main.assertTrue(results[5] == 1);
        Main.assertTrue(results[6] == 70000000000L);
        Main.assertTrue(results[7] == -3);
        Main.assertTrue(results[8] == -70000000003L);
        Main.assertTrue(results[9] == 70000000000L);

        Main.assertTrue(results.length == 10);
    }

    /*
     * Shift some data.  (value=0xd5aa96deff00aa01, dist=8)
     */
    static long[] longShiftTest(long value, int dist) {
        System.out.println("IntMath.longShiftTest");

        long results[] = new long[4];

        results[0] = value << dist;
        results[1] = value >> dist;
        results[2] = value >>> dist;

        results[3] = (((value << dist) >> dist) >>> dist) << dist;
        return results;
    }
    static long longShiftCheck(long[] results) {
        System.out.println("IntMath.longShiftCheck");

        Main.assertTrue(results[0] == 0x96deff00aa010000L);
        Main.assertTrue(results[1] == 0xffffd5aa96deff00L);
        Main.assertTrue(results[2] == 0x0000d5aa96deff00L);
        Main.assertTrue(results[3] == 0xffff96deff000000L);

        Main.assertTrue(results.length == 4);

        return results[0];      // test return-long
    }


    /*
     * Try to cause some unary operations.
     */
    static int unopTest(int x) {
        x = -x;
        x ^= 0xffffffff;
        return x;
    }
    static void unopCheck(int result) {
        Main.assertTrue(result == 37);
    }

    static class Shorty {
        public short mShort;
        public char mChar;
        public byte mByte;
    };

    /*
     * Truncate an int.
     */
    static Shorty truncateTest(int x) {
        System.out.println("IntMath.truncateTest");
        Shorty shorts = new Shorty();

        shorts.mShort = (short) x;
        shorts.mChar = (char) x;
        shorts.mByte = (byte) x;
        return shorts;
    }
    static void truncateCheck(Shorty shorts) {
        Main.assertTrue(shorts.mShort == -5597);     // 0xea23
        Main.assertTrue(shorts.mChar == 59939);      // 0xea23
        Main.assertTrue(shorts.mByte == 35);         // 0x23
    }

    /*
     * Verify that we get a divide-by-zero exception.
     */
    static void divideByZero(int z) {
        System.out.println("IntMath.divideByZero");

        try {
            int x = 100 / z;
            Main.assertTrue(false);
        } catch (ArithmeticException ae) {
        }

        try {
            int x = 100 % z;
            Main.assertTrue(false);
        } catch (ArithmeticException ae) {
        }

        try {
            long x = 100L / z;
            Main.assertTrue(false);
        } catch (ArithmeticException ae) {
        }

        try {
            long x = 100L % z;
            Main.assertTrue(false);
        } catch (ArithmeticException ae) {
        }
    }

    /*
     * Check an edge condition: dividing the most-negative integer by -1
     * returns the most-negative integer, and doesn't cause an exception.
     *
     * Pass in -1, -1L.
     */
    static void bigDivideOverflow(int idiv, long ldiv) {
        System.out.println("IntMath.bigDivideOverflow");
        int mostNegInt = (int) 0x80000000;
        long mostNegLong = (long) 0x8000000000000000L;

        int intDivResult = mostNegInt / idiv;
        int intModResult = mostNegInt % idiv;
        long longDivResult = mostNegLong / ldiv;
        long longModResult = mostNegLong % ldiv;

        Main.assertTrue(intDivResult == mostNegInt);
        Main.assertTrue(intModResult == 0);
        Main.assertTrue(longDivResult == mostNegLong);
        Main.assertTrue(longModResult == 0);
    }

    /*
     * Check "const" instructions.  We use negative values to ensure that
     * sign-extension is happening.
     */
    static void checkConsts(byte small, short medium, int large, long huge) {
        System.out.println("IntMath.checkConsts");

        Main.assertTrue(small == 1);                     // const/4
        Main.assertTrue(medium == -256);                 // const/16
        Main.assertTrue(medium == -256L);                // const-wide/16
        Main.assertTrue(large == -88888);                // const
        Main.assertTrue(large == -88888L);               // const-wide/32
        Main.assertTrue(huge == 0x9922334455667788L);    // const-wide
    }

    /*
     * Test some java.lang.Math functions.
     *
     * The method arguments are positive values.
     */
    static void jlmTests(int ii, long ll) {
        System.out.println("IntMath.jlmTests");

        Main.assertTrue(Math.abs(ii) == ii);
        Main.assertTrue(Math.abs(-ii) == ii);
        Main.assertTrue(Math.min(ii, -5) == -5);
        Main.assertTrue(Math.max(ii, -5) == ii);

        Main.assertTrue(Math.abs(ll) == ll);
        Main.assertTrue(Math.abs(-ll) == ll);
        Main.assertTrue(Math.min(ll, -5L) == -5L);
        Main.assertTrue(Math.max(ll, -5L) == ll);
    }

    public static void run() {
        shiftTest1();
        shiftTest2();
        unsignedShiftTest();
        shiftTest3(32);
        convTest();
        charSubTest();

        int[] intResults;
        long[] longResults;

        intResults = intOperTest(70000, -3);
        intOperCheck(intResults);
        longResults = longOperTest(70000000000L, -3L);
        longOperCheck(longResults);

        intResults = lit16Test(77777);
        lit16Check(intResults);
        intResults = lit8Test(-55555);
        lit8Check(intResults);
        divLiteralTest();

        intResults = intShiftTest(0xff00aa01, 8);
        intShiftCheck(intResults);
        longResults = longShiftTest(0xd5aa96deff00aa01L, 16);
        long longRet = longShiftCheck(longResults);
        Main.assertTrue(longRet == 0x96deff00aa010000L);

        Shorty shorts = truncateTest(-16717277);    // 0xff00ea23
        truncateCheck(shorts);

        divideByZero(0);
        bigDivideOverflow(-1, -1L);

        checkConsts((byte) 1, (short) -256, -88888, 0x9922334455667788L);

        unopCheck(unopTest(38));

        jlmTests(12345, 0x1122334455667788L);
    }
}
