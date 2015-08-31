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

import java.util.ArrayList;
import java.util.List;

class ParallelGC implements Runnable {
    public static void main(String[] args) throws Exception {
        Thread[] threads = new Thread[16];
        for (int i = 0; i < threads.length; i++) {
            threads[i] = new Thread(new ParallelGC(i));
        }
        for (Thread thread : threads) {
            thread.start();
        }
        for (Thread thread : threads) {
            thread.join();
        }
    }

    private final int id;

    private ParallelGC(int id) {
        this.id = id;
    }

    public void run() {
        List l = new ArrayList();
        for (int i = 0; i < 1000; i++) {
            l.add(new ArrayList(i));
            System.out.print(id);
        }
    }
}
