/*
 * Copyright (C) 2011 The Android Open Source Project
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

#if 0

/*
 * Rebuild the interpreter frame then punt to the interpreter to execute
 * instruction at specified PC.
 *
 * Currently parameters are passed to the current frame, so we just need to
 * grow the stack save area above it, fill certain fields in StackSaveArea and
 * Thread that are skipped during whole-method invocation (specified below),
 * then return to the interpreter.
 *
 * StackSaveArea:
 *  - prevSave
 *  - prevFrame
 *  - savedPc
 *  - returnAddr
 *  - method
 *
 * Thread:
 *  - method
 *  - methodClassDex
 *  - curFrame
 */
static void genMethodInflateAndPunt(CompilationUnit *cUnit, MIR *mir,
                                    BasicBlock *bb)
{
    int oldStackSave = r0;
    int newStackSave = r1;
    int oldFP = r2;
    int savedPC = r3;
    int currentPC = r4PC;
    int returnAddr = r7;
    int method = r8;
    int pDvmDex = r9;

    /*
     * TODO: check whether to raise the stack overflow exception when growing
     * the stack save area.
     */

    /* Send everything to home location */
    dvmCompilerFlushAllRegs(cUnit);

    /* oldStackSave = r5FP + sizeof(current frame) */
    opRegRegImm(cUnit, kOpAdd, oldStackSave, r5FP,
                cUnit->method->registersSize * 4);
    /* oldFP = oldStackSave + sizeof(stackSaveArea) */
    opRegRegImm(cUnit, kOpAdd, oldFP, oldStackSave, sizeof(StackSaveArea));
    /* newStackSave = r5FP - sizeof(StackSaveArea) */
    opRegRegImm(cUnit, kOpSub, newStackSave, r5FP, sizeof(StackSaveArea));

    loadWordDisp(cUnit, r13sp, 0, savedPC);
    loadConstant(cUnit, currentPC, (int) (cUnit->method->insns + mir->offset));
    loadConstant(cUnit, method, (int) cUnit->method);
    loadConstant(cUnit, pDvmDex, (int) cUnit->method->clazz->pDvmDex);
#ifdef EASY_GDB
    /* newStackSave->prevSave = oldStackSave */
    storeWordDisp(cUnit, newStackSave, offsetof(StackSaveArea, prevSave),
                  oldStackSave);
#endif
    /* newStackSave->prevSave = oldStackSave */
    storeWordDisp(cUnit, newStackSave, offsetof(StackSaveArea, prevFrame),
                  oldFP);
    /* newStackSave->savedPc = savedPC */
    storeWordDisp(cUnit, newStackSave, offsetof(StackSaveArea, savedPc),
                  savedPC);
    /* return address */
    loadConstant(cUnit, returnAddr, 0);
    storeWordDisp(cUnit, newStackSave, offsetof(StackSaveArea, returnAddr),
                  returnAddr);
    /* newStackSave->method = method */
    storeWordDisp(cUnit, newStackSave, offsetof(StackSaveArea, method), method);
    /* thread->method = method */
    storeWordDisp(cUnit, r6SELF, offsetof(InterpSaveState, method), method);
    /* thread->interpSave.curFrame = current FP */
    storeWordDisp(cUnit, r6SELF, offsetof(Thread, interpSave.curFrame), r5FP);
    /* thread->methodClassDex = pDvmDex */
    storeWordDisp(cUnit, r6SELF, offsetof(InterpSaveState, methodClassDex),
                  pDvmDex);
    /* Restore the stack pointer */
    opRegImm(cUnit, kOpAdd, r13sp, 16);
    genPuntToInterp(cUnit, mir->offset);
}

/*
 * The following are the first-level codegen routines that analyze the format
 * of each bytecode then either dispatch special purpose codegen routines
 * or produce corresponding Thumb instructions directly.
 *
 * TODO - most them are just pass-through to the trace-based versions for now
 */
static bool handleMethodFmt10t_Fmt20t_Fmt30t(CompilationUnit *cUnit, MIR *mir,
                                             BasicBlock *bb, ArmLIR *labelList)
{
    /* backward branch? */
    bool backwardBranch = (bb->taken->startOffset <= mir->offset);

    if (backwardBranch && gDvmJit.genSuspendPoll) {
        genSuspendPoll(cUnit, mir);
    }

    /* For OP_GOTO, OP_GOTO_16, and OP_GOTO_32 */
    genUnconditionalBranch(cUnit, &labelList[bb->taken->id]);
    return false;
}

static bool handleMethodFmt10x(CompilationUnit *cUnit, MIR *mir)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    switch (dalvikOpcode) {
        case OP_RETURN_VOID:
            return false;
        default:
            return handleFmt10x(cUnit, mir);
    }
}

static bool handleMethodFmt11n_Fmt31i(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt11n_Fmt31i(cUnit, mir);
}

static bool handleMethodFmt11x(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                               ArmLIR *labelList)
{
    Opcode dalvikOpcode = mir->dalvikInsn.opcode;
    switch (dalvikOpcode) {
        case OP_THROW:
            genMethodInflateAndPunt(cUnit, mir, bb);
            return false;
        default:
            return handleFmt11x(cUnit, mir);
    }
}

static bool handleMethodFmt12x(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt12x(cUnit, mir);
}

static bool handleMethodFmt20bc(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt20bc(cUnit, mir);
}

static bool handleMethodFmt21c_Fmt31c(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt21c_Fmt31c(cUnit, mir);
}

static bool handleMethodFmt21h(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt21h(cUnit, mir);
}

static bool handleMethodFmt21s(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt21s(cUnit, mir);
}

static bool handleMethodFmt21t(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                               ArmLIR *labelList)
{
    return handleFmt21t(cUnit, mir, bb, labelList);
}

static bool handleMethodFmt22b_Fmt22s(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt22b_Fmt22s(cUnit, mir);
}

static bool handleMethodFmt22c(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt22c(cUnit, mir);
}

static bool handleMethodFmt22cs(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt22cs(cUnit, mir);
}

static bool handleMethodFmt22t(CompilationUnit *cUnit, MIR *mir, BasicBlock *bb,
                               ArmLIR *labelList)
{
    return handleFmt22t(cUnit, mir, bb, labelList);
}

static bool handleMethodFmt22x_Fmt32x(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt22x_Fmt32x(cUnit, mir);
}

static bool handleMethodFmt23x(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt23x(cUnit, mir);
}

static bool handleMethodFmt31t(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt31t(cUnit, mir);
}

static bool handleMethodFmt35c_3rc(CompilationUnit *cUnit, MIR *mir,
                                       BasicBlock *bb, ArmLIR *labelList)
{
    return handleFmt35c_3rc(cUnit, mir, bb, labelList);
}

static bool handleMethodFmt35ms_3rms(CompilationUnit *cUnit, MIR *mir,
                                     BasicBlock *bb, ArmLIR *labelList)
{
    return handleFmt35ms_3rms(cUnit, mir, bb, labelList);
}

static bool handleMethodExecuteInline(CompilationUnit *cUnit, MIR *mir)
{
    return handleExecuteInline(cUnit, mir);
}

static bool handleMethodFmt51l(CompilationUnit *cUnit, MIR *mir)
{
    return handleFmt51l(cUnit, mir);
}

/* Handle the content in each basic block */
static bool methodBlockCodeGen(CompilationUnit *cUnit, BasicBlock *bb)
{
    MIR *mir;
    ArmLIR *labelList = (ArmLIR *) cUnit->blockLabelList;
    int blockId = bb->id;

    cUnit->curBlock = bb;
    labelList[blockId].operands[0] = bb->startOffset;

    /* Insert the block label */
    labelList[blockId].opcode = kArmPseudoNormalBlockLabel;
    dvmCompilerAppendLIR(cUnit, (LIR *) &labelList[blockId]);

    dvmCompilerClobberAllRegs(cUnit);
    dvmCompilerResetNullCheck(cUnit);

    ArmLIR *headLIR = NULL;

    if (bb->blockType == kEntryBlock) {
        /* r0 = callsitePC */
        opImm(cUnit, kOpPush, (1 << r0 | 1 << r1 | 1 << r5FP | 1 << r14lr));
        opRegImm(cUnit, kOpSub, r5FP,
                 sizeof(StackSaveArea) + cUnit->method->registersSize * 4);

    } else if (bb->blockType == kExitBlock) {
        /* No need to pop r0 and r1 */
        opRegImm(cUnit, kOpAdd, r13sp, 8);
        opImm(cUnit, kOpPop, (1 << r5FP | 1 << r15pc));
    }

    for (mir = bb->firstMIRInsn; mir; mir = mir->next) {

        dvmCompilerResetRegPool(cUnit);
        if (gDvmJit.disableOpt & (1 << kTrackLiveTemps)) {
            dvmCompilerClobberAllRegs(cUnit);
        }

        if (gDvmJit.disableOpt & (1 << kSuppressLoads)) {
            dvmCompilerResetDefTracking(cUnit);
        }

        Opcode dalvikOpcode = mir->dalvikInsn.opcode;
        InstructionFormat dalvikFormat =
            dexGetFormatFromOpcode(dalvikOpcode);

        ArmLIR *boundaryLIR;

        /*
         * Don't generate the boundary LIR unless we are debugging this
         * trace or we need a scheduling barrier.
         */
        if (headLIR == NULL || cUnit->printMe == true) {
            boundaryLIR =
                newLIR2(cUnit, kArmPseudoDalvikByteCodeBoundary,
                        mir->offset,
                        (int) dvmCompilerGetDalvikDisassembly(
                            &mir->dalvikInsn, ""));
            /* Remember the first LIR for this block */
            if (headLIR == NULL) {
                headLIR = boundaryLIR;
                /* Set the first boundaryLIR as a scheduling barrier */
                headLIR->defMask = ENCODE_ALL;
            }
        }

        /* Don't generate the SSA annotation unless verbose mode is on */
        if (cUnit->printMe && mir->ssaRep) {
            char *ssaString = dvmCompilerGetSSAString(cUnit, mir->ssaRep);
            newLIR1(cUnit, kArmPseudoSSARep, (int) ssaString);
        }

        bool notHandled;
        switch (dalvikFormat) {
            case kFmt10t:
            case kFmt20t:
            case kFmt30t:
                notHandled = handleMethodFmt10t_Fmt20t_Fmt30t(cUnit, mir, bb,
                                                              labelList);
                break;
            case kFmt10x:
                notHandled = handleMethodFmt10x(cUnit, mir);
                break;
            case kFmt11n:
            case kFmt31i:
                notHandled = handleMethodFmt11n_Fmt31i(cUnit, mir);
                break;
            case kFmt11x:
                notHandled = handleMethodFmt11x(cUnit, mir, bb, labelList);
                break;
            case kFmt12x:
                notHandled = handleMethodFmt12x(cUnit, mir);
                break;
            case kFmt20bc:
                notHandled = handleMethodFmt20bc(cUnit, mir);
                break;
            case kFmt21c:
            case kFmt31c:
                notHandled = handleMethodFmt21c_Fmt31c(cUnit, mir);
                break;
            case kFmt21h:
                notHandled = handleMethodFmt21h(cUnit, mir);
                break;
            case kFmt21s:
                notHandled = handleMethodFmt21s(cUnit, mir);
                break;
            case kFmt21t:
                notHandled = handleMethodFmt21t(cUnit, mir, bb, labelList);
                break;
            case kFmt22b:
            case kFmt22s:
                notHandled = handleMethodFmt22b_Fmt22s(cUnit, mir);
                break;
            case kFmt22c:
                notHandled = handleMethodFmt22c(cUnit, mir);
                break;
            case kFmt22cs:
                notHandled = handleMethodFmt22cs(cUnit, mir);
                break;
            case kFmt22t:
                notHandled = handleMethodFmt22t(cUnit, mir, bb, labelList);
                break;
            case kFmt22x:
            case kFmt32x:
                notHandled = handleMethodFmt22x_Fmt32x(cUnit, mir);
                break;
            case kFmt23x:
                notHandled = handleMethodFmt23x(cUnit, mir);
                break;
            case kFmt31t:
                notHandled = handleMethodFmt31t(cUnit, mir);
                break;
            case kFmt3rc:
            case kFmt35c:
                notHandled = handleMethodFmt35c_3rc(cUnit, mir, bb, labelList);
                break;
            case kFmt3rms:
            case kFmt35ms:
                notHandled = handleMethodFmt35ms_3rms(cUnit, mir, bb,
                                                      labelList);
                break;
            case kFmt35mi:
            case kFmt3rmi:
                notHandled = handleMethodExecuteInline(cUnit, mir);
                break;
            case kFmt51l:
                notHandled = handleMethodFmt51l(cUnit, mir);
                break;
            default:
                notHandled = true;
                break;
        }

        /* FIXME - to be implemented */
        if (notHandled == true && dalvikOpcode >= kNumPackedOpcodes) {
            notHandled = false;
        }

        if (notHandled) {
            ALOGE("%#06x: Opcode %#x (%s) / Fmt %d not handled",
                 mir->offset,
                 dalvikOpcode, dexGetOpcodeName(dalvikOpcode),
                 dalvikFormat);
            dvmCompilerAbort(cUnit);
            break;
        }
    }

    if (headLIR) {
        /*
         * Eliminate redundant loads/stores and delay stores into later
         * slots
         */
        dvmCompilerApplyLocalOptimizations(cUnit, (LIR *) headLIR,
                                           cUnit->lastLIRInsn);

        /*
         * Generate an unconditional branch to the fallthrough block.
         */
        if (bb->fallThrough) {
            genUnconditionalBranch(cUnit,
                                   &labelList[bb->fallThrough->id]);
        }
    }
    return false;
}

void dvmCompilerMethodMIR2LIR(CompilationUnit *cUnit)
{
    // FIXME - enable method compilation for selected routines here
    if (strcmp(cUnit->method->name, "add")) return;

    /* Used to hold the labels of each block */
    cUnit->blockLabelList =
        (void *) dvmCompilerNew(sizeof(ArmLIR) * cUnit->numBlocks, true);

    dvmCompilerDataFlowAnalysisDispatcher(cUnit, methodBlockCodeGen,
                                          kPreOrderDFSTraversal,
                                          false /* isIterative */);

    dvmCompilerApplyGlobalOptimizations(cUnit);

    // FIXME - temporarily enable verbose printing for all methods
    cUnit->printMe = true;

#if defined(WITH_SELF_VERIFICATION)
    selfVerificationBranchInsertPass(cUnit);
#endif
}

#else

void dvmCompilerMethodMIR2LIR(CompilationUnit *cUnit) {
    // Method-based JIT not supported for ARM.
}

#endif
