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

public class ReferenceMap {
  public ReferenceMap() {
  }

  Object f() {
    Object x[] = new Object[2];
    Object y = null;
    try {
      y = new Object();
      x[2] = y;  // out-of-bound exception
    } catch(Exception ex) {
      if (y == null) {
        x[1] = new Object();
      }
    } finally {
      x[1] = y;
      refmap(0);
    };
    return y;
  }
  native int refmap(int x);

  static {
    System.loadLibrary("arttest");
  }

  public static void main(String[] args) {
    ReferenceMap rm = new ReferenceMap();
    rm.f();
  }
}
