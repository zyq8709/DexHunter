/*
 * Copyright (C) 2012 The Android Open Source Project
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

import java.util.Random;

public class Main {
    private static final int buckets = 16 * 1024;
    private static final int bufferSize = 1024;

    static class ByteContainer {
        public byte[] bytes;
    }

    public static void main(String[] args) throws Exception {
        ByteContainer[] l = new ByteContainer[buckets];

        for (int i = 0; i < buckets; ++i) {
            l[i] = new ByteContainer();
        }

        Random rnd = new Random(123456);
        for (int i = 0; i < buckets / 256; ++i) {
            int index = rnd.nextInt(buckets);
            l[index].bytes = new byte[bufferSize];

            // Try to get GC to run if we can
            Runtime.getRuntime().gc();

            // Shuffle the array to try cause the lost object problem:
            // This problem occurs when an object is white, it may be
            // only referenced from a white or grey object. If the white
            // object is moved during a CMS to be a black object's field, it
            // causes the moved object to not get marked. This can result in
            // heap corruption. A typical way to address this issue is by
            // having a card table.
            // This aspect of the test is meant to ensure that card
            // dirtying works and that we check the marked cards after
            // marking.
            // If these operations are not done, a segfault / failed assert
            // should occur.
            for (int j = 0; j < l.length; ++j) {
                int a = l.length - i - 1;
                int b = rnd.nextInt(a);
                byte[] temp = l[a].bytes;
                l[a].bytes = l[b].bytes;
                l[b].bytes = temp;
            }
        }
        System.out.println("Test complete");
    }
}
