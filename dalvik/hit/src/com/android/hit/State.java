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

/*
 * State is a snapshot of all of the heaps, and related meta-data, for
 * the runtime at a given instant.
 *
 * During parsing of the HPROF file HEAP_DUMP_INFO chunks change which heap
 * is being referenced.
 */
public class State {
    HashMap<Integer, Heap> mHeaps;
    Heap mCurrentHeap;

    public State() {
        mHeaps = new HashMap<Integer, Heap>();
        setToDefaultHeap();
    }

    public Heap setToDefaultHeap() {
        return setHeapTo(0, "default");
    }

    public Heap setHeapTo(int id, String name) {
        Heap heap = mHeaps.get(id);

        if (heap == null) {
            heap = new Heap(name);
            heap.mState = this;
            mHeaps.put(id, heap);
        }

        mCurrentHeap = heap;

        return mCurrentHeap;
    }

    public Heap getHeap(int id) {
        return mHeaps.get(id);
    }

    public Heap getHeap(String name) {
        for (Heap heap: mHeaps.values()) {
            if (heap.mName.equals(name)) {
                return heap;
            }
        }

        return null;
    }

    public final void addStackFrame(StackFrame theFrame) {
        mCurrentHeap.addStackFrame(theFrame);
    }

    public final StackFrame getStackFrame(long id) {
        return mCurrentHeap.getStackFrame(id);
    }

    public final void addStackTrace(StackTrace theTrace) {
        mCurrentHeap.addStackTrace(theTrace);
    }

    public final StackTrace getStackTrace(int traceSerialNumber) {
        return mCurrentHeap.getStackTrace(traceSerialNumber);
    }

    public final StackTrace getStackTraceAtDepth(int traceSerialNumber,
            int depth) {
        return mCurrentHeap.getStackTraceAtDepth(traceSerialNumber, depth);
    }

    public final void addRoot(RootObj root) {
        mCurrentHeap.addRoot(root);
    }

    public final void addThread(ThreadObj thread, int serialNumber) {
        mCurrentHeap.addThread(thread, serialNumber);
    }

    public final ThreadObj getThread(int serialNumber) {
        return mCurrentHeap.getThread(serialNumber);
    }

    public final void addInstance(long id, Instance instance) {
        mCurrentHeap.addInstance(id, instance);
    }

    public final void addClass(long id, ClassObj theClass) {
        mCurrentHeap.addClass(id, theClass);
    }

    public final Instance findReference(long id) {
        for (Heap heap: mHeaps.values()) {
            Instance instance = heap.getInstance(id);

            if (instance != null) {
                return instance;
            }
        }

        //  Couldn't find an instance of a class, look for a class object
        return findClass(id);
    }

    public final ClassObj findClass(long id) {
        for (Heap heap: mHeaps.values()) {
            ClassObj theClass = heap.getClass(id);

            if (theClass != null) {
                return theClass;
            }
        }

        return null;
    }

    public final ClassObj findClass(String name) {
        for (Heap heap: mHeaps.values()) {
            ClassObj theClass = heap.getClass(name);

            if (theClass != null) {
                return theClass;
            }
        }

        return null;
    }

    public final void dumpInstanceCounts() {
        for (Heap heap: mHeaps.values()) {
            System.out.println(
                "+------------------ instance counts for heap: " + heap.mName);
            heap.dumpInstanceCounts();
        }
    }

    public final void dumpSizes() {
        for (Heap heap: mHeaps.values()) {
            System.out.println(
                "+------------------ sizes for heap: " + heap.mName);
            heap.dumpSizes();
        }
    }

    public final void dumpSubclasses() {
        for (Heap heap: mHeaps.values()) {
            System.out.println(
                "+------------------ subclasses for heap: " + heap.mName);
            heap.dumpSubclasses();
        }
    }

    public final void resolveReferences() {
        for (Heap heap: mHeaps.values()) {
            heap.resolveInstanceRefs(this);
            heap.resolveClassStatics(this);
            heap.resolveRoots(this);
        }
    }
}
