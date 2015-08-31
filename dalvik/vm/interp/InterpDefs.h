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
 * Dalvik interpreter definitions.  These are internal to the interpreter.
 *
 * This includes defines, types, function declarations, and inline functions
 * that are common to all interpreter implementations.
 *
 * Functions and globals declared here are defined in Interp.c.
 */
#ifndef DALVIK_INTERP_DEFS_H_
#define DALVIK_INTERP_DEFS_H_

#if defined(WITH_JIT)
/*
 * Size of save area for callee-save FP regs, which are not automatically
 * saved by interpreter main because it doesn't use them (but Jit'd code
 * may). Save/restore routine is defined by target, and size should
 * be >= max needed by any target.
 */
#define JIT_CALLEE_SAVE_DOUBLE_COUNT 8

#endif

/*
 * Portable interpreter.
 */
extern void dvmInterpretPortable(Thread* self);

/*
 * "mterp" interpreter.
 */
extern void dvmMterpStd(Thread* self);

/*
 * Get the "this" pointer from the current frame.
 */
Object* dvmGetThisPtr(const Method* method, const u4* fp);

/*
 * Verify that our tracked local references are valid.
 */
void dvmInterpCheckTrackedRefs(Thread* self, const Method* method,
    int debugTrackedRefStart);

/*
 * Process switch statement.
 */
extern "C" s4 dvmInterpHandlePackedSwitch(const u2* switchData, s4 testVal);
extern "C" s4 dvmInterpHandleSparseSwitch(const u2* switchData, s4 testVal);

/*
 * Process fill-array-data.
 */
extern "C" bool dvmInterpHandleFillArrayData(ArrayObject* arrayObject,
                                  const u2* arrayData);

/*
 * Find an interface method.
 */
Method* dvmInterpFindInterfaceMethod(ClassObject* thisClass, u4 methodIdx,
    const Method* method, DvmDex* methodClassDex);

/*
 * Determine if the debugger or profiler is currently active.
 */
static inline bool dvmDebuggerOrProfilerActive()
{
    return gDvm.debuggerActive || gDvm.activeProfilers != 0;
}

#if defined(WITH_JIT)
/*
 * Determine if the jit, debugger or profiler is currently active.  Used when
 * selecting which interpreter to switch to.
 */
static inline bool dvmJitDebuggerOrProfilerActive()
{
    return (gDvmJit.pProfTable != NULL) || dvmDebuggerOrProfilerActive();
}

/*
 * Hide the translations and stick with the interpreter as long as one of the
 * following conditions is true.
 */
static inline bool dvmJitHideTranslation()
{
    return (gDvm.sumThreadSuspendCount != 0) ||
           (gDvmJit.codeCacheFull == true) ||
           (gDvmJit.pProfTable == NULL);
}

#endif

/*
 * Construct an s4 from two consecutive half-words of switch data.
 * This needs to check endianness because the DEX optimizer only swaps
 * half-words in instruction stream.
 *
 * "switchData" must be 32-bit aligned.
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline s4 s4FromSwitchData(const void* switchData) {
    return *(s4*) switchData;
}
#else
static inline s4 s4FromSwitchData(const void* switchData) {
    u2* data = switchData;
    return data[0] | (((s4) data[1]) << 16);
}
#endif

#endif  // DALVIK_INTERP_DEFS_H_
