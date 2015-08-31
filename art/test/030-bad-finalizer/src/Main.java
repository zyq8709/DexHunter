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
 * Test a class with a bad finalizer.
 */
public class Main {
    public static void main(String[] args) {
        BadFinalizer bf = new BadFinalizer();

        System.out.println("Constructed object.");
        bf = null;

        System.out.println("Nulled. Requestion gc.");
        System.gc();

        for (int i = 0; i < 8; i++) {
            BadFinalizer.snooze(4000);
            System.out.println("Requesting another GC.");
            System.gc();
        }

        System.out.println("Done waiting.");
        System.exit(0);
    }
}
