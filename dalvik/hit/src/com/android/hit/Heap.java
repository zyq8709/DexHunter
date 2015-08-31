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

import java.util.ArrayList;
import java.util.HashMap;

public class Heap {
    String mName;

    //  List of individual stack frames
    HashMap<Long, StackFrame> mFrames = new HashMap<Long, StackFrame>();

    //  List stack traces, which are lists of stack frames
    HashMap<Integer, StackTrace> mTraces = new HashMap<Integer, StackTrace>();

    //  Root objects such as interned strings, jni locals, etc
    ArrayList<RootObj> mRoots = new ArrayList<RootObj>();

    //  List of threads
    HashMap<Integer, ThreadObj> mThreads = new HashMap<Integer, ThreadObj>();

    //  Class definitions
    HashMap<Long, ClassObj> mClassesById = new HashMap<Long, ClassObj>();
    HashMap<String, ClassObj> mClassesByName = new HashMap<String, ClassObj>();

    //  List of instances of above class definitions
    HashMap<Long, Instance> mInstances = new HashMap<Long, Instance>();

    //  The super-state that this heap is part of
    State mState;

    public Heap(String name) {
        mName = name;
    }

    public final void addStackFrame(StackFrame theFrame) {
        mFrames.put(theFrame.mId, theFrame);
    }

    public final StackFrame getStackFrame(long id) {
        return mFrames.get(id);
    }

    public final void addStackTrace(StackTrace theTrace) {
        mTraces.put(theTrace.mSerialNumber, theTrace);
    }

    public final StackTrace getStackTrace(int traceSerialNumber) {
        return mTraces.get(traceSerialNumber);
    }

    public final StackTrace getStackTraceAtDepth(int traceSerialNumber,
            int depth) {
        StackTrace trace = mTraces.get(traceSerialNumber);

        if (trace != null) {
            trace = trace.fromDepth(depth);
        }

        return trace;
    }

    public final void addRoot(RootObj root) {
        root.mIndex = mRoots.size();
        mRoots.add(root);
    }

    public final void addThread(ThreadObj thread, int serialNumber) {
        mThreads.put(serialNumber, thread);
    }

    public final ThreadObj getThread(int serialNumber) {
        return mThreads.get(serialNumber);
    }

    public final void addInstance(long id, Instance instance) {
        mInstances.put(id, instance);
    }

    public final Instance getInstance(long id) {
        return mInstances.get(id);
    }

    public final void addClass(long id, ClassObj theClass) {
        mClassesById.put(id, theClass);
        mClassesByName.put(theClass.mClassName, theClass);
    }

    public final ClassObj getClass(long id) {
        return mClassesById.get(id);
    }

    public final ClassObj getClass(String name) {
        return mClassesByName.get(name);
    }

    public final void dumpInstanceCounts() {
        for (ClassObj theClass: mClassesById.values()) {
            int count = theClass.mInstances.size();

            if (count > 0) {
                System.out.println(theClass + ": " + count);
            }
        }
    }

    public final void dumpSubclasses() {
        for (ClassObj theClass: mClassesById.values()) {
            int count = theClass.mSubclasses.size();

            if (count > 0) {
                System.out.println(theClass);
                theClass.dumpSubclasses();
            }
        }
    }

    public final void dumpSizes() {
        for (ClassObj theClass: mClassesById.values()) {
            int size = 0;

            for (Instance instance: theClass.mInstances) {
                size += instance.getCompositeSize();
            }

            if (size > 0) {
                System.out.println(theClass + ": base " + theClass.getSize()
                    + ", composite " + size);
            }
        }
    }

    /*
     * Spin through all of the class instances and link them to their
     * parent class definition objects.  Then have each instance resolve
     * its own internal object references.
     */
    public final void resolveInstanceRefs(State state) {
        for (Instance instance : mInstances.values()) {
            ClassObj theClass = mClassesById.get(instance.mClassId);

            if (theClass == null) {
                continue;
            }

            String name = theClass.mClassName;
            String superclassName = "none";
            ClassObj superClass = mClassesById.get(theClass.mSuperclassId);

            if (superClass != null) {
                superclassName = superClass.mClassName;
            }

            theClass.addInstance(instance);
            instance.resolveReferences(state);
        }
    }

    public final void resolveClassStatics(State state) {
        for (ClassObj theClass: mClassesById.values()) {
            theClass.resolveReferences(state);
        }
    }

    public final void resolveRoots(State state) {
        for (RootObj root: mRoots) {
            root.resolveReferences(state);
        }
    }
}
