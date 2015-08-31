/*
 * Copyright (C) 2013 The Android Open Source Project
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
 * Testing check-cast, see comment in info.txt
 */

class B {}
class D extends B {}

public class Main {
    public static void main(String args[]) {
        B b = null;
        try {
            if (1 == args.length) {
                b = new B();
            } else {
                b = new D();
            }
            D d = (D) b;
            if (!(b instanceof D)) {
                System.out.println("Error: No ClassCastException throuwn when it should have been.");
            } else {
                System.out.println("OK");
            }
        }
        catch (ClassCastException cce) {
            if (b instanceof D) {
                System.out.println("Error: ClassCastException thrown when it shouldn't have been.");
            } else {
                System.out.println("OK");
            }
        }
    }
}
