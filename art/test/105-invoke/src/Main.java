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

class Main implements InvokeInterface {

    int virI_I(int a) {
        return a + 123;
    }

    int virI_II(int a, int b) {
        return a + b + 321;
    }

    int virI_III(int a, int b, int c) {
        return a + b + c + 432;
    }

    int virI_IIII(int a, int b, int c, int d) {
        return a + b + c + d + 919;
    }

    int virI_IIIII(int a, int b, int c, int d, int e) {
        return a + b + c + d + e + 1010;
    }

    int virI_IIIIII(int a, int b, int c, int d, int e, int f) {
        return a + b + c + d + e + f + 2020;
    }

    static int statI_I(int a) {
         return a + 123;
    }

    static int statI_II(int a, int b) {
        return a + b + 321;
    }

    static int statI_III(int a, int b, int c) {
        return a + b + c + 432;
    }

    static int statI_IIII(int a, int b, int c, int d) {
        return a + b + c + d + 919;
    }

    static int statI_IIIII(int a, int b, int c, int d, int e) {
        return a + b + c + d + e + 1010;
    }

    static int statI_IIIIII(int a, int b, int c, int d, int e, int f) {
        return a + b + c + d + e + f + 2020;
    }

    public int interfaceMethod(int i) {
        return i + 23;
    }

    static int invoke(int a) {
        Main foo = new Main();

        return foo.virI_I(a) +
               foo.virI_II(a, 1) +
               foo.virI_III(a, 1, 2) +
               foo.virI_IIII(a, 1, 2, 3) +
               foo.virI_IIIII(a, 1, 2, 3, 4) +
               foo.virI_IIIIII(a, 1, 2, 3, 4, 5) +
               statI_I(a) +
               statI_II(a, 1) +
               statI_III(a, 1, 2) +
               statI_IIII(a, 1, 2, 3) +
               statI_IIIII(a, 1, 2, 3, 4) +
               statI_IIIIII(a, 1, 2, 3, 4, 5) +
               foo.interfaceMethod(a);
    }

    public static void main(String[] args) {
        boolean failure = false;
        int res = invoke(912);
        if (res == 21599) {
            System.out.println("invoke PASSED");
        } else {
            System.out.println("invoke FAILED: " + res);
            failure = true;
        }
        System.exit(failure ? 1 : 0);
    }
}

interface InvokeInterface {
    int interfaceMethod(int i);
}
