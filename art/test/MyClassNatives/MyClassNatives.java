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

class MyClassNatives {
    native void throwException();
    native void foo();
    native int bar(int count);
    static native int sbar(int count);
    native int fooI(int x);
    native int fooII(int x, int y);
    native long fooJJ(long x, long y);
    native Object fooO(Object x);
    native double fooDD(double x, double y);
    synchronized native long fooJJ_synchronized(long x, long y);
    native Object fooIOO(int x, Object y, Object z);
    static native Object fooSIOO(int x, Object y, Object z);
    static native int fooSII(int x, int y);
    static native double fooSDD(double x, double y);
    static synchronized native Object fooSSIOO(int x, Object y, Object z);
    static native void arraycopy(Object src, int src_pos, Object dst, int dst_pos, int length);
    native boolean compareAndSwapInt(Object obj, long offset, int expected, int newval);
    static native int getText(long val1, Object obj1, long val2, Object obj2);
    synchronized native Object []getSinkPropertiesNative(String path);

    native Class instanceMethodThatShouldReturnClass();
    static native Class staticMethodThatShouldReturnClass();

    native void instanceMethodThatShouldTakeClass(int i, Class c);
    static native void staticMethodThatShouldTakeClass(int i, Class c);
}
