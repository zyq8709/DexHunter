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

import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.util.Set;

public class ArrayInstance extends Instance {
    private int mType;
    private int mNumEntries;
    private byte[] mData;

    public ArrayInstance(long id, StackTrace stack, int type, int numEntries,
            byte[] data) {
        mId = id;
        mStack = stack;
        mType = type;
        mNumEntries = numEntries;
        mData = data;
    }

    public final void resolveReferences(State state) {
        if (mType != Types.OBJECT) {
            return;
        }

        /*
         * mData holds a stream of object instance ids
         * Spin through them all and list ourselves as a reference holder.
         */
        int idSize = Types.getTypeSize(mType);
        final int N = mNumEntries;

        ByteArrayInputStream bais = new ByteArrayInputStream(mData);
        DataInputStream dis = new DataInputStream(bais);

        for (int i = 0; i < N; i++) {
            long id;

            try {
                if (idSize == 4) {
                    id = dis.readInt();
                } else {
                    id = dis.readLong();
                }

                Instance instance = state.findReference(id);

                if (instance != null) {
                    instance.addParent(this);
                }
            } catch (java.io.IOException e) {
                e.printStackTrace();
            }
        }
    }

    @Override
    public final int getSize() {
        return mData.length;
    }

    @Override
    public final void visit(Set<Instance> resultSet, Filter filter) {
        //  If we're in the set then we and our children have been visited
        if (resultSet.contains(this)) {
            return;
        }

        if (null != filter) {
            if (filter.accept(this)) {
                resultSet.add(this);
            }
        } else {
            resultSet.add(this);
        }

        if (mType != Types.OBJECT) {
            return;
        }

        /*
         * mData holds a stream of object instance ids
         * Spin through them all and visit them
         */
        int idSize = Types.getTypeSize(mType);
        final int N = mNumEntries;

        ByteArrayInputStream bais = new ByteArrayInputStream(mData);
        DataInputStream dis = new DataInputStream(bais);
        State state = mHeap.mState;

        for (int i = 0; i < N; i++) {
            long id;

            try {
                if (idSize == 4) {
                    id = dis.readInt();
                } else {
                    id = dis.readLong();
                }

                Instance instance = state.findReference(id);

                if (instance != null) {
                    instance.visit(resultSet, filter);
                }
            } catch (java.io.IOException e) {
                e.printStackTrace();
            }
        }
    }

    @Override
    public final String getTypeName() {
        return Types.getTypeName(mType) + "[" + mNumEntries + "]";
    }

    public final String toString() {
        return String.format("%s@0x08x", getTypeName(), mId);
    }

    @Override
    public String describeReferenceTo(long referent) {
        //  If this isn't an object array then we can't refer to an object
        if (mType != Types.OBJECT) {
            return super.describeReferenceTo(referent);
        }

        int idSize = Types.getTypeSize(mType);
        final int N = mNumEntries;
        int numRefs = 0;
        StringBuilder result = new StringBuilder("Elements [");
        ByteArrayInputStream bais = new ByteArrayInputStream(mData);
        DataInputStream dis = new DataInputStream(bais);

        /*
         * Spin through all the objects and build up a string describing
         * all of the array elements that refer to the target object.
         */
        for (int i = 0; i < N; i++) {
            long id;

            try {
                if (idSize == 4) {
                    id = dis.readInt();
                } else {
                    id = dis.readLong();
                }

                if (id == referent) {
                    numRefs++;

                    if (numRefs > 1) {
                        result.append(", ");
                    }

                    result.append(i);
                }
            } catch (java.io.IOException e) {
                e.printStackTrace();
            }
        }

        if (numRefs == 0) {
            return super.describeReferenceTo(referent);
        }

        result.append("]");

        return result.toString();
    }
}
