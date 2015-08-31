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
 * Test comparison operators.
 */
public class Compare {

    /*
     * Test the integer comparisons in various ways.
     */
    static void testIntCompare(int minus, int plus, int plus2, int zero) {
        System.out.println("IntMath.testIntCompare");

        if (minus > plus)
            Main.assertTrue(false);
        if (minus >= plus)
            Main.assertTrue(false);
        if (plus < minus)
            Main.assertTrue(false);
        if (plus <= minus)
            Main.assertTrue(false);
        if (plus == minus)
            Main.assertTrue(false);
        if (plus != plus2)
            Main.assertTrue(false);

        /* try a branch-taken */
        if (plus != minus) {
            Main.assertTrue(true);
        } else {
            Main.assertTrue(false);
        }

        if (minus > 0)
            Main.assertTrue(false);
        if (minus >= 0)
            Main.assertTrue(false);
        if (plus < 0)
            Main.assertTrue(false);
        if (plus <= 0)
            Main.assertTrue(false);
        if (plus == 0)
            Main.assertTrue(false);
        if (zero != 0)
            Main.assertTrue(false);

        if (zero == 0) {
            Main.assertTrue(true);
        } else {
            Main.assertTrue(false);
        }
    }

    /*
     * Test cmp-long.
     *
     * minus=-5, alsoMinus=0xFFFFFFFF00000009, plus=4, alsoPlus=8
     */
    static void testLongCompare(long minus, long alsoMinus, long plus,
        long alsoPlus) {

        System.out.println("IntMath.testLongCompare");
        if (minus > plus)
            Main.assertTrue(false);
        if (plus < minus)
            Main.assertTrue(false);
        if (plus == minus)
            Main.assertTrue(false);

        if (plus >= plus+1)
            Main.assertTrue(false);
        if (minus >= minus+1)
            Main.assertTrue(false);

        /* try a branch-taken */
        if (plus != minus) {
            Main.assertTrue(true);
        } else {
            Main.assertTrue(false);
        }

        /* compare when high words are equal but low words differ */
        if (plus > alsoPlus)
            Main.assertTrue(false);
        if (alsoPlus < plus)
            Main.assertTrue(false);
        if (alsoPlus == plus)
            Main.assertTrue(false);

        /* high words are equal, low words have apparently different signs */
        if (minus < alsoMinus)      // bug!
            Main.assertTrue(false);
        if (alsoMinus > minus)
            Main.assertTrue(false);
        if (alsoMinus == minus)
            Main.assertTrue(false);
    }

    /*
     * Test cmpl-float and cmpg-float.
     */
    static void testFloatCompare(float minus, float plus, float plus2,
        float nan) {

        System.out.println("IntMath.testFloatCompare");
        if (minus > plus)
            Main.assertTrue(false);
        if (plus < minus)
            Main.assertTrue(false);
        if (plus == minus)
            Main.assertTrue(false);
        if (plus != plus2)
            Main.assertTrue(false);

        if (plus <= nan)
            Main.assertTrue(false);
        if (plus >= nan)
            Main.assertTrue(false);
        if (minus <= nan)
            Main.assertTrue(false);
        if (minus >= nan)
            Main.assertTrue(false);
        if (nan >= plus)
            Main.assertTrue(false);
        if (nan <= plus)
            Main.assertTrue(false);

        if (nan == nan)
            Main.assertTrue(false);
    }

    static void testDoubleCompare(double minus, double plus, double plus2,
        double nan) {

        System.out.println("IntMath.testDoubleCompare");
        if (minus > plus)
            Main.assertTrue(false);
        if (plus < minus)
            Main.assertTrue(false);
        if (plus == minus)
            Main.assertTrue(false);
        if (plus != plus2)
            Main.assertTrue(false);

        if (plus <= nan)
            Main.assertTrue(false);
        if (plus >= nan)
            Main.assertTrue(false);
        if (minus <= nan)
            Main.assertTrue(false);
        if (minus >= nan)
            Main.assertTrue(false);
        if (nan >= plus)
            Main.assertTrue(false);
        if (nan <= plus)
            Main.assertTrue(false);

        if (nan == nan)
            Main.assertTrue(false);
    }

    public static void run() {
        testIntCompare(-5, 4, 4, 0);
        testLongCompare(-5L, -4294967287L, 4L, 8L);

        testFloatCompare(-5.0f, 4.0f, 4.0f, (1.0f/0.0f) / (1.0f/0.0f));
        testDoubleCompare(-5.0, 4.0, 4.0, (1.0/0.0) / (1.0/0.0));
    }
}
