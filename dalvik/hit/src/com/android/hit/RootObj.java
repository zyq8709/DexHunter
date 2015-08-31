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

import java.util.Set;

public class RootObj extends Instance {
    RootType mType = RootType.UNKNOWN;
    int mIndex;
    int mThread;

    /*
     * These two fields are only used by roots that are static
     * fields of class objects
     */
    long mParent;
    String mComment;

    public RootObj(RootType type) {
        this(type, 0, 0, null);
    }

    public RootObj(RootType type, long id) {
        this(type, id, 0, null);
    }

    public RootObj(RootType type, long id, int thread, StackTrace stack) {
        mType = type;
        mId = id;
        mThread = thread;
        mStack = stack;
    }

    public final String getClassName(State state) {
        ClassObj theClass;

        if (mType == RootType.SYSTEM_CLASS) {
            theClass = state.findClass(mId);
        } else {
            Instance instance = state.findReference(mId);

            theClass = state.findClass(instance.mClassId);
        }

        if (theClass == null) {
            return "no class defined!!";
        }

        return theClass.mClassName;
    }

    @Override
    public final int getSize() {
        Instance instance = null;

        if (mType == RootType.SYSTEM_CLASS) {
            instance = mHeap.mState.findClass(mId);
        } else {
            instance = mHeap.mState.findReference(mId);
        }

        if (instance == null) {
            return 0;
        }

        return instance.getSize();
    }

    @Override
    public final void visit(Set<Instance> resultSet, Filter filter) {
        if (resultSet.contains(this)) {
            return;
        }

        if (filter != null) {
            if (filter.accept(this)) {
                resultSet.add(this);
            }
        } else {
            resultSet.add(this);
        }
    }

    @Override
    public final void resolveReferences(State state) {
        //  Nothing to do here
    }

    @Override
    public final String getTypeName() {
        return "root " + mType.getName();
    }

    public final String toString() {
        return String.format("%s@0x08x", mType.getName(), mId);
    }
}
