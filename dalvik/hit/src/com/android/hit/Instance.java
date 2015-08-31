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
import java.util.HashSet;
import java.util.Set;

public abstract class Instance {
    long mId;

    //  Id of the ClassObj of which this object is an instance
    long mClassId;

    //  The stack in which this object was allocated
    StackTrace mStack;

    //  The heap in which this object was allocated (app, zygote, etc)
    Heap mHeap;

    //  The size of this object
    int mSize;

    public interface Filter {
        public boolean accept(Instance instance);
    }

    //  List of all objects that hold a live reference to this object
    private ArrayList<Instance> mParents;

    /*
     * After the whole HPROF file is read and parsed this method will be
     * called on all heap objects so that they can resolve their internal
     * object references.
     *
     * The super-State is passed in because some object references (such
     * as interned Strings and static class fields) may need to be searched
     * for in a heap other than the one this Instance is in.
     */
    public abstract void resolveReferences(State state);

    /*
     * Some operations require gathering all the objects in a given section
     * of the object graph.  If non-null, the filter is applied to each
     * node in the graph to determine if it should be added to the result
     * set.
     */
    public abstract void visit(Set<Instance> resultSet, Filter filter);

    public void setSize(int size) {
        mSize = size;
    }

    public final int getCompositeSize() {
        HashSet<Instance> set = new HashSet<Instance>();

        visit(set, null);

        int size = 0;

        for (Instance instance: set) {
            size += instance.getSize();
        }

        return size;
    }

    //  Returns the instrinsic size of a given object
    public int getSize() {
        return mSize;
    }

    public abstract String getTypeName();

    public void setHeap(Heap heap) {
        mHeap = heap;
    }

    //  Add to the list of objects that have a hard reference to this Instance
    public void addParent(Instance parent) {
        if (mParents == null) {
            mParents = new ArrayList<Instance>();
        }

        mParents.add(parent);
    }

    public ArrayList<Instance> getParents() {
        if (mParents == null) {
            mParents = new ArrayList<Instance>();
        }

        return mParents;
    }

    /*
     * If this object has a reference to the object identified by id, return
     * a String describing the reference in detail.
     */
    public String describeReferenceTo(long id) {
        return "No reference to 0x" + Long.toHexString(id);
    }
}
