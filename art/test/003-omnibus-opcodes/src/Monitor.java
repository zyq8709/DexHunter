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
 * Exercise monitors.
 */
public class Monitor {
    public static int mVal = 0;

    public synchronized void subTest() {
        Object obj = new Object();
        synchronized (obj) {
            mVal++;
            obj = null;     // does NOT cause a failure on exit
            Main.assertTrue(obj == null);
        }
    }


    public static void run() {
        System.out.println("Monitor.run");

        Object obj = null;

        try {
            synchronized (obj) {
                mVal++;
            }
            Main.assertTrue(false);
        } catch (NullPointerException npe) {
            /* expected */
        }

        obj = new Object();
        synchronized (obj) {
            mVal++;
        }

        new Monitor().subTest();

        Main.assertTrue(mVal == 2);
    }
}
