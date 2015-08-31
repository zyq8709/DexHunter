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

public class Main {
    int mFoo = 27;

    private void doStuff(int i, int[][] is, String s, String[][] ss) {
        System.out.println("mFoo is " + mFoo);
    }

    public static void main(String[] args) {
        Main instance = null;
        instance.doStuff(0, null, null, null);
    }
}
