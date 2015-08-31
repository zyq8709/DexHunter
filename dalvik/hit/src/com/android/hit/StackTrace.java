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

public class StackTrace {
    int mSerialNumber;
    int mThreadSerialNumber;
    StackFrame[] mFrames;

    /*
     * For subsets of the stack frame we'll reference the parent list of frames
     * but keep track of its offset into the parent's list of stack frame ids.
     * This alleviates the need to constantly be duplicating subsections of the
     * list of stack frame ids.
     */
    StackTrace mParent = null;
    int mOffset = 0;

    private StackTrace() {

    }

    public StackTrace(int serial, int thread, StackFrame[] frames) {
        mSerialNumber = serial;
        mThreadSerialNumber = thread;
        mFrames = frames;
    }

    public final StackTrace fromDepth(int startingDepth) {
        StackTrace result = new StackTrace();

        if (mParent != null) {
            result.mParent = mParent;
        } else {
            result.mParent = this;
        }

        result.mOffset = startingDepth + mOffset;

        return result;
    }

    public final void dump() {
        final int N = mFrames.length;

        for (int i = 0; i < N; i++) {
            System.out.println(mFrames[i].toString());
        }
    }
}
