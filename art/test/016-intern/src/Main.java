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

/**
 * Interned strings
 */
public class Main {
    public static void main(String args[]) {
        String a, b;
        final String foo = "foo";
        final String bar = "bar";

        // Two interned strings should match.
        a = foo.concat(bar).intern();
        b = foo.concat(bar).intern();
        if (a == b && foo != bar) {
            System.out.println("good! " + a);
        } else {
            System.out.println("bad! " + a + " != " + b);
        }

        // An interned string should match a string literal.
        a = ("f" + foo.substring(1,3)).intern();
        if (a == foo) {
            System.out.println("good! " + a);
        } else {
            System.out.println("bad! " + a + " != " + b);
        }

        // Check that a string literal in libcore equals one in the app.
        a = (new java.nio.charset.IllegalCharsetNameException(null)).getMessage();
        b = "null";
        if (a == b) {
            System.out.println("good! " + a);
        } else {
            System.out.println("bad! " + a + " != " + b);
        }
    }
}
