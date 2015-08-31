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

public class StackWalk2 {
  // use v1 for this

  String str = new String();  // use v0 for str in <init>

  int f() {
    g(1);  // use v0 for 1, v1 for this
    g(2);  // use v0 for 2, v1 for this
    strTest();  // use v1 for this
    return 0;
  }

  void g(int num_calls) throws RuntimeException {
    if (num_calls == 1) {  // use v0 for 1, v3 for num_calls
      System.logI("1st call");  // use v0 for PrintStream, v1 for "1st call"
      refmap2(24);  // use v0 for 24, v2 for this
    } else if (num_calls == 2) {  // use v0 for 2, v3 for num_calls
      System.logI("2nd call");  // use v0 for PrintStream, v1 for "2nd call"
      refmap2(25);  // use v0 for 24, v2 for this
    }
    throw new RuntimeException();  // use v0 for new RuntimeException
  }

  void strTest() {
    System.logI(str);  // use v1 for PrintStream, v2, v3 for str
    str = null;  // use v1 for null, v3 for str
    str = new String("ya");  // use v2 for "ya", v1 for new String
    String s = str;  // use v0, v1, v3
    System.logI(str);  // use v1 for PrintStream, v2, v3 for str
    System.logI(s);  // use v1 for PrintStream, v0 for s
    s = null;  // use v0
    System.logI(s);  // use v1 for PrintStream, v0 for s
  }

  native int refmap2(int x);

  static {
    System.loadLibrary("arttest");
  }

  public static void main(String[] args) {
    StackWalk2 st = new StackWalk2();
    st.f();
  }
}
