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
 * Return stuff.
 */
public class Main {
    public static void main(String[] args) {

        System.out.println("pick 1");
        pickOne(1).run();
        System.out.println(((CommonInterface)pickOne(1)).doStuff());

        System.out.println("pick 2");
        pickOne(2).run();
        System.out.println(((CommonInterface)pickOne(2)).doStuff());

        System.out.println("pick 3");
        pickOne(3).run();
    }

    public static Runnable pickOne(int which) {
        Runnable runme;

        if (which == 1)
            runme = new ClassOne();
        else if (which == 2)
            runme = new ClassTwo();
        else if (which == 3)
            runme = new ClassThree();
        else
            runme = null;

        return runme;
    }
}

class ClassOne implements CommonInterface, Runnable {
    public void run() {
        System.out.println("one running");
    }
    public int doStuff() {
        System.out.println("one");
        return 1;
    }
}

class ClassTwo implements CommonInterface, Runnable {
    public void run() {
        System.out.println("two running");
    }
    public int doStuff() {
        System.out.println("two");
        return 2;
    }
}

class ClassThree implements Runnable {
    public void run() {
        System.out.println("three running");
    }
}

interface CommonInterface {
    int doStuff();
}
