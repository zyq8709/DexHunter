/*
 * Copyright (C) 2008 The Android Open Source Project
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
 * Exercise arrays.
 */
public class Array {

    /*
     * Verify array contents.
     */
    static void checkBytes(byte[] bytes) {
        Main.assertTrue(bytes[0] == 0);
        Main.assertTrue(bytes[1] == -1);
        Main.assertTrue(bytes[2] == -2);
        Main.assertTrue(bytes[3] == -3);
        Main.assertTrue(bytes[4] == -4);
    }
    static void checkShorts(short[] shorts) {
        Main.assertTrue(shorts[0] == 20);
        Main.assertTrue(shorts[1] == 10);
        Main.assertTrue(shorts[2] == 0);
        Main.assertTrue(shorts[3] == -10);
        Main.assertTrue(shorts[4] == -20);
    }
    static void checkChars(char[] chars) {
        Main.assertTrue(chars[0] == 40000);
        Main.assertTrue(chars[1] == 40001);
        Main.assertTrue(chars[2] == 40002);
        Main.assertTrue(chars[3] == 40003);
        Main.assertTrue(chars[4] == 40004);
    }
    static void checkInts(int[] ints) {
        Main.assertTrue(ints[0] == 70000);
        Main.assertTrue(ints[1] == 70001);
        Main.assertTrue(ints[2] == 70002);
        Main.assertTrue(ints[3] == 70003);
        Main.assertTrue(ints[4] == 70004);
    }
    static void checkBooleans(boolean[] booleans) {
        Main.assertTrue(booleans[0]);
        Main.assertTrue(booleans[1]);
        Main.assertTrue(!booleans[2]);
        Main.assertTrue(booleans[3]);
        Main.assertTrue(!booleans[4]);
    }
    static void checkFloats(float[] floats) {
        Main.assertTrue(floats[0] == -1.5);
        Main.assertTrue(floats[1] == -0.5);
        Main.assertTrue(floats[2] == 0.0);
        Main.assertTrue(floats[3] == 0.5);
        Main.assertTrue(floats[4] == 1.5);
    }
    static void checkLongs(long[] longs) {
        Main.assertTrue(longs[0] == 0x1122334455667788L);
        Main.assertTrue(longs[1] == 0x8877665544332211L);
        Main.assertTrue(longs[2] == 0L);
        Main.assertTrue(longs[3] == 1L);
        Main.assertTrue(longs[4] == -1L);
    }
    static void checkStrings(String[] strings) {
        Main.assertTrue(strings[0].equals("zero"));
        Main.assertTrue(strings[1].equals("one"));
        Main.assertTrue(strings[2].equals("two"));
        Main.assertTrue(strings[3].equals("three"));
        Main.assertTrue(strings[4].equals("four"));
    }

    /*
     * Try bad range values, 32 bit get/put.
     */
    static void checkRange32(int[] ints, int[] empty, int negVal1, int negVal2) {
        System.out.println("Array.checkRange32");
        int i = 0;

        Main.assertTrue(ints.length == 5);

        try {
            i = ints[5];            // exact bound
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            ints[5] = i;            // exact bound
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            i = ints[6];            // one past
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            i = ints[negVal1];      // -1
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            ints[negVal1] = i;      // -1
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            i = ints[negVal2];      // min int
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }


        try {
            i = empty[1];
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
    }

    /*
     * Try bad range values, 64 bit get/put.
     */
    static void checkRange64(long[] longs, int negVal1, int negVal2) {
        System.out.println("Array.checkRange64");
        long l = 0L;

        Main.assertTrue(longs.length == 5);

        try {
            l = longs[5];            // exact bound
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            longs[5] = l;            // exact bound
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            l = longs[6];            // one past
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            l = longs[negVal1];      // -1
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            longs[negVal1] = l;      // -1
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
        try {
            l = longs[negVal2];      // min int
            Main.assertTrue(false);
        } catch (ArrayIndexOutOfBoundsException aioobe) {
            // good
        }
    }

    /*
     * Test negative allocations of object and primitive arrays.
     */
    static void checkNegAlloc(int count) {
        System.out.println("Array.checkNegAlloc");
        String[] strings;
        int[] ints;

        try {
            ints = new int[count];
            Main.assertTrue(false);
        } catch (NegativeArraySizeException nase) {
            // good
        }

        try {
            strings = new String[count];
            Main.assertTrue(false);
        } catch (NegativeArraySizeException nase) {
            // good
        }
    }

    public static void run() {
        System.out.println("Array check...");

        byte[] xBytes = new byte[] { 0, -1, -2, -3, -4 };
        short[] xShorts = new short[] { 20, 10, 0, -10, -20 };
        char[] xChars = new char[] { 40000, 40001, 40002, 40003, 40004 };
        int[] xInts = new int[] { 70000, 70001, 70002, 70003, 70004 };
        boolean[] xBooleans = new boolean[] { true, true, false, true, false };
        float[] xFloats = new float[] { -1.5f, -0.5f, 0.0f, 0.5f, 1.5f };
        long[] xLongs = new long[] {
            0x1122334455667788L, 0x8877665544332211L, 0L, 1L, -1l };
        String[] xStrings = new String[] {
            "zero", "one", "two", "three", "four" };

        int[] xEmpty = new int[0];

        checkBytes(xBytes);
        checkShorts(xShorts);
        checkChars(xChars);
        checkInts(xInts);
        checkBooleans(xBooleans);
        checkFloats(xFloats);
        checkLongs(xLongs);
        checkStrings(xStrings);

        checkRange32(xInts, xEmpty, -1, (int) 0x80000000);
        checkRange64(xLongs, -1, (int) 0x80000000);

        checkNegAlloc(-1);
    }
}
