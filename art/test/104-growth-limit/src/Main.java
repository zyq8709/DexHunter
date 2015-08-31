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

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class Main {

    public static void main(String[] args) throws Exception {

        int alloc1 = 1;
        try {
            List<byte[]> l = new ArrayList<byte[]>();
            while (true) {
                // Allocate a MB at a time
                l.add(new byte[1048576]);
                alloc1++;
            }
        } catch (OutOfMemoryError e) {
        }
        // Expand the heap to the maximum size.
        // Reflective equivalent of: dalvik.system.VMRuntime.getRuntime().clearGrowthLimit();
        Class<?> vm_runtime = Class.forName("dalvik.system.VMRuntime");
        Method get_runtime = vm_runtime.getDeclaredMethod("getRuntime");
        Object runtime = get_runtime.invoke(null);
        Method clear_growth_limit = vm_runtime.getDeclaredMethod("clearGrowthLimit");
        clear_growth_limit.invoke(runtime);

        int alloc2 = 1;
        try {
            List<byte[]> l = new ArrayList<byte[]>();
            while (true) {
                // Allocate a MB at a time
                l.add(new byte[1048576]);
                alloc2++;
            }
        } catch (OutOfMemoryError e2) {
            if (alloc1 > alloc2) {
                System.out.println("ERROR: Allocated less memory after growth" +
                    "limit cleared (" + alloc1 + " MBs > " + alloc2 + " MBs");
                System.exit(1);
            }
        }
        System.out.println("Test complete");
    }
}
