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
import java.io.IOException;
import java.util.Set;

public class ClassInstance extends Instance {
    private byte[] mFieldValues;

    public ClassInstance(long id, StackTrace stack, long classId) {
        mId = id;
        mStack = stack;
        mClassId = classId;
    }

    public final void loadFieldData(DataInputStream in, int numBytes)
            throws IOException {
        mFieldValues = new byte[numBytes];
        in.readFully(mFieldValues);
    }

    @Override
    public void resolveReferences(State state) {
        ClassObj isa = mHeap.mState.findClass(mClassId);

        resolve(state, isa, isa.mStaticFieldTypes, isa.mStaticFieldValues);
        resolve(state, isa, isa.mFieldTypes, mFieldValues);
    }

    private void resolve(State state, ClassObj isa, int[] types,
            byte[] values) {
        ByteArrayInputStream bais = new ByteArrayInputStream(values);
        DataInputStream dis = new DataInputStream(bais);
        final int N = types.length;

        /*
         * Spin through the list of fields, find all object references,
         * and list ourselves as a reference holder.
         */
        try {
            for (int i = 0; i < N; i++) {
                int type = types[i];
                int size = Types.getTypeSize(type);

                    if (type == Types.OBJECT) {
                        long id;

                        if (size == 4) {
                            id = dis.readInt();
                        } else {
                            id = dis.readLong();
                        }

                        Instance instance = state.findReference(id);

                        if (instance != null) {
                            instance.addParent(this);
                        }
                    } else {
                        dis.skipBytes(size);
                    }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public final int getSize() {
        ClassObj isa = mHeap.mState.findClass(mClassId);

        return isa.getSize();
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

        State state = mHeap.mState;
        ClassObj isa = state.findClass(mClassId);
        int[] types = isa.mFieldTypes;
        ByteArrayInputStream bais = new ByteArrayInputStream(mFieldValues);
        DataInputStream dis = new DataInputStream(bais);
        final int N = types.length;

        /*
         * Spin through the list of fields, find all object references,
         * and list ourselves as a reference holder.
         */
        try {
            for (int i = 0; i < N; i++) {
                int type = types[i];
                int size = Types.getTypeSize(type);

                if (type == Types.OBJECT) {
                    long id;

                    if (size == 4) {
                        id = dis.readInt();
                    } else {
                        id = dis.readLong();
                    }

                    Instance instance = state.findReference(id);

                    if (instance != null) {
                        instance.visit(resultSet, filter);
                    }
                } else {
                    dis.skipBytes(size);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public final String getTypeName() {
        ClassObj theClass = mHeap.mState.findClass(mClassId);

        return theClass.mClassName;
    }

    public final String toString() {
        return String.format("%s@0x%08x", getTypeName(), mId);
    }

    @Override
    public String describeReferenceTo(long referent) {
        ClassObj isa = mHeap.mState.findClass(mClassId);
        int[] types = isa.mFieldTypes;
        String[] fieldNames = isa.mFieldNames;
        ByteArrayInputStream bais = new ByteArrayInputStream(mFieldValues);
        DataInputStream dis = new DataInputStream(bais);
        final int N = types.length;
        StringBuilder result = new StringBuilder("Referenced in field(s):");
        int numReferences = 0;

        /*
         * Spin through the list of fields, add info about the field
         * references to the output text.
         */
        try {
            for (int i = 0; i < N; i++) {
                int type = types[i];
                int size = Types.getTypeSize(type);

                if (type == Types.OBJECT) {
                    long id;

                    if (size == 4) {
                        id = dis.readInt();
                    } else {
                        id = dis.readLong();
                    }

                    if (id == referent) {
                        numReferences++;
                        result.append("\n    ");
                        result.append(fieldNames[i]);
                    }
                } else {
                    dis.skipBytes(size);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }

        /*
         *  TODO:  perform a similar loop over the static fields of isa
         */

        if (numReferences == 0) {
            return super.describeReferenceTo(referent);
        }

        return result.toString();
    }
}
