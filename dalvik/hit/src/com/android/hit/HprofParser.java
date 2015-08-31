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

import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.InputStream;
import java.io.EOFException;
import java.io.IOException;
import java.util.HashMap;

public class HprofParser
{
    private static final int STRING_IN_UTF8             =   0x01;
    private static final int LOAD_CLASS                 =   0x02;
    private static final int UNLOAD_CLASS               =   0x03;   //  unused
    private static final int STACK_FRAME                =   0x04;
    private static final int STACK_TRACE                =   0x05;
    private static final int ALLOC_SITES                =   0x06;   //  unused
    private static final int HEAP_SUMMARY               =   0x07;
    private static final int START_THREAD               =   0x0a;   //  unused
    private static final int END_THREAD                 =   0x0b;   //  unused
    private static final int HEAP_DUMP                  =   0x0c;
    private static final int HEAP_DUMP_SEGMENT          =   0x1c;
    private static final int HEAP_DUMP_END              =   0x2c;
    private static final int CPU_SAMPLES                =   0x0d;   //  unused
    private static final int CONTROL_SETTINGS           =   0x0e;   //  unused

    private static final int ROOT_UNKNOWN               =   0xff;
    private static final int ROOT_JNI_GLOBAL            =   0x01;
    private static final int ROOT_JNI_LOCAL             =   0x02;
    private static final int ROOT_JAVA_FRAME            =   0x03;
    private static final int ROOT_NATIVE_STACK          =   0x04;
    private static final int ROOT_STICKY_CLASS          =   0x05;
    private static final int ROOT_THREAD_BLOCK          =   0x06;
    private static final int ROOT_MONITOR_USED          =   0x07;
    private static final int ROOT_THREAD_OBJECT         =   0x08;
    private static final int ROOT_CLASS_DUMP            =   0x20;
    private static final int ROOT_INSTANCE_DUMP         =   0x21;
    private static final int ROOT_OBJECT_ARRAY_DUMP     =   0x22;
    private static final int ROOT_PRIMITIVE_ARRAY_DUMP  =   0x23;

    /**
     * Android format addition
     *
     * Specifies information about which heap certain objects came from.
     * When a sub-tag of this type appears in a HPROF_HEAP_DUMP or
     * HPROF_HEAP_DUMP_SEGMENT record, entries that follow it will be
     * associated with the specified heap.  The HEAP_DUMP_INFO data is reset
     * at the end of the HEAP_DUMP[_SEGMENT].  Multiple HEAP_DUMP_INFO entries
     * may appear in a single HEAP_DUMP[_SEGMENT].
     *
     * Format:
     *     u1: Tag value (0xFE)
     *     u4: heap ID
     *     ID: heap name string ID
     */
    private static final int ROOT_HEAP_DUMP_INFO        =   0xfe;
    private static final int ROOT_INTERNED_STRING       =   0x89;
    private static final int ROOT_FINALIZING            =   0x8a;
    private static final int ROOT_DEBUGGER              =   0x8b;
    private static final int ROOT_REFERENCE_CLEANUP     =   0x8c;
    private static final int ROOT_VM_INTERNAL           =   0x8d;
    private static final int ROOT_JNI_MONITOR           =   0x8e;
    private static final int ROOT_UNREACHABLE           =   0x90;
    private static final int ROOT_PRIMITIVE_ARRAY_NODATA=   0xc3;

    DataInputStream mInput;
    int mIdSize;
    State mState;

    byte[] mFieldBuffer = new byte[8];

    /*
     * These are only needed while parsing so are not kept as part of the
     * heap data.
     */
    HashMap<Long, String> mStrings = new HashMap<Long, String>();
    HashMap<Long, String> mClassNames = new HashMap<Long, String>();

    public HprofParser(DataInputStream in) {
        mInput = in;
    }

    public final State parse() {
        State state = new State();
        mState = state;

        try {
            String  s = readNullTerminatedString();
            DataInputStream in = mInput;

            mIdSize = in.readInt();
            Types.setIdSize(mIdSize);

            in.readLong();  //  Timestamp, ignored for now

            while (true) {
                int tag = in.readUnsignedByte();
                int timestamp = in.readInt();
                int length = in.readInt();

                switch (tag) {
                    case STRING_IN_UTF8:
                        loadString(length - 4);
                        break;

                    case LOAD_CLASS:
                        loadClass();
                        break;

                    case STACK_FRAME:
                        loadStackFrame();
                        break;

                    case STACK_TRACE:
                        loadStackTrace();
                        break;

                    case HEAP_DUMP:
                        loadHeapDump(length);
                        mState.setToDefaultHeap();
                        break;

                    case HEAP_DUMP_SEGMENT:
                        loadHeapDump(length);
                        mState.setToDefaultHeap();
                        break;

                    default:
                        skipFully(length);
                }

            }
        } catch (EOFException eof) {
            //  this is fine
        } catch (Exception e) {
            e.printStackTrace();
        }

        mState.resolveReferences();

        return state;
    }

    private String readNullTerminatedString() throws IOException {
        StringBuilder s = new StringBuilder();
        DataInputStream in = mInput;

        for (int c = in.read(); c != 0; c = in.read()) {
            s.append((char) c);
        }

        return s.toString();
    }

    private long readId() throws IOException {
        switch (mIdSize) {
            case 1: return mInput.readUnsignedByte();
            case 2: return mInput.readUnsignedShort();
            case 4: return ((long) mInput.readInt()) & 0x00000000ffffffffL;
            case 8: return mInput.readLong();
        }

        throw new IllegalArgumentException("ID Length must be 1, 2, 4, or 8");
    }

    private String readUTF8(int length) throws IOException {
        byte[] b = new byte[length];

        mInput.read(b);

        return new String(b, "utf-8");
    }

    private void loadString(int length) throws IOException {
        long id = readId();
        String string = readUTF8(length);

        mStrings.put(id, string);
    }

    private void loadClass() throws IOException {
        DataInputStream in = mInput;
        int serial = in.readInt();
        long id = readId();
        int stackTrace = in.readInt();              //  unused
        String name = mStrings.get(readId());

        mClassNames.put(id, name);
    }

    private void loadStackFrame() throws IOException {
        long id = readId();
        String methodName = mStrings.get(readId());
        String methodSignature = mStrings.get(readId());
        String sourceFile = mStrings.get(readId());
        int serial = mInput.readInt();
        int lineNumber = mInput.readInt();

        StackFrame frame = new StackFrame(id, methodName, methodSignature,
            sourceFile, serial, lineNumber);

        mState.addStackFrame(frame);
    }

    private void loadStackTrace() throws IOException {
        int serialNumber = mInput.readInt();
        int threadSerialNumber = mInput.readInt();
        final int numFrames = mInput.readInt();
        StackFrame[] frames = new StackFrame[numFrames];

        for (int i = 0; i < numFrames; i++) {
            frames[i] = mState.getStackFrame(readId());
        }

        StackTrace trace = new StackTrace(serialNumber, threadSerialNumber,
            frames);

        mState.addStackTrace(trace);
    }

    private void loadHeapDump(int length) throws IOException {
        DataInputStream in = mInput;

        while (length > 0) {
            int tag = in.readUnsignedByte();
            length--;

            switch (tag) {
                case ROOT_UNKNOWN:
                    length -= loadBasicObj(RootType.UNKNOWN);
                    break;

                case ROOT_JNI_GLOBAL:
                    length -= loadBasicObj(RootType.NATIVE_STATIC);
                    readId();   //  ignored
                    length -= mIdSize;
                    break;

                case ROOT_JNI_LOCAL:
                    length -= loadJniLocal();
                    break;

                case ROOT_JAVA_FRAME:
                    length -= loadJavaFrame();
                    break;

                case ROOT_NATIVE_STACK:
                    length -= loadNativeStack();
                    break;

                case ROOT_STICKY_CLASS:
                    length -= loadBasicObj(RootType.SYSTEM_CLASS);
                    break;

                case ROOT_THREAD_BLOCK:
                    length -= loadThreadBlock();
                    break;

                case ROOT_MONITOR_USED:
                    length -= loadBasicObj(RootType.BUSY_MONITOR);
                    break;

                case ROOT_THREAD_OBJECT:
                    length -= loadThreadObject();
                    break;

                case ROOT_CLASS_DUMP:
                    length -= loadClassDump();
                    break;

                case ROOT_INSTANCE_DUMP:
                    length -= loadInstanceDump();
                    break;

                case ROOT_OBJECT_ARRAY_DUMP:
                    length -= loadObjectArrayDump();
                    break;

                case ROOT_PRIMITIVE_ARRAY_DUMP:
                    length -= loadPrimitiveArrayDump();
                    break;

                case ROOT_PRIMITIVE_ARRAY_NODATA:
                    System.err.println("+--- PRIMITIVE ARRAY NODATA DUMP");
                    length -= loadPrimitiveArrayDump();

                    throw new IllegalArgumentException(
                        "Don't know how to load a nodata array");

                case ROOT_HEAP_DUMP_INFO:
                    int heapId = mInput.readInt();
                    long heapNameId = readId();
                    String heapName = mStrings.get(heapNameId);

                    mState.setHeapTo(heapId, heapName);
                    length -= 4 + mIdSize;
                    break;

                case ROOT_INTERNED_STRING:
                    length -= loadBasicObj(RootType.INTERNED_STRING);
                    break;

                case ROOT_FINALIZING:
                    length -= loadBasicObj(RootType.FINALIZING);
                    break;

                case ROOT_DEBUGGER:
                    length -= loadBasicObj(RootType.DEBUGGER);
                    break;

                case ROOT_REFERENCE_CLEANUP:
                    length -= loadBasicObj(RootType.REFERENCE_CLEANUP);
                    break;

                case ROOT_VM_INTERNAL:
                    length -= loadBasicObj(RootType.VM_INTERNAL);
                    break;

                case ROOT_JNI_MONITOR:
                    length -= loadJniMonitor();
                    break;

                case ROOT_UNREACHABLE:
                    length -= loadBasicObj(RootType.UNREACHABLE);
                    break;

                default:
                    throw new IllegalArgumentException(
                        "loadHeapDump loop with unknown tag " + tag
                        + " with " + mInput.available()
                        + " bytes possibly remaining");
            }
        }
    }

    private int loadJniLocal() throws IOException {
        long id = readId();
        int threadSerialNumber = mInput.readInt();
        int stackFrameNumber = mInput.readInt();
        ThreadObj thread = mState.getThread(threadSerialNumber);
        StackTrace trace = mState.getStackTraceAtDepth(thread.mStackTrace,
            stackFrameNumber);
        RootObj root = new RootObj(RootType.NATIVE_LOCAL, id,
            threadSerialNumber, trace);

        root.setHeap(mState.mCurrentHeap);
        mState.addRoot(root);

        return mIdSize + 4 + 4;
    }

    private int loadJavaFrame() throws IOException {
        long id = readId();
        int threadSerialNumber = mInput.readInt();
        int stackFrameNumber = mInput.readInt();
        ThreadObj thread = mState.getThread(threadSerialNumber);
        StackTrace trace = mState.getStackTraceAtDepth(thread.mStackTrace,
            stackFrameNumber);
        RootObj root = new RootObj(RootType.JAVA_LOCAL, id, threadSerialNumber,
            trace);

        root.setHeap(mState.mCurrentHeap);
        mState.addRoot(root);

        return mIdSize + 4 + 4;
    }

    private int loadNativeStack() throws IOException {
        long id = readId();
        int threadSerialNumber = mInput.readInt();
        ThreadObj thread = mState.getThread(threadSerialNumber);
        StackTrace trace = mState.getStackTrace(thread.mStackTrace);
        RootObj root = new RootObj(RootType.NATIVE_STACK, id,
            threadSerialNumber, trace);

        root.setHeap(mState.mCurrentHeap);
        mState.addRoot(root);

        return mIdSize + 4;
    }

    private int loadBasicObj(RootType type) throws IOException {
        long id = readId();
        RootObj root = new RootObj(type, id);

        root.setHeap(mState.mCurrentHeap);
        mState.addRoot(root);

        return mIdSize;
    }

    private int loadThreadBlock() throws IOException {
        long id = readId();
        int threadSerialNumber = mInput.readInt();
        ThreadObj thread = mState.getThread(threadSerialNumber);
        StackTrace stack = mState.getStackTrace(thread.mStackTrace);
        RootObj root = new RootObj(RootType.THREAD_BLOCK, id,
            threadSerialNumber, stack);

        root.setHeap(mState.mCurrentHeap);
        mState.addRoot(root);

        return mIdSize + 4;
    }

    private int loadThreadObject() throws IOException {
        long id = readId();
        int threadSerialNumber = mInput.readInt();
        int stackSerialNumber = mInput.readInt();
        ThreadObj thread = new ThreadObj(id, stackSerialNumber);

        mState.addThread(thread, threadSerialNumber);

        return mIdSize + 4 + 4;
    }

    private int loadClassDump() throws IOException {
        int bytesRead = 0;
        DataInputStream in = mInput;
        long id = readId();
        int stackSerialNumber = in.readInt();
        StackTrace stack = mState.getStackTrace(stackSerialNumber);
        long superClassId = readId();
        long classLoaderId = readId();
        long signersId = readId();
        long protectionDomainId = readId();
        long reserved1 = readId();
        long reserved2 = readId();
        int instanceSize = in.readInt();

        bytesRead = (7 * mIdSize) + 4 + 4;

        //  Skip over the constant pool
        int numEntries = in.readUnsignedShort();
        bytesRead += 2;

        for (int i = 0; i < numEntries; i++) {
            in.readUnsignedShort();
            bytesRead += 2 + skipValue();
        }

        //  Static fields
        numEntries = in.readUnsignedShort();
        bytesRead += 2;

        String[] staticFieldNames = new String[numEntries];
        int[] staticFieldTypes = new int[numEntries];
        ByteArrayOutputStream staticFieldValues = new ByteArrayOutputStream();
        byte[] buffer = mFieldBuffer;

        for (int i = 0; i < numEntries; i++) {
            staticFieldNames[i] = mStrings.get(readId());

            int fieldType = in.readByte();
            int fieldSize = Types.getTypeSize(fieldType);
            staticFieldTypes[i] = fieldType;

            in.readFully(buffer, 0, fieldSize);
            staticFieldValues.write(buffer, 0, fieldSize);

            bytesRead += mIdSize + 1 + fieldSize;
        }

        //  Instance fields
        numEntries = in.readUnsignedShort();
        bytesRead += 2;

        String[] names = new String[numEntries];
        int[] types = new int[numEntries];

        for (int i = 0; i < numEntries; i++) {
            long fieldName = readId();
            int type = in.readUnsignedByte();

            names[i] = mStrings.get(fieldName);
            types[i] = type;

            bytesRead += mIdSize + 1;
        }

        ClassObj theClass = new ClassObj(id, stack, mClassNames.get(id));

        theClass.setStaticFieldNames(staticFieldNames);
        theClass.setStaticFieldTypes(staticFieldTypes);
        theClass.setStaticFieldValues(staticFieldValues.toByteArray());

        theClass.setSuperclassId(superClassId);
        theClass.setFieldNames(names);
        theClass.setFieldTypes(types);
        theClass.setSize(instanceSize);

        theClass.setHeap(mState.mCurrentHeap);

        mState.addClass(id, theClass);

        return bytesRead;
    }

    private int loadInstanceDump() throws IOException {
        long id = readId();
        int stackId = mInput.readInt();
        StackTrace stack = mState.getStackTrace(stackId);
        long classId = readId();
        int remaining = mInput.readInt();
        ClassInstance instance = new ClassInstance(id, stack, classId);

        instance.loadFieldData(mInput, remaining);
        instance.setHeap(mState.mCurrentHeap);
        mState.addInstance(id, instance);

        return mIdSize + 4 + mIdSize + 4 + remaining;
    }

    private int loadObjectArrayDump() throws IOException {
        long id = readId();
        int stackId = mInput.readInt();
        StackTrace stack = mState.getStackTrace(stackId);
        int numElements = mInput.readInt();
        long classId = readId();
        int totalBytes = numElements * mIdSize;
        byte[] data = new byte[totalBytes];
        String className = mClassNames.get(classId);

        mInput.readFully(data);

        ArrayInstance array = new ArrayInstance(id, stack, Types.OBJECT,
            numElements, data);

        array.mClassId = classId;
        array.setHeap(mState.mCurrentHeap);
        mState.addInstance(id, array);

        return mIdSize + 4 + 4 + mIdSize + totalBytes;
    }

    private int loadPrimitiveArrayDump() throws IOException {
        long id = readId();
        int stackId = mInput.readInt();
        StackTrace stack = mState.getStackTrace(stackId);
        int numElements = mInput.readInt();
        int type = mInput.readUnsignedByte();
        int size = Types.getTypeSize(type);
        int totalBytes = numElements * size;
        byte[] data = new byte[totalBytes];

        mInput.readFully(data);

        ArrayInstance array = new ArrayInstance(id, stack, type, numElements,
            data);

        array.setHeap(mState.mCurrentHeap);
        mState.addInstance(id, array);

        return mIdSize + 4 + 4 + 1 + totalBytes;
    }

    private int loadJniMonitor() throws IOException {
        long id = readId();
        int threadSerialNumber = mInput.readInt();
        int stackDepth = mInput.readInt();
        ThreadObj thread = mState.getThread(threadSerialNumber);
        StackTrace trace = mState.getStackTraceAtDepth(thread.mStackTrace,
            stackDepth);
        RootObj root = new RootObj(RootType.NATIVE_MONITOR, id,
            threadSerialNumber, trace);

        root.setHeap(mState.mCurrentHeap);
        mState.addRoot(root);

        return mIdSize + 4 + 4;
    }

    private int skipValue() throws IOException {
        int type = mInput.readUnsignedByte();
        int size = Types.getTypeSize(type);

        skipFully(size);

        return size + 1;
    }

    /*
     * BufferedInputStream will not skip(int) the entire requested number
     * of bytes if it extends past the current buffer boundary.  So, this
     * routine is needed to actually skip over the requested number of bytes
     * using as many iterations as needed.
     */
    private void skipFully(long numBytes) throws IOException {
        while (numBytes > 0) {
            long skipped = mInput.skip(numBytes);

            numBytes -= skipped;
        }
    }
}
