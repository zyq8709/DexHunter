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

import java.util.Timer;
import java.util.TimerTask;

/*
 * Throw an exception from a finalizer and make sure it's harmless.  Under
 * Dalvik this may also generate a warning in the log file.
 */
public class Main {
    static Object waiter = new Object();
    static volatile boolean didFinal = false;

    static void createAndForget() {
        Main main = new Main();
    }

    public static void main(String[] args) {
        createAndForget();

        System.gc();
        System.runFinalization();

        new Timer(true).schedule(new TimerTask() {
                public void run() {
                    System.out.println("Timed out, exiting");
                    System.exit(1);
                }
            }, 30000);

        while (!didFinal) {
            try {
                Thread.sleep(500);
            } catch (InterruptedException ie) {
                System.err.println(ie);
            }
        }

        /* give it a chance to cause mayhem */
        try {
            Thread.sleep(750);
        } catch (InterruptedException ie) {
            System.err.println(ie);
        }

        System.out.println("done");
    }

    protected void finalize() throws Throwable {
        System.out.println("In finalizer");

        didFinal = true;

        throw new InterruptedException("whee");
    }
}
