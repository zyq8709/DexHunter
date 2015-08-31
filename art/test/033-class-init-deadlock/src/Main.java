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
 * This causes most VMs to lock up.
 *
 * Interrupting threads in class initialization should NOT work.
 */
public class Main {
    public static boolean aInitialized = false;
    public static boolean bInitialized = false;

    static public void main(String[] args) {
        Thread thread1, thread2;

        System.out.println("Deadlock test starting.");
        thread1 = new Thread() { public void run() { new A(); } };
        thread2 = new Thread() { public void run() { new B(); } };
        thread1.start();
        // Give thread1 a chance to start before starting thread2.
        try { Thread.sleep(1000); } catch (InterruptedException ie) { }
        thread2.start();

        try { Thread.sleep(6000); } catch (InterruptedException ie) { }

        System.out.println("Deadlock test interrupting threads.");
        thread1.interrupt();
        thread2.interrupt();
        System.out.println("Deadlock test main thread bailing.");
        System.out.println("A initialized: " + aInitialized);
        System.out.println("B initialized: " + bInitialized);
        System.exit(0);
    }
}

class A {
    static {
        System.out.println("A initializing...");
        try { Thread.sleep(3000); } catch (InterruptedException ie) { }
        new B();
        System.out.println("A initialized");
        Main.aInitialized = true;
    }
}

class B {
    static {
        System.out.println("B initializing...");
        try { Thread.sleep(3000); } catch (InterruptedException ie) { }
        new A();
        System.out.println("B initialized");
        Main.bInitialized = true;
    }
}
