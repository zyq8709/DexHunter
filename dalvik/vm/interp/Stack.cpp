/*
 * Copyright (C) 2008 The Android Open Source Project
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

/*
 * Stacks and their uses (e.g. native --> interpreted method calls).
 *
 * See the majestic ASCII art in Stack.h.
 */
#include "Dalvik.h"
#include "jni.h"

#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_ANDROID_OS
#include <corkscrew/backtrace.h>
#endif

/*
 * Initialize the interpreter stack in a new thread.
 *
 * Currently this doesn't do much, since we don't need to zero out the
 * stack (and we really don't want to if it was created with mmap).
 */
bool dvmInitInterpStack(Thread* thread, int stackSize)
{
    assert(thread->interpStackStart != NULL);

    assert(thread->interpSave.curFrame == NULL);

    return true;
}

/*
 * We're calling an interpreted method from an internal VM function or
 * via reflection.
 *
 * Push a frame for an interpreted method onto the stack.  This is only
 * used when calling into interpreted code from native code.  (The
 * interpreter does its own stack frame manipulation for interp-->interp
 * calls.)
 *
 * The size we need to reserve is the sum of parameters, local variables,
 * saved goodies, and outbound parameters.
 *
 * We start by inserting a "break" frame, which ensures that the interpreter
 * hands control back to us after the function we call returns or an
 * uncaught exception is thrown.
 */
static bool dvmPushInterpFrame(Thread* self, const Method* method)
{
    StackSaveArea* saveBlock;
    StackSaveArea* breakSaveBlock;
    int stackReq;
    u1* stackPtr;

    assert(!dvmIsNativeMethod(method));
    assert(!dvmIsAbstractMethod(method));

    stackReq = method->registersSize * 4        // params + locals
                + sizeof(StackSaveArea) * 2     // break frame + regular frame
                + method->outsSize * 4;         // args to other methods

    if (self->interpSave.curFrame != NULL)
        stackPtr = (u1*) SAVEAREA_FROM_FP(self->interpSave.curFrame);
    else
        stackPtr = self->interpStackStart;

    if (stackPtr - stackReq < self->interpStackEnd) {
        /* not enough space */
        ALOGW("Stack overflow on call to interp "
             "(req=%d top=%p cur=%p size=%d %s.%s)",
            stackReq, self->interpStackStart, self->interpSave.curFrame,
            self->interpStackSize, method->clazz->descriptor, method->name);
        dvmHandleStackOverflow(self, method);
        assert(dvmCheckException(self));
        return false;
    }

    /*
     * Shift the stack pointer down, leaving space for the function's
     * args/registers and save area.
     */
    stackPtr -= sizeof(StackSaveArea);
    breakSaveBlock = (StackSaveArea*)stackPtr;
    stackPtr -= method->registersSize * 4 + sizeof(StackSaveArea);
    saveBlock = (StackSaveArea*) stackPtr;

#if !defined(NDEBUG) && !defined(PAD_SAVE_AREA)
    /* debug -- memset the new stack, unless we want valgrind's help */
    memset(stackPtr - (method->outsSize*4), 0xaf, stackReq);
#endif
#ifdef EASY_GDB
    breakSaveBlock->prevSave =
       (StackSaveArea*)FP_FROM_SAVEAREA(self->interpSave.curFrame);
    saveBlock->prevSave = breakSaveBlock;
#endif

    breakSaveBlock->prevFrame = self->interpSave.curFrame;
    breakSaveBlock->savedPc = NULL;             // not required
    breakSaveBlock->xtra.localRefCookie = 0;    // not required
    breakSaveBlock->method = NULL;
    saveBlock->prevFrame = FP_FROM_SAVEAREA(breakSaveBlock);
    saveBlock->savedPc = NULL;                  // not required
    saveBlock->xtra.currentPc = NULL;           // not required?
    saveBlock->method = method;

    LOGVV("PUSH frame: old=%p new=%p (size=%d)",
        self->interpSave.curFrame, FP_FROM_SAVEAREA(saveBlock),
        (u1*)self->interpSave.curFrame - (u1*)FP_FROM_SAVEAREA(saveBlock));

    self->interpSave.curFrame = FP_FROM_SAVEAREA(saveBlock);

    return true;
}

/*
 * We're calling a JNI native method from an internal VM fuction or
 * via reflection.  This is also used to create the "fake" native-method
 * frames at the top of the interpreted stack.
 *
 * This actually pushes two frames; the first is a "break" frame.
 *
 * The top frame has additional space for JNI local reference tracking.
 */
bool dvmPushJNIFrame(Thread* self, const Method* method)
{
    StackSaveArea* saveBlock;
    StackSaveArea* breakSaveBlock;
    int stackReq;
    u1* stackPtr;

    assert(dvmIsNativeMethod(method));

    stackReq = method->registersSize * 4        // params only
                + sizeof(StackSaveArea) * 2;    // break frame + regular frame

    if (self->interpSave.curFrame != NULL)
        stackPtr = (u1*) SAVEAREA_FROM_FP(self->interpSave.curFrame);
    else
        stackPtr = self->interpStackStart;

    if (stackPtr - stackReq < self->interpStackEnd) {
        /* not enough space */
        ALOGW("Stack overflow on call to native "
             "(req=%d top=%p cur=%p size=%d '%s')",
            stackReq, self->interpStackStart, self->interpSave.curFrame,
            self->interpStackSize, method->name);
        dvmHandleStackOverflow(self, method);
        assert(dvmCheckException(self));
        return false;
    }

    /*
     * Shift the stack pointer down, leaving space for just the stack save
     * area for the break frame, then shift down farther for the full frame.
     * We leave space for the method args, which are copied in later.
     */
    stackPtr -= sizeof(StackSaveArea);
    breakSaveBlock = (StackSaveArea*)stackPtr;
    stackPtr -= method->registersSize * 4 + sizeof(StackSaveArea);
    saveBlock = (StackSaveArea*) stackPtr;

#if !defined(NDEBUG) && !defined(PAD_SAVE_AREA)
    /* debug -- memset the new stack */
    memset(stackPtr, 0xaf, stackReq);
#endif
#ifdef EASY_GDB
    if (self->interpSave.curFrame == NULL)
        breakSaveBlock->prevSave = NULL;
    else {
        void* fp = FP_FROM_SAVEAREA(self->interpSave.curFrame);
        breakSaveBlock->prevSave = (StackSaveArea*)fp;
    }
    saveBlock->prevSave = breakSaveBlock;
#endif

    breakSaveBlock->prevFrame = self->interpSave.curFrame;
    breakSaveBlock->savedPc = NULL;             // not required
    breakSaveBlock->xtra.localRefCookie = 0;    // not required
    breakSaveBlock->method = NULL;
    saveBlock->prevFrame = FP_FROM_SAVEAREA(breakSaveBlock);
    saveBlock->savedPc = NULL;                  // not required
    saveBlock->xtra.localRefCookie = self->jniLocalRefTable.segmentState.all;
    saveBlock->method = method;

    LOGVV("PUSH JNI frame: old=%p new=%p (size=%d)",
        self->interpSave.curFrame, FP_FROM_SAVEAREA(saveBlock),
        (u1*)self->interpSave.curFrame - (u1*)FP_FROM_SAVEAREA(saveBlock));

    self->interpSave.curFrame = FP_FROM_SAVEAREA(saveBlock);

    return true;
}

/*
 * This is used by the JNI PushLocalFrame call.  We push a new frame onto
 * the stack that has no ins, outs, or locals, and no break frame above it.
 * It's strictly used for tracking JNI local refs, and will be popped off
 * by dvmPopFrame if it's not removed explicitly.
 */
bool dvmPushLocalFrame(Thread* self, const Method* method)
{
    StackSaveArea* saveBlock;
    int stackReq;
    u1* stackPtr;

    assert(dvmIsNativeMethod(method));

    stackReq = sizeof(StackSaveArea);       // regular frame

    assert(self->interpSave.curFrame != NULL);
    stackPtr = (u1*) SAVEAREA_FROM_FP(self->interpSave.curFrame);

    if (stackPtr - stackReq < self->interpStackEnd) {
        /* not enough space; let JNI throw the exception */
        ALOGW("Stack overflow on PushLocal "
             "(req=%d top=%p cur=%p size=%d '%s')",
            stackReq, self->interpStackStart, self->interpSave.curFrame,
            self->interpStackSize, method->name);
        dvmHandleStackOverflow(self, method);
        assert(dvmCheckException(self));
        return false;
    }

    /*
     * Shift the stack pointer down, leaving space for just the stack save
     * area for the break frame, then shift down farther for the full frame.
     */
    stackPtr -= sizeof(StackSaveArea);
    saveBlock = (StackSaveArea*) stackPtr;

#if !defined(NDEBUG) && !defined(PAD_SAVE_AREA)
    /* debug -- memset the new stack */
    memset(stackPtr, 0xaf, stackReq);
#endif
#ifdef EASY_GDB
    saveBlock->prevSave =
        (StackSaveArea*)FP_FROM_SAVEAREA(self->interpSave.curFrame);
#endif

    saveBlock->prevFrame = self->interpSave.curFrame;
    saveBlock->savedPc = NULL;                  // not required
    saveBlock->xtra.localRefCookie = self->jniLocalRefTable.segmentState.all;
    saveBlock->method = method;

    LOGVV("PUSH JNI local frame: old=%p new=%p (size=%d)",
        self->interpSave.curFrame, FP_FROM_SAVEAREA(saveBlock),
        (u1*)self->interpSave.curFrame - (u1*)FP_FROM_SAVEAREA(saveBlock));

    self->interpSave.curFrame = FP_FROM_SAVEAREA(saveBlock);

    return true;
}

/*
 * Pop one frame pushed on by JNI PushLocalFrame.
 *
 * If we've gone too far, the previous frame is either a break frame or
 * an interpreted frame.  Either way, the method pointer won't match.
 */
bool dvmPopLocalFrame(Thread* self)
{
    StackSaveArea* saveBlock = SAVEAREA_FROM_FP(self->interpSave.curFrame);

    assert(!dvmIsBreakFrame((u4*)self->interpSave.curFrame));
    if (saveBlock->method != SAVEAREA_FROM_FP(saveBlock->prevFrame)->method) {
        /*
         * The previous frame doesn't have the same method pointer -- we've
         * been asked to pop too much.
         */
        assert(dvmIsBreakFrame((u4*)saveBlock->prevFrame) ||
               !dvmIsNativeMethod(
                       SAVEAREA_FROM_FP(saveBlock->prevFrame)->method));
        return false;
    }

    LOGVV("POP JNI local frame: removing %s, now %s",
        saveBlock->method->name,
        SAVEAREA_FROM_FP(saveBlock->prevFrame)->method->name);
    dvmPopJniLocals(self, saveBlock);
    self->interpSave.curFrame = saveBlock->prevFrame;

    return true;
}

/*
 * Pop a frame we added.  There should be one method frame and one break
 * frame.
 *
 * If JNI Push/PopLocalFrame calls were mismatched, we might end up
 * popping multiple method frames before we find the break.
 *
 * Returns "false" if there was no frame to pop.
 */
static bool dvmPopFrame(Thread* self)
{
    StackSaveArea* saveBlock;

    if (self->interpSave.curFrame == NULL)
        return false;

    saveBlock = SAVEAREA_FROM_FP(self->interpSave.curFrame);
    assert(!dvmIsBreakFrame((u4*)self->interpSave.curFrame));

    /*
     * Remove everything up to the break frame.  If this was a call into
     * native code, pop the JNI local references table.
     */
    while (saveBlock->prevFrame != NULL && saveBlock->method != NULL) {
        /* probably a native->native JNI call */

        if (dvmIsNativeMethod(saveBlock->method)) {
            LOGVV("Popping JNI stack frame for %s.%s%s",
                saveBlock->method->clazz->descriptor,
                saveBlock->method->name,
                (SAVEAREA_FROM_FP(saveBlock->prevFrame)->method == NULL) ?
                "" : " (JNI local)");
            dvmPopJniLocals(self, saveBlock);
        }

        saveBlock = SAVEAREA_FROM_FP(saveBlock->prevFrame);
    }
    if (saveBlock->method != NULL) {
        ALOGE("PopFrame missed the break");
        assert(false);
        dvmAbort();     // stack trashed -- nowhere to go in this thread
    }

    LOGVV("POP frame: cur=%p new=%p",
        self->interpSave.curFrame, saveBlock->prevFrame);

    self->interpSave.curFrame = saveBlock->prevFrame;
    return true;
}

/*
 * Common code for dvmCallMethodV/A and dvmInvokeMethod.
 *
 * Pushes a call frame on, advancing self->interpSave.curFrame.
 */
static ClassObject* callPrep(Thread* self, const Method* method, Object* obj,
    bool checkAccess)
{
    ClassObject* clazz;

#ifndef NDEBUG
    if (self->status != THREAD_RUNNING) {
        ALOGW("threadid=%d: status=%d on call to %s.%s -",
            self->threadId, self->status,
            method->clazz->descriptor, method->name);
    }
#endif

    assert(self != NULL);
    assert(method != NULL);

    if (obj != NULL)
        clazz = obj->clazz;
    else
        clazz = method->clazz;

    IF_LOGVV() {
        char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
        LOGVV("thread=%d native code calling %s.%s %s", self->threadId,
            clazz->descriptor, method->name, desc);
        free(desc);
    }

    if (checkAccess) {
        /* needed for java.lang.reflect.Method.invoke */
        if (!dvmCheckMethodAccess(dvmGetCaller2Class(self->interpSave.curFrame),
                method))
        {
            /* note this throws IAException, not IAError */
            dvmThrowIllegalAccessException("access to method denied");
            return NULL;
        }
    }

    /*
     * Push a call frame on.  If there isn't enough room for ins, locals,
     * outs, and the saved state, it will throw an exception.
     *
     * This updates self->interpSave.curFrame.
     */
    if (dvmIsNativeMethod(method)) {
        /* native code calling native code the hard way */
        if (!dvmPushJNIFrame(self, method)) {
            assert(dvmCheckException(self));
            return NULL;
        }
    } else {
        /* native code calling interpreted code */
        if (!dvmPushInterpFrame(self, method)) {
            assert(dvmCheckException(self));
            return NULL;
        }
    }

    return clazz;
}

/*
 * Issue a method call.
 *
 * Pass in NULL for "obj" on calls to static methods.
 *
 * (Note this can't be inlined because it takes a variable number of args.)
 */
void dvmCallMethod(Thread* self, const Method* method, Object* obj,
    JValue* pResult, ...)
{
    va_list args;
    va_start(args, pResult);
    dvmCallMethodV(self, method, obj, false, pResult, args);
    va_end(args);
}

/*
 * Issue a method call with a variable number of arguments.  We process
 * the contents of "args" by scanning the method signature.
 *
 * Pass in NULL for "obj" on calls to static methods.
 *
 * We don't need to take the class as an argument because, in Dalvik,
 * we don't need to worry about static synchronized methods.
 */
void dvmCallMethodV(Thread* self, const Method* method, Object* obj,
    bool fromJni, JValue* pResult, va_list args)
{
    const char* desc = &(method->shorty[1]); // [0] is the return type.
    int verifyCount = 0;
    ClassObject* clazz;
    u4* ins;

    clazz = callPrep(self, method, obj, false);
    if (clazz == NULL)
        return;

    /* "ins" for new frame start at frame pointer plus locals */
    ins = ((u4*)self->interpSave.curFrame) +
           (method->registersSize - method->insSize);

    //ALOGD("  FP is %p, INs live at >= %p", self->interpSave.curFrame, ins);

    /* put "this" pointer into in0 if appropriate */
    if (!dvmIsStaticMethod(method)) {
#ifdef WITH_EXTRA_OBJECT_VALIDATION
        assert(obj != NULL && dvmIsHeapAddress(obj));
#endif
        *ins++ = (u4) obj;
        verifyCount++;
    }

    while (*desc != '\0') {
        switch (*(desc++)) {
            case 'D': case 'J': {
                u8 val = va_arg(args, u8);
                memcpy(ins, &val, 8);       // EABI prevents direct store
                ins += 2;
                verifyCount += 2;
                break;
            }
            case 'F': {
                /* floats were normalized to doubles; convert back */
                float f = (float) va_arg(args, double);
                *ins++ = dvmFloatToU4(f);
                verifyCount++;
                break;
            }
            case 'L': {     /* 'shorty' descr uses L for all refs, incl array */
                void* arg = va_arg(args, void*);
                assert(obj == NULL || dvmIsHeapAddress(obj));
                jobject argObj = reinterpret_cast<jobject>(arg);
                if (fromJni)
                    *ins++ = (u4) dvmDecodeIndirectRef(self, argObj);
                else
                    *ins++ = (u4) argObj;
                verifyCount++;
                break;
            }
            default: {
                /* Z B C S I -- all passed as 32-bit integers */
                *ins++ = va_arg(args, u4);
                verifyCount++;
                break;
            }
        }
    }

#ifndef NDEBUG
    if (verifyCount != method->insSize) {
        ALOGE("Got vfycount=%d insSize=%d for %s.%s", verifyCount,
            method->insSize, clazz->descriptor, method->name);
        assert(false);
        goto bail;
    }
#endif

    //dvmDumpThreadStack(dvmThreadSelf());

    if (dvmIsNativeMethod(method)) {
        TRACE_METHOD_ENTER(self, method);
        /*
         * Because we leave no space for local variables, "curFrame" points
         * directly at the method arguments.
         */
        (*method->nativeFunc)((u4*)self->interpSave.curFrame, pResult,
                              method, self);
        TRACE_METHOD_EXIT(self, method);
    } else {
        dvmInterpret(self, method, pResult);
    }

#ifndef NDEBUG
bail:
#endif
    dvmPopFrame(self);
}

/*
 * Issue a method call with arguments provided in an array.  We process
 * the contents of "args" by scanning the method signature.
 *
 * The values were likely placed into an uninitialized jvalue array using
 * the field specifiers, which means that sub-32-bit fields (e.g. short,
 * boolean) may not have 32 or 64 bits of valid data.  This is different
 * from the varargs invocation where the C compiler does a widening
 * conversion when calling a function.  As a result, we have to be a
 * little more precise when pulling stuff out.
 *
 * "args" may be NULL if the method has no arguments.
 */
void dvmCallMethodA(Thread* self, const Method* method, Object* obj,
    bool fromJni, JValue* pResult, const jvalue* args)
{
    const char* desc = &(method->shorty[1]); // [0] is the return type.
    int verifyCount = 0;
    ClassObject* clazz;
    u4* ins;

    clazz = callPrep(self, method, obj, false);
    if (clazz == NULL)
        return;

    /* "ins" for new frame start at frame pointer plus locals */
    ins = ((u4*)self->interpSave.curFrame) +
        (method->registersSize - method->insSize);

    /* put "this" pointer into in0 if appropriate */
    if (!dvmIsStaticMethod(method)) {
        assert(obj != NULL);
        *ins++ = (u4) obj;              /* obj is a "real" ref */
        verifyCount++;
    }

    while (*desc != '\0') {
        switch (*desc++) {
        case 'D':                       /* 64-bit quantity; have to use */
        case 'J':                       /*  memcpy() in case of mis-alignment */
            memcpy(ins, &args->j, 8);
            ins += 2;
            verifyCount++;              /* this needs an extra push */
            break;
        case 'L':                       /* includes array refs */
            if (fromJni)
                *ins++ = (u4) dvmDecodeIndirectRef(self, args->l);
            else
                *ins++ = (u4) args->l;
            break;
        case 'F':
        case 'I':
            *ins++ = args->i;           /* full 32 bits */
            break;
        case 'S':
            *ins++ = args->s;           /* 16 bits, sign-extended */
            break;
        case 'C':
            *ins++ = args->c;           /* 16 bits, unsigned */
            break;
        case 'B':
            *ins++ = args->b;           /* 8 bits, sign-extended */
            break;
        case 'Z':
            *ins++ = args->z;           /* 8 bits, zero or non-zero */
            break;
        default:
            ALOGE("Invalid char %c in short signature of %s.%s",
                *(desc-1), clazz->descriptor, method->name);
            assert(false);
            goto bail;
        }

        verifyCount++;
        args++;
    }

#ifndef NDEBUG
    if (verifyCount != method->insSize) {
        ALOGE("Got vfycount=%d insSize=%d for %s.%s", verifyCount,
            method->insSize, clazz->descriptor, method->name);
        assert(false);
        goto bail;
    }
#endif

    if (dvmIsNativeMethod(method)) {
        TRACE_METHOD_ENTER(self, method);
        /*
         * Because we leave no space for local variables, "curFrame" points
         * directly at the method arguments.
         */
        (*method->nativeFunc)((u4*)self->interpSave.curFrame, pResult,
                              method, self);
        TRACE_METHOD_EXIT(self, method);
    } else {
        dvmInterpret(self, method, pResult);
    }

bail:
    dvmPopFrame(self);
}

static void throwArgumentTypeMismatch(int argIndex, ClassObject* expected, DataObject* arg) {
    std::string expectedClassName(dvmHumanReadableDescriptor(expected->descriptor));
    std::string actualClassName = dvmHumanReadableType(arg);
    dvmThrowExceptionFmt(gDvm.exIllegalArgumentException, "argument %d should have type %s, got %s",
            argIndex + 1, expectedClassName.c_str(), actualClassName.c_str());
}

/*
 * Invoke a method, using the specified arguments and return type, through
 * one of the reflection interfaces.  Could be a virtual or direct method
 * (including constructors).  Used for reflection.
 *
 * Deals with boxing/unboxing primitives and performs widening conversions.
 *
 * "invokeObj" will be null for a static method.
 *
 * If the invocation returns with an exception raised, we have to wrap it.
 */
Object* dvmInvokeMethod(Object* obj, const Method* method,
    ArrayObject* argList, ArrayObject* params, ClassObject* returnType,
    bool noAccessCheck)
{
    ClassObject* clazz;
    Object* retObj = NULL;
    Thread* self = dvmThreadSelf();
    s4* ins;
    int verifyCount, argListLength;
    JValue retval;
    bool needPop = false;

    /* verify arg count */
    if (argList != NULL)
        argListLength = argList->length;
    else
        argListLength = 0;
    if (argListLength != (int) params->length) {
        dvmThrowExceptionFmt(gDvm.exIllegalArgumentException,
            "wrong number of arguments; expected %d, got %d",
            params->length, argListLength);
        return NULL;
    }

    clazz = callPrep(self, method, obj, !noAccessCheck);
    if (clazz == NULL)
        return NULL;
    needPop = true;

    /* "ins" for new frame start at frame pointer plus locals */
    ins = ((s4*)self->interpSave.curFrame) +
        (method->registersSize - method->insSize);
    verifyCount = 0;

    //ALOGD("  FP is %p, INs live at >= %p", self->interpSave.curFrame, ins);

    /* put "this" pointer into in0 if appropriate */
    if (!dvmIsStaticMethod(method)) {
        assert(obj != NULL);
        *ins++ = (s4) obj;
        verifyCount++;
    }

    /*
     * Copy the args onto the stack.  Primitive types are converted when
     * necessary, and object types are verified.
     */
    DataObject** args = (DataObject**)(void*)argList->contents;
    ClassObject** types = (ClassObject**)(void*)params->contents;
    for (int i = 0; i < argListLength; i++) {
        int width = dvmConvertArgument(*args++, *types++, ins);
        if (width < 0) {
            dvmPopFrame(self);      // throw wants to pull PC out of stack
            needPop = false;
            throwArgumentTypeMismatch(i, *(types-1), *(args-1));
            goto bail;
        }

        ins += width;
        verifyCount += width;
    }

#ifndef NDEBUG
    if (verifyCount != method->insSize) {
        ALOGE("Got vfycount=%d insSize=%d for %s.%s", verifyCount,
            method->insSize, clazz->descriptor, method->name);
        assert(false);
        goto bail;
    }
#endif

    if (dvmIsNativeMethod(method)) {
        TRACE_METHOD_ENTER(self, method);
        /*
         * Because we leave no space for local variables, "curFrame" points
         * directly at the method arguments.
         */
        (*method->nativeFunc)((u4*)self->interpSave.curFrame, &retval,
                              method, self);
        TRACE_METHOD_EXIT(self, method);
    } else {
        dvmInterpret(self, method, &retval);
    }

    /*
     * Pop the frame immediately.  The "wrap" calls below can cause
     * allocations, and we don't want the GC to walk the now-dead frame.
     */
    dvmPopFrame(self);
    needPop = false;

    /*
     * If an exception is raised, wrap and replace.  This is necessary
     * because the invoked method could have thrown a checked exception
     * that the caller wasn't prepared for.
     *
     * We might be able to do this up in the interpreted code, but that will
     * leave us with a shortened stack trace in the top-level exception.
     */
    if (dvmCheckException(self)) {
        dvmWrapException("Ljava/lang/reflect/InvocationTargetException;");
    } else {
        /*
         * If this isn't a void method or constructor, convert the return type
         * to an appropriate object.
         *
         * We don't do this when an exception is raised because the value
         * in "retval" is undefined.
         */
        if (returnType != NULL) {
            retObj = (Object*)dvmBoxPrimitive(retval, returnType);
            dvmReleaseTrackedAlloc(retObj, NULL);
        }
    }

bail:
    if (needPop) {
        dvmPopFrame(self);
    }
    return retObj;
}

struct LineNumFromPcContext {
    u4 address;
    u4 lineNum;
};

static int lineNumForPcCb(void *cnxt, u4 address, u4 lineNum)
{
    LineNumFromPcContext *pContext = (LineNumFromPcContext *)cnxt;

    // We know that this callback will be called in
    // ascending address order, so keep going until we find
    // a match or we've just gone past it.

    if (address > pContext->address) {
        // The line number from the previous positions callback
        // wil be the final result.
        return 1;
    }

    pContext->lineNum = lineNum;

    return (address == pContext->address) ? 1 : 0;
}

/*
 * Determine the source file line number based on the program counter.
 * "pc" is an offset, in 16-bit units, from the start of the method's code.
 *
 * Returns -1 if no match was found (possibly because the source files were
 * compiled without "-g", so no line number information is present).
 * Returns -2 for native methods (as expected in exception traces).
 */
int dvmLineNumFromPC(const Method* method, u4 relPc)
{
    const DexCode* pDexCode = dvmGetMethodCode(method);

    if (pDexCode == NULL) {
        if (dvmIsNativeMethod(method) && !dvmIsAbstractMethod(method))
            return -2;
        return -1;      /* can happen for abstract method stub */
    }

    LineNumFromPcContext context;
    memset(&context, 0, sizeof(context));
    context.address = relPc;
    // A method with no line number info should return -1
    context.lineNum = -1;

    dexDecodeDebugInfo(method->clazz->pDvmDex->pDexFile, pDexCode,
            method->clazz->descriptor,
            method->prototype.protoIdx,
            method->accessFlags,
            lineNumForPcCb, NULL, &context);

    return context.lineNum;
}

/*
 * Compute the frame depth.
 *
 * Excludes "break" frames.
 */
int dvmComputeExactFrameDepth(const void* fp)
{
    int count = 0;

    for ( ; fp != NULL; fp = SAVEAREA_FROM_FP(fp)->prevFrame) {
        if (!dvmIsBreakFrame((u4*)fp))
            count++;
    }

    return count;
}

/*
 * Compute the "vague" frame depth, which is just a pointer subtraction.
 * The result is NOT an overly generous assessment of the number of
 * frames; the only meaningful use is to compare against the result of
 * an earlier invocation.
 *
 * Useful for implementing single-step debugger modes, which may need to
 * call this for every instruction.
 */
int dvmComputeVagueFrameDepth(Thread* thread, const void* fp)
{
    const u1* interpStackStart = thread->interpStackStart;

    assert((u1*) fp >= interpStackStart - thread->interpStackSize);
    assert((u1*) fp < interpStackStart);
    return interpStackStart - (u1*) fp;
}

/*
 * Get the calling frame.  Pass in the current fp.
 *
 * Skip "break" frames and reflection invoke frames.
 */
void* dvmGetCallerFP(const void* curFrame)
{
    void* caller = SAVEAREA_FROM_FP(curFrame)->prevFrame;
    StackSaveArea* saveArea;

retry:
    if (dvmIsBreakFrame((u4*)caller)) {
        /* pop up one more */
        caller = SAVEAREA_FROM_FP(caller)->prevFrame;
        if (caller == NULL)
            return NULL;        /* hit the top */

        /*
         * If we got here by java.lang.reflect.Method.invoke(), we don't
         * want to return Method's class loader.  Shift up one and try
         * again.
         */
        saveArea = SAVEAREA_FROM_FP(caller);
        if (dvmIsReflectionMethod(saveArea->method)) {
            caller = saveArea->prevFrame;
            assert(caller != NULL);
            goto retry;
        }
    }

    return caller;
}

/*
 * Get the caller's class.  Pass in the current fp.
 *
 * This is used by e.g. java.lang.Class.
 */
ClassObject* dvmGetCallerClass(const void* curFrame)
{
    void* caller;

    caller = dvmGetCallerFP(curFrame);
    if (caller == NULL)
        return NULL;

    return SAVEAREA_FROM_FP(caller)->method->clazz;
}

/*
 * Get the caller's caller's class.  Pass in the current fp.
 *
 * This is used by e.g. java.lang.Class, which wants to know about the
 * class loader of the method that called it.
 */
ClassObject* dvmGetCaller2Class(const void* curFrame)
{
    void* caller = SAVEAREA_FROM_FP(curFrame)->prevFrame;
    void* callerCaller;

    /* at the top? */
    if (dvmIsBreakFrame((u4*)caller) &&
        SAVEAREA_FROM_FP(caller)->prevFrame == NULL)
        return NULL;

    /* go one more */
    callerCaller = dvmGetCallerFP(caller);
    if (callerCaller == NULL)
        return NULL;

    return SAVEAREA_FROM_FP(callerCaller)->method->clazz;
}

/*
 * Get the caller's caller's caller's class.  Pass in the current fp.
 *
 * This is used by e.g. java.lang.Class, which wants to know about the
 * class loader of the method that called it.
 */
ClassObject* dvmGetCaller3Class(const void* curFrame)
{
    void* caller = SAVEAREA_FROM_FP(curFrame)->prevFrame;
    int i;

    /* at the top? */
    if (dvmIsBreakFrame((u4*)caller) &&
        SAVEAREA_FROM_FP(caller)->prevFrame == NULL)
        return NULL;

    /* Walk up two frames if possible. */
    for (i = 0; i < 2; i++) {
        caller = dvmGetCallerFP(caller);
        if (caller == NULL)
            return NULL;
    }

    return SAVEAREA_FROM_FP(caller)->method->clazz;
}

/*
 * Fill a flat array of methods that comprise the current interpreter
 * stack trace.  Pass in the current frame ptr.  Break frames are
 * skipped, but reflection invocations are not.
 *
 * The current frame will be in element 0.
 */
void dvmFillStackTraceArray(const void* fp, const Method** array, size_t length)
{
    assert(fp != NULL);
    assert(array != NULL);
    size_t i = 0;
    while (fp != NULL) {
        if (!dvmIsBreakFrame((u4*)fp)) {
            assert(i < length);
            array[i++] = SAVEAREA_FROM_FP(fp)->method;
        }
        fp = SAVEAREA_FROM_FP(fp)->prevFrame;
    }
}

/*
 * Open up the reserved area and throw an exception.  The reserved area
 * should only be needed to create and initialize the exception itself.
 *
 * If we already opened it and we're continuing to overflow, abort the VM.
 *
 * We have to leave the "reserved" area open until the "catch" handler has
 * finished doing its processing.  This is because the catch handler may
 * need to resolve classes, which requires calling into the class loader if
 * the classes aren't already in the "initiating loader" list.
 */
void dvmHandleStackOverflow(Thread* self, const Method* method)
{
    /*
     * Can we make the reserved area available?
     */
    if (self->stackOverflowed) {
        /*
         * Already did, nothing to do but bail.
         */
        ALOGE("DalvikVM: double-overflow of stack in threadid=%d; aborting",
            self->threadId);
        dvmDumpThread(self, false);
        dvmAbort();
    }

    /* open it up to the full range */
    ALOGI("threadid=%d: stack overflow on call to %s.%s:%s",
        self->threadId,
        method->clazz->descriptor, method->name, method->shorty);
    StackSaveArea* saveArea = SAVEAREA_FROM_FP(self->interpSave.curFrame);
    ALOGI("  method requires %d+%d+%d=%d bytes, fp is %p (%d left)",
        method->registersSize * 4, sizeof(StackSaveArea), method->outsSize * 4,
        (method->registersSize + method->outsSize) * 4 + sizeof(StackSaveArea),
        saveArea, (u1*) saveArea - self->interpStackEnd);
    ALOGI("  expanding stack end (%p to %p)", self->interpStackEnd,
        self->interpStackStart - self->interpStackSize);
    //dvmDumpThread(self, false);
    self->interpStackEnd = self->interpStackStart - self->interpStackSize;
    self->stackOverflowed = true;

    /*
     * If we were trying to throw an exception when the stack overflowed,
     * we will blow up when doing the class lookup on StackOverflowError
     * because of the pending exception.  So, we clear it and make it
     * the cause of the SOE.
     */
    Object* excep = dvmGetException(self);
    if (excep != NULL) {
        ALOGW("Stack overflow while throwing exception");
        dvmClearException(self);
    }
    dvmThrowChainedException(gDvm.exStackOverflowError, NULL, excep);
}

/*
 * Reduce the available stack size.  By this point we should have finished
 * our overflow processing.
 */
void dvmCleanupStackOverflow(Thread* self, const Object* exception)
{
    const u1* newStackEnd;

    assert(self->stackOverflowed);

    if (exception->clazz != gDvm.exStackOverflowError) {
        /* exception caused during SOE, not the SOE itself */
        return;
    }

    newStackEnd = (self->interpStackStart - self->interpStackSize)
        + STACK_OVERFLOW_RESERVE;
    if ((u1*)self->interpSave.curFrame <= newStackEnd) {
        ALOGE("Can't shrink stack: curFrame is in reserved area (%p %p)",
            self->interpStackEnd, self->interpSave.curFrame);
        dvmDumpThread(self, false);
        dvmAbort();
    }

    self->interpStackEnd = newStackEnd;
    self->stackOverflowed = false;

    ALOGI("Shrank stack (to %p, curFrame is %p)", self->interpStackEnd,
        self->interpSave.curFrame);
}


/*
 * Extract the object that is the target of a monitor-enter instruction
 * in the top stack frame of "thread".
 *
 * The other thread might be alive, so this has to work carefully.
 *
 * The thread list lock must be held.
 *
 * Returns "true" if we successfully recover the object.  "*pOwner" will
 * be NULL if we can't determine the owner for some reason (e.g. race
 * condition on ownership transfer).
 */
static bool extractMonitorEnterObject(Thread* thread, Object** pLockObj,
    Thread** pOwner)
{
    void* framePtr = thread->interpSave.curFrame;

    if (framePtr == NULL || dvmIsBreakFrame((u4*)framePtr))
        return false;

    const StackSaveArea* saveArea = SAVEAREA_FROM_FP(framePtr);
    const Method* method = saveArea->method;
    const u2* currentPc = saveArea->xtra.currentPc;

    /* check Method* */
    if (!dvmLinearAllocContains(method, sizeof(Method))) {
        ALOGD("ExtrMon: method %p not valid", method);
        return false;
    }

    /* check currentPc */
    u4 insnsSize = dvmGetMethodInsnsSize(method);
    if (currentPc < method->insns ||
        currentPc >= method->insns + insnsSize)
    {
        ALOGD("ExtrMon: insns %p not valid (%p - %p)",
            currentPc, method->insns, method->insns + insnsSize);
        return false;
    }

    /* check the instruction */
    if ((*currentPc & 0xff) != OP_MONITOR_ENTER) {
        ALOGD("ExtrMon: insn at %p is not monitor-enter (0x%02x)",
            currentPc, *currentPc & 0xff);
        return false;
    }

    /* get and check the register index */
    unsigned int reg = *currentPc >> 8;
    if (reg >= method->registersSize) {
        ALOGD("ExtrMon: invalid register %d (max %d)",
            reg, method->registersSize);
        return false;
    }

    /* get and check the object in that register */
    u4* fp = (u4*) framePtr;
    Object* obj = (Object*) fp[reg];
    if (obj != NULL && !dvmIsHeapAddress(obj)) {
        ALOGD("ExtrMon: invalid object %p at %p[%d]", obj, fp, reg);
        return false;
    }
    *pLockObj = obj;

    /*
     * Try to determine the object's lock holder; it's okay if this fails.
     *
     * We're assuming the thread list lock is already held by this thread.
     * If it's not, we may be living dangerously if we have to scan through
     * the thread list to find a match.  (The VM will generally be in a
     * suspended state when executing here, so this is a minor concern
     * unless we're dumping while threads are running, in which case there's
     * a good chance of stuff blowing up anyway.)
     */
    *pOwner = dvmGetObjectLockHolder(obj);

    return true;
}

static void printWaitMessage(const DebugOutputTarget* target, const char* detail, Object* obj,
        Thread* thread)
{
    std::string msg(StringPrintf("  - waiting %s <%p> ", detail, obj));

    if (obj->clazz != gDvm.classJavaLangClass) {
        // I(16573)   - waiting on <0xf5feda38> (a java.util.LinkedList)
        // I(16573)   - waiting on <0xf5ed54f8> (a java.lang.Class<java.lang.ref.ReferenceQueue>)
        msg += "(a " + dvmHumanReadableType(obj) + ")";
    }

    if (thread != NULL) {
        std::string threadName(dvmGetThreadName(thread));
        StringAppendF(&msg, " held by tid=%d (%s)", thread->threadId, threadName.c_str());
    }

    dvmPrintDebugMessage(target, "%s\n", msg.c_str());
}

/*
 * Dump stack frames, starting from the specified frame and moving down.
 *
 * Each frame holds a pointer to the currently executing method, and the
 * saved program counter from the caller ("previous" frame).  This means
 * we don't have the PC for the current method on the stack, which is
 * pretty reasonable since it's in the "PC register" for the VM.  Because
 * exceptions need to show the correct line number we actually *do* have
 * an updated version in the fame's "xtra.currentPc", but it's unreliable.
 *
 * Note "framePtr" could be NULL in rare circumstances.
 */
static void dumpFrames(const DebugOutputTarget* target, void* framePtr,
    Thread* thread)
{
    const StackSaveArea* saveArea;
    const Method* method;
    int checkCount = 0;
    const u2* currentPc = NULL;
    bool first = true;

    /*
     * We call functions that require us to be holding the thread list lock.
     * It's probable that the caller has already done so, but it's not
     * guaranteed.  If it's not locked, lock it now.
     */
    bool needThreadUnlock = dvmTryLockThreadList();

    /*
     * The "currentPc" is updated whenever we execute an instruction that
     * might throw an exception.  Show it here.
     */
    if (framePtr != NULL && !dvmIsBreakFrame((u4*)framePtr)) {
        saveArea = SAVEAREA_FROM_FP(framePtr);

        if (saveArea->xtra.currentPc != NULL)
            currentPc = saveArea->xtra.currentPc;
    }

    while (framePtr != NULL) {
        saveArea = SAVEAREA_FROM_FP(framePtr);
        method = saveArea->method;

        if (dvmIsBreakFrame((u4*)framePtr)) {
            //dvmPrintDebugMessage(target, "  (break frame)\n");
        } else {
            int relPc;

            if (currentPc != NULL)
                relPc = currentPc - saveArea->method->insns;
            else
                relPc = -1;

            std::string methodName(dvmHumanReadableMethod(method, false));
            if (dvmIsNativeMethod(method)) {
                dvmPrintDebugMessage(target, "  at %s(Native Method)\n",
                        methodName.c_str());
            } else {
                dvmPrintDebugMessage(target, "  at %s(%s:%s%d)\n",
                        methodName.c_str(), dvmGetMethodSourceFile(method),
                        (relPc >= 0 && first) ? "~" : "",
                        relPc < 0 ? -1 : dvmLineNumFromPC(method, relPc));
            }

            if (first) {
                /*
                 * Decorate WAIT and MONITOR threads with some detail on
                 * the first frame.
                 *
                 * warning: wait status not stable, even in suspend
                 */
                if (thread->status == THREAD_WAIT ||
                    thread->status == THREAD_TIMED_WAIT)
                {
                    Monitor* mon = thread->waitMonitor;
                    Object* obj = dvmGetMonitorObject(mon);
                    if (obj != NULL) {
                        Thread* joinThread = NULL;
                        if (obj->clazz == gDvm.classJavaLangVMThread) {
                            joinThread = dvmGetThreadFromThreadObject(obj);
                        }
                        if (joinThread == NULL) {
                            joinThread = dvmGetObjectLockHolder(obj);
                        }
                        printWaitMessage(target, "on", obj, joinThread);
                    }
                } else if (thread->status == THREAD_MONITOR) {
                    Object* obj;
                    Thread* owner;
                    if (extractMonitorEnterObject(thread, &obj, &owner)) {
                        printWaitMessage(target, "to lock", obj, owner);
                    }
                }
            }
        }

        /*
         * Get saved PC for previous frame.  There's no savedPc in a "break"
         * frame, because that represents native or interpreted code
         * invoked by the VM.  The saved PC is sitting in the "PC register",
         * a local variable on the native stack.
         */
        currentPc = saveArea->savedPc;

        first = false;

        if (saveArea->prevFrame != NULL && saveArea->prevFrame <= framePtr) {
            ALOGW("Warning: loop in stack trace at frame %d (%p -> %p)",
                checkCount, framePtr, saveArea->prevFrame);
            break;
        }
        framePtr = saveArea->prevFrame;

        checkCount++;
        if (checkCount > 300) {
            dvmPrintDebugMessage(target,
                "  ***** printed %d frames, not showing any more\n",
                checkCount);
            break;
        }
    }

    if (needThreadUnlock) {
        dvmUnlockThreadList();
    }
}


/*
 * Dump the stack for the specified thread.
 */
void dvmDumpThreadStack(const DebugOutputTarget* target, Thread* thread)
{
    dumpFrames(target, thread->interpSave.curFrame, thread);
}

/*
 * Dump the stack for the specified thread, which is still running.
 *
 * This is very dangerous, because stack frames are being pushed on and
 * popped off, and if the thread exits we'll be looking at freed memory.
 * The plan here is to take a snapshot of the stack and then dump that
 * to try to minimize the chances of catching it mid-update.  This should
 * work reasonably well on a single-CPU system.
 *
 * There is a small chance that calling here will crash the VM.
 */
void dvmDumpRunningThreadStack(const DebugOutputTarget* target, Thread* thread)
{
    StackSaveArea* saveArea;
    const u1* origStack;
    u1* stackCopy = NULL;
    int origSize, fpOffset;
    void* fp;
    int depthLimit = 200;

    if (thread == NULL || thread->interpSave.curFrame == NULL) {
        dvmPrintDebugMessage(target,
            "DumpRunning: Thread at %p has no curFrame (threadid=%d)\n",
            thread, (thread != NULL) ? thread->threadId : 0);
        return;
    }

    /* wait for a full quantum */
    sched_yield();

    /* copy the info we need, then the stack itself */
    origSize = thread->interpStackSize;
    origStack = (const u1*) thread->interpStackStart - origSize;
    stackCopy = (u1*) malloc(origSize);
    fpOffset = (u1*) thread->interpSave.curFrame - origStack;
    memcpy(stackCopy, origStack, origSize);

    /*
     * Run through the stack and rewrite the "prev" pointers.
     */
    //ALOGI("DR: fpOff=%d (from %p %p)",fpOffset, origStack,
    //     thread->interpSave.curFrame);
    fp = stackCopy + fpOffset;
    while (true) {
        int prevOffset;

        if (depthLimit-- < 0) {
            /* we're probably screwed */
            dvmPrintDebugMessage(target, "DumpRunning: depth limit hit\n");
            dvmAbort();
        }
        saveArea = SAVEAREA_FROM_FP(fp);
        if (saveArea->prevFrame == NULL)
            break;

        prevOffset = (u1*) saveArea->prevFrame - origStack;
        if (prevOffset < 0 || prevOffset > origSize) {
            dvmPrintDebugMessage(target,
                "DumpRunning: bad offset found: %d (from %p %p)\n",
                prevOffset, origStack, saveArea->prevFrame);
            saveArea->prevFrame = NULL;
            break;
        }

        saveArea->prevFrame = (u4*)(stackCopy + prevOffset);
        fp = saveArea->prevFrame;
    }

    /*
     * We still need to pass the Thread for some monitor wait stuff.
     */
    dumpFrames(target, stackCopy + fpOffset, thread);
    free(stackCopy);
}

/*
 * Dump the native stack for the specified thread.
 */
void dvmDumpNativeStack(const DebugOutputTarget* target, pid_t tid)
{
#ifdef HAVE_ANDROID_OS
    const size_t MAX_DEPTH = 32;
    backtrace_frame_t backtrace[MAX_DEPTH];
    ssize_t frames = unwind_backtrace_thread(tid, backtrace, 0, MAX_DEPTH);
    if (frames > 0) {
        backtrace_symbol_t backtrace_symbols[MAX_DEPTH];
        get_backtrace_symbols(backtrace, frames, backtrace_symbols);

        for (size_t i = 0; i < size_t(frames); i++) {
            char line[MAX_BACKTRACE_LINE_LENGTH];
            format_backtrace_line(i, &backtrace[i], &backtrace_symbols[i],
                    line, MAX_BACKTRACE_LINE_LENGTH);
            dvmPrintDebugMessage(target, "  %s\n", line);
        }

        free_backtrace_symbols(backtrace_symbols, frames);
    } else {
        dvmPrintDebugMessage(target, "  (native backtrace unavailable)\n");
    }
#endif
}
