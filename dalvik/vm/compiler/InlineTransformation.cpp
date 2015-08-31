/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include "Dalvik.h"
#include "Dataflow.h"
#include "libdex/DexOpcodes.h"

/* Convert the reg id from the callee to the original id passed by the caller */
static inline u4 convertRegId(const DecodedInstruction *invoke,
                              const Method *calleeMethod,
                              int calleeRegId, bool isRange)
{
    /* The order in the original arg passing list */
    int rank = calleeRegId -
               (calleeMethod->registersSize - calleeMethod->insSize);
    assert(rank >= 0);
    if (!isRange) {
        return invoke->arg[rank];
    } else {
        return invoke->vC + rank;
    }
}

static bool inlineGetter(CompilationUnit *cUnit,
                         const Method *calleeMethod,
                         MIR *invokeMIR,
                         BasicBlock *invokeBB,
                         bool isPredicted,
                         bool isRange)
{
    BasicBlock *moveResultBB = invokeBB->fallThrough;
    MIR *moveResultMIR = moveResultBB->firstMIRInsn;
    MIR *newGetterMIR = (MIR *)dvmCompilerNew(sizeof(MIR), true);
    DecodedInstruction getterInsn;

    /*
     * Not all getter instructions have vC but vC will be read by
     * dvmCompilerGetDalvikDisassembly unconditionally.
     * Initialize it here to get Valgrind happy.
     */
    getterInsn.vC = 0;

    dexDecodeInstruction(calleeMethod->insns, &getterInsn);

    if (!dvmCompilerCanIncludeThisInstruction(calleeMethod, &getterInsn))
        return false;

    /*
     * Some getters (especially invoked through interface) are not followed
     * by a move result.
     */
    if ((moveResultMIR == NULL) ||
        (moveResultMIR->dalvikInsn.opcode != OP_MOVE_RESULT &&
         moveResultMIR->dalvikInsn.opcode != OP_MOVE_RESULT_OBJECT &&
         moveResultMIR->dalvikInsn.opcode != OP_MOVE_RESULT_WIDE)) {
        return false;
    }

    int dfFlags = dvmCompilerDataFlowAttributes[getterInsn.opcode];

    /* Expecting vA to be the destination register */
    if (dfFlags & (DF_UA | DF_UA_WIDE)) {
        ALOGE("opcode %d has DF_UA set (not expected)", getterInsn.opcode);
        dvmAbort();
    }

    if (dfFlags & DF_UB) {
        getterInsn.vB = convertRegId(&invokeMIR->dalvikInsn, calleeMethod,
                                     getterInsn.vB, isRange);
    }

    if (dfFlags & DF_UC) {
        getterInsn.vC = convertRegId(&invokeMIR->dalvikInsn, calleeMethod,
                                     getterInsn.vC, isRange);
    }

    getterInsn.vA = moveResultMIR->dalvikInsn.vA;

    /* Now setup the Dalvik instruction with converted src/dst registers */
    newGetterMIR->dalvikInsn = getterInsn;

    newGetterMIR->width = dexGetWidthFromOpcode(getterInsn.opcode);

    newGetterMIR->OptimizationFlags |= MIR_CALLEE;

    /*
     * If the getter instruction is about to raise any exception, punt to the
     * interpreter and re-execute the invoke.
     */
    newGetterMIR->offset = invokeMIR->offset;

    newGetterMIR->meta.calleeMethod = calleeMethod;

    dvmCompilerInsertMIRAfter(invokeBB, invokeMIR, newGetterMIR);

    if (isPredicted) {
        MIR *invokeMIRSlow = (MIR *)dvmCompilerNew(sizeof(MIR), true);
        *invokeMIRSlow = *invokeMIR;
        invokeMIR->dalvikInsn.opcode = (Opcode)kMirOpCheckInlinePrediction;

        /* Use vC to denote the first argument (ie this) */
        if (!isRange) {
            invokeMIR->dalvikInsn.vC = invokeMIRSlow->dalvikInsn.arg[0];
        }

        moveResultMIR->OptimizationFlags |= MIR_INLINED_PRED;

        dvmCompilerInsertMIRAfter(invokeBB, newGetterMIR, invokeMIRSlow);
        invokeMIRSlow->OptimizationFlags |= MIR_INLINED_PRED;
#if defined(WITH_JIT_TUNING)
        gDvmJit.invokePolyGetterInlined++;
#endif
    } else {
        invokeMIR->OptimizationFlags |= MIR_INLINED;
        moveResultMIR->OptimizationFlags |= MIR_INLINED;
#if defined(WITH_JIT_TUNING)
        gDvmJit.invokeMonoGetterInlined++;
#endif
    }

    return true;
}

static bool inlineSetter(CompilationUnit *cUnit,
                         const Method *calleeMethod,
                         MIR *invokeMIR,
                         BasicBlock *invokeBB,
                         bool isPredicted,
                         bool isRange)
{
    MIR *newSetterMIR = (MIR *)dvmCompilerNew(sizeof(MIR), true);
    DecodedInstruction setterInsn;

    /*
     * Not all setter instructions have vC but vC will be read by
     * dvmCompilerGetDalvikDisassembly unconditionally.
     * Initialize it here to get Valgrind happy.
     */
    setterInsn.vC = 0;

    dexDecodeInstruction(calleeMethod->insns, &setterInsn);

    if (!dvmCompilerCanIncludeThisInstruction(calleeMethod, &setterInsn))
        return false;

    int dfFlags = dvmCompilerDataFlowAttributes[setterInsn.opcode];

    if (dfFlags & (DF_UA | DF_UA_WIDE)) {
        setterInsn.vA = convertRegId(&invokeMIR->dalvikInsn, calleeMethod,
                                     setterInsn.vA, isRange);

    }

    if (dfFlags & DF_UB) {
        setterInsn.vB = convertRegId(&invokeMIR->dalvikInsn, calleeMethod,
                                     setterInsn.vB, isRange);

    }

    if (dfFlags & DF_UC) {
        setterInsn.vC = convertRegId(&invokeMIR->dalvikInsn, calleeMethod,
                                     setterInsn.vC, isRange);
    }

    /* Now setup the Dalvik instruction with converted src/dst registers */
    newSetterMIR->dalvikInsn = setterInsn;

    newSetterMIR->width = dexGetWidthFromOpcode(setterInsn.opcode);

    newSetterMIR->OptimizationFlags |= MIR_CALLEE;

    /*
     * If the setter instruction is about to raise any exception, punt to the
     * interpreter and re-execute the invoke.
     */
    newSetterMIR->offset = invokeMIR->offset;

    newSetterMIR->meta.calleeMethod = calleeMethod;

    dvmCompilerInsertMIRAfter(invokeBB, invokeMIR, newSetterMIR);

    if (isPredicted) {
        MIR *invokeMIRSlow = (MIR *)dvmCompilerNew(sizeof(MIR), true);
        *invokeMIRSlow = *invokeMIR;
        invokeMIR->dalvikInsn.opcode = (Opcode)kMirOpCheckInlinePrediction;

        /* Use vC to denote the first argument (ie this) */
        if (!isRange) {
            invokeMIR->dalvikInsn.vC = invokeMIRSlow->dalvikInsn.arg[0];
        }

        dvmCompilerInsertMIRAfter(invokeBB, newSetterMIR, invokeMIRSlow);
        invokeMIRSlow->OptimizationFlags |= MIR_INLINED_PRED;
#if defined(WITH_JIT_TUNING)
        gDvmJit.invokePolySetterInlined++;
#endif
    } else {
        /*
         * The invoke becomes no-op so it needs an explicit branch to jump to
         * the chaining cell.
         */
        invokeBB->needFallThroughBranch = true;
        invokeMIR->OptimizationFlags |= MIR_INLINED;
#if defined(WITH_JIT_TUNING)
        gDvmJit.invokeMonoSetterInlined++;
#endif
    }

    return true;
}

static bool tryInlineSingletonCallsite(CompilationUnit *cUnit,
                                       const Method *calleeMethod,
                                       MIR *invokeMIR,
                                       BasicBlock *invokeBB,
                                       bool isRange)
{
    /* Not a Java method */
    if (dvmIsNativeMethod(calleeMethod)) return false;

    CompilerMethodStats *methodStats =
        dvmCompilerAnalyzeMethodBody(calleeMethod, true);

    /* Empty callee - do nothing */
    if (methodStats->attributes & METHOD_IS_EMPTY) {
        /* The original invoke instruction is effectively turned into NOP */
        invokeMIR->OptimizationFlags |= MIR_INLINED;
        /*
         * Need to insert an explicit branch to catch the falling knife (into
         * the PC reconstruction or chaining cell).
         */
        invokeBB->needFallThroughBranch = true;
        return true;
    }

    if (methodStats->attributes & METHOD_IS_GETTER) {
        return inlineGetter(cUnit, calleeMethod, invokeMIR, invokeBB, false,
                            isRange);
    } else if (methodStats->attributes & METHOD_IS_SETTER) {
        return inlineSetter(cUnit, calleeMethod, invokeMIR, invokeBB, false,
                            isRange);
    }
    return false;
}

static bool inlineEmptyVirtualCallee(CompilationUnit *cUnit,
                                     const Method *calleeMethod,
                                     MIR *invokeMIR,
                                     BasicBlock *invokeBB)
{
    MIR *invokeMIRSlow = (MIR *)dvmCompilerNew(sizeof(MIR), true);
    *invokeMIRSlow = *invokeMIR;
    invokeMIR->dalvikInsn.opcode = (Opcode)kMirOpCheckInlinePrediction;

    dvmCompilerInsertMIRAfter(invokeBB, invokeMIR, invokeMIRSlow);
    invokeMIRSlow->OptimizationFlags |= MIR_INLINED_PRED;
    return true;
}

static bool tryInlineVirtualCallsite(CompilationUnit *cUnit,
                                     const Method *calleeMethod,
                                     MIR *invokeMIR,
                                     BasicBlock *invokeBB,
                                     bool isRange)
{
    /* Not a Java method */
    if (dvmIsNativeMethod(calleeMethod)) return false;

    CompilerMethodStats *methodStats =
        dvmCompilerAnalyzeMethodBody(calleeMethod, true);

    /* Empty callee - do nothing by checking the clazz pointer */
    if (methodStats->attributes & METHOD_IS_EMPTY) {
        return inlineEmptyVirtualCallee(cUnit, calleeMethod, invokeMIR,
                                        invokeBB);
    }

    if (methodStats->attributes & METHOD_IS_GETTER) {
        return inlineGetter(cUnit, calleeMethod, invokeMIR, invokeBB, true,
                            isRange);
    } else if (methodStats->attributes & METHOD_IS_SETTER) {
        return inlineSetter(cUnit, calleeMethod, invokeMIR, invokeBB, true,
                            isRange);
    }
    return false;
}


void dvmCompilerInlineMIR(CompilationUnit *cUnit, JitTranslationInfo *info)
{
    bool isRange = false;
    GrowableListIterator iterator;

    dvmGrowableListIteratorInit(&cUnit->blockList, &iterator);
    /*
     * Analyze the basic block containing an invoke to see if it can be inlined
     */
    while (true) {
        BasicBlock *bb = (BasicBlock *) dvmGrowableListIteratorNext(&iterator);
        if (bb == NULL) break;
        if (bb->blockType != kDalvikByteCode)
            continue;
        MIR *lastMIRInsn = bb->lastMIRInsn;
        Opcode opcode = lastMIRInsn->dalvikInsn.opcode;
        int flags = (int)dexGetFlagsFromOpcode(opcode);

        /* No invoke - continue */
        if ((flags & kInstrInvoke) == 0)
            continue;

        /* Disable inlining when doing method tracing */
        if (gDvmJit.methodTraceSupport)
            continue;

        /*
         * If the invoke itself is selected for single stepping, don't bother
         * to inline it.
         */
        if (SINGLE_STEP_OP(opcode))
            continue;

        const Method *calleeMethod;

        switch (opcode) {
            case OP_INVOKE_SUPER:
            case OP_INVOKE_DIRECT:
            case OP_INVOKE_STATIC:
            case OP_INVOKE_SUPER_QUICK:
                calleeMethod = lastMIRInsn->meta.callsiteInfo->method;
                break;
            case OP_INVOKE_SUPER_RANGE:
            case OP_INVOKE_DIRECT_RANGE:
            case OP_INVOKE_STATIC_RANGE:
            case OP_INVOKE_SUPER_QUICK_RANGE:
                isRange = true;
                calleeMethod = lastMIRInsn->meta.callsiteInfo->method;
                break;
            default:
                calleeMethod = NULL;
                break;
        }

        if (calleeMethod) {
            bool inlined = tryInlineSingletonCallsite(cUnit, calleeMethod,
                                                      lastMIRInsn, bb, isRange);
            if (!inlined &&
                !(gDvmJit.disableOpt & (1 << kMethodJit)) &&
                !dvmIsNativeMethod(calleeMethod)) {
                CompilerMethodStats *methodStats =
                    dvmCompilerAnalyzeMethodBody(calleeMethod, true);
                if ((methodStats->attributes & METHOD_IS_LEAF) &&
                    !(methodStats->attributes & METHOD_CANNOT_COMPILE)) {
                    /* Callee has been previously compiled */
                    if (dvmJitGetMethodAddr(calleeMethod->insns)) {
                        lastMIRInsn->OptimizationFlags |= MIR_INVOKE_METHOD_JIT;
                    } else {
                        /* Compile the callee first */
                        dvmCompileMethod(calleeMethod, info);
                        if (dvmJitGetMethodAddr(calleeMethod->insns)) {
                            lastMIRInsn->OptimizationFlags |=
                                MIR_INVOKE_METHOD_JIT;
                        } else {
                            methodStats->attributes |= METHOD_CANNOT_COMPILE;
                        }
                    }
                }
            }
            return;
        }

        switch (opcode) {
            case OP_INVOKE_VIRTUAL:
            case OP_INVOKE_VIRTUAL_QUICK:
            case OP_INVOKE_INTERFACE:
                isRange = false;
                calleeMethod = lastMIRInsn->meta.callsiteInfo->method;
                break;
            case OP_INVOKE_VIRTUAL_RANGE:
            case OP_INVOKE_VIRTUAL_QUICK_RANGE:
            case OP_INVOKE_INTERFACE_RANGE:
                isRange = true;
                calleeMethod = lastMIRInsn->meta.callsiteInfo->method;
                break;
            default:
                break;
        }

        if (calleeMethod) {
            bool inlined = tryInlineVirtualCallsite(cUnit, calleeMethod,
                                                    lastMIRInsn, bb, isRange);
            if (!inlined &&
                !(gDvmJit.disableOpt & (1 << kMethodJit)) &&
                !dvmIsNativeMethod(calleeMethod)) {
                CompilerMethodStats *methodStats =
                    dvmCompilerAnalyzeMethodBody(calleeMethod, true);
                if ((methodStats->attributes & METHOD_IS_LEAF) &&
                    !(methodStats->attributes & METHOD_CANNOT_COMPILE)) {
                    /* Callee has been previously compiled */
                    if (dvmJitGetMethodAddr(calleeMethod->insns)) {
                        lastMIRInsn->OptimizationFlags |= MIR_INVOKE_METHOD_JIT;
                    } else {
                        /* Compile the callee first */
                        dvmCompileMethod(calleeMethod, info);
                        if (dvmJitGetMethodAddr(calleeMethod->insns)) {
                            lastMIRInsn->OptimizationFlags |=
                                MIR_INVOKE_METHOD_JIT;
                        } else {
                            methodStats->attributes |= METHOD_CANNOT_COMPILE;
                        }
                    }
                }
            }
            return;
        }
    }
}
