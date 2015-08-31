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
 * Make sure that a sub-thread can join the main thread.
 */
public class Main {
    public static void main(String[] args) {
        Thread t;

        t = new Thread(new JoinMainSub(Thread.currentThread()), "Joiner");
        System.out.print("Starting thread '" + t.getName() + "'\n");
        t.start();

        try { Thread.sleep(1000); }
        catch (InterruptedException ie) {}

        System.out.print("JoinMain starter returning\n");
    }
}

class JoinMainSub implements Runnable {
    private Thread mJoinMe;

    public JoinMainSub(Thread joinMe) {
        mJoinMe = joinMe;
    }

    public void run() {
        System.out.print("@ JoinMainSub running\n");

        try {
            mJoinMe.join();
            System.out.print("@ JoinMainSub successfully joined main\n");
        } catch (InterruptedException ie) {
            System.out.print("@ JoinMainSub interrupted!\n");
        }
        finally {
            System.out.print("@ JoinMainSub bailing\n");
        }
    }
}
