/*
 * Copyright (C) 2008 Google Inc.
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

package com.android.hit;

public enum RootType {
    UNREACHABLE         (0, "unreachable object"),
    INVALID_TYPE        (1, "invalid type"),
    INTERNED_STRING     (2, "interned string"),
    UNKNOWN             (3, "unknown"),
    SYSTEM_CLASS        (4, "system class"),
    VM_INTERNAL         (5, "vm internal"),
    DEBUGGER            (6, "debugger"),
    NATIVE_LOCAL        (7, "native local"),
    NATIVE_STATIC       (8, "native static"),
    THREAD_BLOCK        (9, "thread block"),
    BUSY_MONITOR        (10, "busy monitor"),
    NATIVE_MONITOR      (11, "native monitor"),
    REFERENCE_CLEANUP   (12, "reference cleanup"),
    FINALIZING          (13, "finalizing"),
    JAVA_LOCAL          (14, "java local"),
    NATIVE_STACK        (15, "native stack"),
    JAVA_STATIC         (16, "java static");

    private final int mType;
    private final String mName;

    RootType(int type, String name) {
        mType = type;
        mName = name;
    }

    public final int getType() {
        return mType;
    }

    public final String getName() {
        return mName;
    }
}
