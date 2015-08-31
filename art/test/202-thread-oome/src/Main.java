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

public class Main {
  public static void main(String[] args) throws Exception {
    Thread t = new Thread(null, new Runnable() { public void run() {} }, "", 3*1024*1024*1024);
    try {
      t.start();
    } catch (OutOfMemoryError expected) {
      // TODO: fix bionic bug https://b/6702535 so we can check the full detail message.
      if (!expected.getMessage().startsWith("pthread_create (3GB stack) failed: ")) {
        throw new AssertionError(expected);
      }
    }
  }
}
