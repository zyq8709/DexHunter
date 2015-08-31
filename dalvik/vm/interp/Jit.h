/*
 * Copyright (C) 2009 The Android Open Source Project
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
 * Jit control
 */
#ifndef DALVIK_INTERP_JIT_H_
#define DALVIK_INTERP_JIT_H_

#include "InterpDefs.h"
#include "mterp/common/jit-config.h"

#define JIT_MAX_TRACE_LEN 100

#if defined (WITH_SELF_VERIFICATION)

#define REG_SPACE 256                /* default size of shadow space */
#define HEAP_SPACE JIT_MAX_TRACE_LEN /* default size of heap space */

struct ShadowHeap {
    int addr;
    int data;
};

struct InstructionTrace {
    int addr;
    DecodedInstruction decInsn;
};

struct ShadowSpace {
    const u2* startPC;          /* starting pc of jitted region */
    u4* fp;                     /* starting fp of jitted region */
    const Method *method;
    DvmDex* methodClassDex;
    JValue retval;
    const u1* interpStackEnd;
    SelfVerificationState jitExitState;  /* exit point for JIT'ed code */
    SelfVerificationState selfVerificationState;  /* current SV running state */
    const u2* endPC;            /* ending pc of jitted region */
    void* shadowFP;       /* pointer to fp in shadow space */
    int* registerSpace;         /* copy of register state */
    int registerSpaceSize;      /* current size of register space */
    ShadowHeap heapSpace[HEAP_SPACE]; /* copy of heap space */
    ShadowHeap* heapSpaceTail;        /* tail pointer to heapSpace */
    const void* endShadowFP;    /* ending fp in shadow space */
    InstructionTrace trace[JIT_MAX_TRACE_LEN]; /* opcode trace for debugging */
    int traceLength;            /* counter for current trace length */
};

/*
 * Self verification functions.
 */
extern "C" {
void* dvmSelfVerificationShadowSpaceAlloc(Thread* self);
void dvmSelfVerificationShadowSpaceFree(Thread* self);
void* dvmSelfVerificationSaveState(const u2* pc, u4* fp,
                                   Thread* self,
                                   int targetTrace);
void* dvmSelfVerificationRestoreState(const u2* pc, u4* fp,
                                      SelfVerificationState exitPoint,
                                      Thread *self);
void dvmCheckSelfVerification(const u2* pc, Thread* self);
}
#endif

/*
 * Offsets for metadata in the trace run array from the trace that ends with
 * invoke instructions.
 */
#define JIT_TRACE_CLASS_DESC    1
#define JIT_TRACE_CLASS_LOADER  2
#define JIT_TRACE_CUR_METHOD    3

/*
 * JitTable hash function.
 */

static inline u4 dvmJitHashMask( const u2* p, u4 mask ) {
    return ((((u4)p>>12)^(u4)p)>>1) & (mask);
}

static inline u4 dvmJitHash( const u2* p ) {
    return dvmJitHashMask( p, gDvmJit.jitTableMask );
}

/*
 * The width of the chain field in JitEntryInfo sets the upper
 * bound on the number of translations.  Be careful if changing
 * the size of JitEntry struct - the Dalvik PC to JitEntry
 * hash functions have built-in knowledge of the size.
 */
#define JIT_ENTRY_CHAIN_WIDTH 2
#define JIT_MAX_ENTRIES (1 << (JIT_ENTRY_CHAIN_WIDTH * 8))

/*
 * The trace profiling counters are allocated in blocks and individual
 * counters must not move so long as any referencing trace exists.
 */
#define JIT_PROF_BLOCK_ENTRIES 1024
#define JIT_PROF_BLOCK_BUCKETS (JIT_MAX_ENTRIES / JIT_PROF_BLOCK_ENTRIES)

typedef s4 JitTraceCounter_t;

struct JitTraceProfCounters {
    unsigned int           next;
    JitTraceCounter_t      *buckets[JIT_PROF_BLOCK_BUCKETS];
};

/*
 * Entries in the JIT's address lookup hash table.
 * Fields which may be updated by multiple threads packed into a
 * single 32-bit word to allow use of atomic update.
 */

struct JitEntryInfo {
    unsigned int           isMethodEntry:1;
    unsigned int           inlineCandidate:1;
    unsigned int           profileEnabled:1;
    JitInstructionSetType  instructionSet:3;
    unsigned int           profileOffset:5;
    unsigned int           unused:5;
    u2                     chain;                 /* Index of next in chain */
};

union JitEntryInfoUnion {
    JitEntryInfo info;
    volatile int infoWord;
};

struct JitEntry {
    JitEntryInfoUnion   u;
    const u2*           dPC;            /* Dalvik code address */
    void*               codeAddress;    /* Code address of native translation */
};

extern "C" {
void dvmCheckJit(const u2* pc, Thread* self);
void* dvmJitGetTraceAddr(const u2* dPC);
void* dvmJitGetMethodAddr(const u2* dPC);
void* dvmJitGetTraceAddrThread(const u2* dPC, Thread* self);
void* dvmJitGetMethodAddrThread(const u2* dPC, Thread* self);
void dvmJitCheckTraceRequest(Thread* self);
void dvmJitStopTranslationRequests(void);
#if defined(WITH_JIT_TUNING)
void dvmBumpNoChain(int from);
void dvmBumpNormal(void);
void dvmBumpPunt(int from);
#endif
void dvmJitStats(void);
bool dvmJitResizeJitTable(unsigned int size);
void dvmJitResetTable(void);
JitEntry *dvmJitFindEntry(const u2* pc, bool isMethodEntry);
s8 dvmJitd2l(double d);
s8 dvmJitf2l(float f);
void dvmJitSetCodeAddr(const u2* dPC, void *nPC, JitInstructionSetType set,
                       bool isMethodEntry, int profilePrefixSize);
void dvmJitEndTraceSelect(Thread* self, const u2* dPC);
JitTraceCounter_t *dvmJitNextTraceCounter(void);
void dvmJitTraceProfilingOff(void);
void dvmJitTraceProfilingOn(void);
void dvmJitChangeProfileMode(TraceProfilingModes newState);
void dvmJitDumpTraceDesc(JitTraceDescription *trace);
void dvmJitUpdateThreadStateSingle(Thread* threead);
void dvmJitUpdateThreadStateAll(void);
void dvmJitResumeTranslation(Thread* self, const u2* pc, const u4* fp);
}

#endif  // DALVIK_INTERP_JIT_H_
