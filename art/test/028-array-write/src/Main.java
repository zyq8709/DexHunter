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
 * Array write speed test.
 */
public class Main {
    /** whether to report times */
    static boolean timing = false;

    static final int STORAGE_SIZE = 128*1024;
    static int[] mStorage = new int[STORAGE_SIZE];

    static public void report(long start, long end) {
        if (! timing) {
            return;
        }

        System.out.println("Finished in " + ((end - start) / 1000000.0)
            + " msec");
    }

    static void writeArray(int val) {
        for (int i = STORAGE_SIZE-1; i >= 0; i--)
            mStorage[i] = val;
    }

    static void writeTest() {
        long start, end;

        writeArray(0);  // touch all the memory

        System.out.println("Running writeTest...");
        start = System.nanoTime();
        for (int i = 1; i < 20; i++)
            writeArray(i);
        end = System.nanoTime();

        report(start, end);
    }

    static void copyTest() {
        long start, end;

        // touch once
        System.arraycopy(mStorage, 0, mStorage,
            STORAGE_SIZE/2, STORAGE_SIZE/2);

        System.out.println("Running copyTest...");
        start = System.nanoTime();
        for (int i = 1; i < 35; i++) {
            System.arraycopy(mStorage, 0, mStorage,
                STORAGE_SIZE/2, STORAGE_SIZE/2);
        }
        end = System.nanoTime();

        report(start, end);
    }

    public static void array_028() {
        writeTest();
        copyTest();
        System.out.println("Done!");
    }

    public static void main(String[] args) {
        if ((args.length >= 1) && args[0].equals("--timing")) {
            timing = true;
        }
        array_028();
    }
}
