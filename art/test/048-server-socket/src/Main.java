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

import java.net.ServerSocket;
import java.io.IOException;


/**
 * Quick server socket test.
 */
public class Main {
    private static void snooze(int sec) {
        try {
            Thread.sleep(sec * 1000);
        } catch (InterruptedException ie) {
            ie.printStackTrace();
        }
    }

    public static void main(String[] args) {
        ServerSocket socket;

        try {
            socket = new ServerSocket(7890);
        } catch (IOException ioe) {
            System.out.println("couldn't open socket " + ioe.getMessage());
            return;
        }

        System.out.println("opened!");
        snooze(1);

        try {
            socket.close();
        } catch (IOException ioe) {
            System.out.println("couldn't close socket " + ioe.getMessage());
            return;
        }

        System.out.println("closed!");
        snooze(1);

        try {
            socket = new ServerSocket(7890);
        } catch (IOException ioe) {
            System.out.println("couldn't reopen socket " + ioe.getMessage());
            return;
        }

        System.out.println("reopened!");
        System.out.println("done");
    }
}
