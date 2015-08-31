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
 * Test the uncaught exception handler.
 */
public class Main {
    public static void main(String[] args) {
        testThread(1);
        testThread(2);
        testThread(3);

        catchTheUncaught(1);
    }

    private static void testThread(int which) {
        Thread t = new Helper(which);
        t.start();

        try {
            t.join();
        } catch (InterruptedException ex) {
            ex.printStackTrace();
        }
    }

    static void catchTheUncaught(int which) {
        ThreadDeathHandler defHandler = new ThreadDeathHandler("DEFAULT");
        ThreadDeathHandler threadHandler = new ThreadDeathHandler("THREAD");

        System.out.println("Test " + which);
        switch (which) {
            case 1: {
                Thread.setDefaultUncaughtExceptionHandler(defHandler);
                break;
            }
            case 2: {
                Thread.currentThread().setUncaughtExceptionHandler(
                        threadHandler);
                break;
            }
            case 3: {
                Thread.setDefaultUncaughtExceptionHandler(defHandler);
                Thread.currentThread().setUncaughtExceptionHandler(
                        threadHandler);
                break;
            }
        }

        throw new NullPointerException("Hi diddly-ho, neighborino.");
    }

    private static class Helper extends Thread {
        private int which;

        public Helper(int which) {
            this.which = which;
        }

        public void run() {
            catchTheUncaught(which);
        }
    }
}
