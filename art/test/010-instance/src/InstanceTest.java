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

import java.io.Serializable;

/**
 * Test some instanceof stuff.
 */
public class InstanceTest {
    public static void main(String[] args) {
        System.out.println("instance begin");

        X x = new X();
        X[] xar = new X[1];
        X[][] xarar = new X[1][1];
        X[][][] xararar = new X[1][1][1];
        Y y = new Y();
        Y[] yar = new Y[1];
        Y[][] yarar = new Y[1][1];
        Y[][][] yararar = new Y[1][1][1];
        int[] iar = new int[1];
        int[][] iarar = new int[1][1];
        Object test;

        test = x;
        System.out.println("x instanceof X (true): " + (test instanceof X));
        System.out.println("x instanceof Y (false): " + (test instanceof Y));
        test = y;
        System.out.println("y instanceof X (true): " + (test instanceof X));
        System.out.println("y instanceof Y (true): " + (test instanceof Y));

        test = xar;
        System.out.println("xar instanceof Object (true): "
            + (test instanceof Object));
        System.out.println("xar instanceof X (false): "
            + (test instanceof X));
        System.out.println("xar instanceof X[] (true): "
            + (test instanceof X[]));
        System.out.println("xar instanceof Y[] (false): "
            + (test instanceof Y[]));
        System.out.println("xar instanceof Object[] (true): "
            + (test instanceof Object[]));
        System.out.println("xar instanceof X[][] (false): "
            + (test instanceof X[][]));
        test = yar;
        System.out.println("yar instanceof X[] (true): "
            + (test instanceof X[]));

        test = xararar;
        System.out.println("xararar instanceof Object (true): "
            + (test instanceof Object));
        System.out.println("xararar instanceof Object[] (true): "
            + (test instanceof Object[]));
        System.out.println("xararar instanceof X (false): "
            + (test instanceof X));
        System.out.println("xararar instanceof X[] (false): "
            + (test instanceof X[]));
        System.out.println("xararar instanceof X[][] (false): "
            + (test instanceof X[][]));
        System.out.println("xararar instanceof X[][][] (true): "
            + (test instanceof X[][][]));
        System.out.println("xararar instanceof Object[][][] (true): "
            + (test instanceof Object[][][]));

        System.out.println("xararar instanceof Serializable (true): "
            + (test instanceof Serializable));
        System.out.println("xararar instanceof Serializable[] (true): "
            + (test instanceof Serializable[]));
        System.out.println("xararar instanceof Serializable[][] (true): "
            + (test instanceof Serializable[][]));
        System.out.println("xararar instanceof Serializable[][][] (false): "
            + (test instanceof Serializable[][][]));

        test = yararar;
        System.out.println("yararar instanceof X[][][] (true): "
            + (test instanceof X[][][]));

        test = iar;
        System.out.println("iar instanceof Object (true): "
            + (test instanceof Object));
        System.out.println("iar instanceof Object[] (false): "
            + (test instanceof Object[]));

        test = iarar;
        System.out.println("iarar instanceof Object (true): "
            + (test instanceof Object));
        System.out.println("iarar instanceof Object[] (true): "
            + (test instanceof Object[]));
        System.out.println("iarar instanceof Object[][] (false): "
            + (test instanceof Object[][]));

        System.out.println("instanceof end");
    }
}
