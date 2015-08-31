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

import java.lang.reflect.Method;

class JniTest {
    public static void main(String[] args) {
        System.loadLibrary("arttest");
        testFindClassOnAttachedNativeThread();
        testFindFieldOnAttachedNativeThread();
        testCallStaticVoidMethodOnSubClass();
        testGetMirandaMethod();
    }

    private static native void testFindClassOnAttachedNativeThread();

    private static boolean testFindFieldOnAttachedNativeThreadField;

    private static void testFindFieldOnAttachedNativeThread() {
      testFindFieldOnAttachedNativeThreadNative();
      if (!testFindFieldOnAttachedNativeThreadField) {
            throw new AssertionError();
        }
    }

    private static native void testFindFieldOnAttachedNativeThreadNative();

    private static void testCallStaticVoidMethodOnSubClass() {
        testCallStaticVoidMethodOnSubClassNative();
        if (!testCallStaticVoidMethodOnSubClass_SuperClass.executed) {
            throw new AssertionError();
        }
    }

    private static native void testCallStaticVoidMethodOnSubClassNative();

    private static class testCallStaticVoidMethodOnSubClass_SuperClass {
        private static boolean executed = false;
        private static void execute() {
            executed = true;
        }
    }

    private static class testCallStaticVoidMethodOnSubClass_SubClass
        extends testCallStaticVoidMethodOnSubClass_SuperClass {
    }

    private static native Method testGetMirandaMethodNative();

    private static void testGetMirandaMethod() {
        Method m = testGetMirandaMethodNative();
        if (m.getDeclaringClass() != testGetMirandaMethod_MirandaInterface.class) {
            throw new AssertionError();
        }
    }

    private static abstract class testGetMirandaMethod_MirandaAbstract implements testGetMirandaMethod_MirandaInterface {
        public boolean inAbstract() {
            return true;
        }
    }

    private static interface testGetMirandaMethod_MirandaInterface {
        public boolean inInterface();
    }
}
