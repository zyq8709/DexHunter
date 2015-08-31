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

/*
 * Some basic operations for testing the debugger.
 */
public class Main {
    long mLong = 0x1122334455667788L;

    public Main() {
        double d = 3.1415;
        System.out.println("d is " + d);
    }

    public static void showObject(Object[] foo) {
        int xyz = 27;
        System.out.println("class: " + foo.getClass());

        for (int i = 0; i < foo.length; i++) {
            System.out.println(i + ": "  + foo[i]);
        }
    }

    public static void main(String[] args) {
        int x = 5;
        Main testObj = new Main();

        Object[] array = new Object[5];
        showObject(array);

        String[] niftyStrings = new String[] { "hey", "you", "there" };
        array = niftyStrings;
        showObject(array);
    }
}
