/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align 2

#if defined(WITH_JIT)
#if defined(WITH_SELF_VERIFICATION)

/*
 * "longjmp" to a translation after single-stepping.  Before returning
 * to translation, must save state for self-verification.
 */
    .global dvmJitResumeTranslation             # (Thread* self, u4* dFP)
dvmJitResumeTranslation:
    move    rSELF, a0                           # restore self
    move    rPC, a1                             # restore Dalvik pc
    move    rFP, a2                             # restore Dalvik fp
    lw      rBIX, offThread_jitResumeNPC(rSELF)
    sw      zero, offThread_jitResumeNPC(rSELF) # reset resume address
    lw      sp, offThread_jitResumeNSP(rSELF)   # cut back native stack
    b       jitSVShadowRunStart                 # resume as if cache hit
                                                # expects resume addr in rBIX

    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    li        a2, kSVSPunt                 #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    move      rPC, a0                      # set up dalvik pc
    EXPORT_PC()
    sw        ra, offThread_jitResumeNPC(rSELF)
    sw        a1, offThread_jitResumeDPC(rSELF)
    li        a2, kSVSSingleStep           #  a2 <- interpreter entry point
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpNoChainNoProfile
dvmJitToInterpNoChainNoProfile:
    move      a0, rPC                      #  pass our target PC
    li        a2, kSVSNoProfile            #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
    move      a0, rPC                      #  pass our target PC
    li        a2, kSVSTraceSelect          #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    lw        a0, 0(ra)                   #  pass our target PC
    li        a2, kSVSTraceSelect          #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpBackwardBranch
dvmJitToInterpBackwardBranch:
    lw        a0, 0(ra)                   #  pass our target PC
    li        a2, kSVSBackwardBranch       #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
    lw        a0, 0(ra)                   #  pass our target PC
    li        a2, kSVSNormal               #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
    move      a0, rPC                      #  pass our target PC
    li        a2, kSVSNoChain              #  a2 <- interpreter entry point
    sw        zero, offThread_inJitCodeCache(rSELF) #  Back to the interp land
    b         jitSVShadowRunEnd            #  doesn't return
#else                                   /*  WITH_SELF_VERIFICATION */


/*
 * "longjmp" to a translation after single-stepping.
 */
    .global dvmJitResumeTranslation             # (Thread* self, u4* dFP)
dvmJitResumeTranslation:
    move    rSELF, a0                           # restore self
    move    rPC, a1                             # restore Dalvik pc
    move    rFP, a2                             # restore Dalvik fp
    lw      a0, offThread_jitResumeNPC(rSELF)
    sw      zero, offThread_jitResumeNPC(rSELF) # reset resume address
    lw      sp, offThread_jitResumeNSP(rSELF)   # cut back native stack
    jr      a0                                  # resume translation


/*
 * Return from the translation cache to the interpreter when the compiler is
 * having issues translating/executing a Dalvik instruction. We have to skip
 * the code cache lookup otherwise it is possible to indefinitely bouce
 * between the interpreter and the code cache if the instruction that fails
 * to be compiled happens to be at a trace start.
 */
    .global dvmJitToInterpPunt
dvmJitToInterpPunt:
    lw        gp, STACK_OFFSET_GP(sp)
    move      rPC, a0
#if defined(WITH_JIT_TUNING)
    move      a0, ra
    JAL(dvmBumpPunt)
#endif
    EXPORT_PC()
    sw        zero, offThread_inJitCodeCache(rSELF) # Back to the interp land
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

/*
 * Return to the interpreter to handle a single instruction.
 * On entry:
 *    rPC <= Dalvik PC of instrucion to interpret
 *    a1 <= Dalvik PC of resume instruction
 *    ra <= resume point in translation
 */

    .global dvmJitToInterpSingleStep
dvmJitToInterpSingleStep:
    lw        gp, STACK_OFFSET_GP(sp)
    move      rPC, a0                       # set up dalvik pc
    EXPORT_PC()
    sw        ra, offThread_jitResumeNPC(rSELF)
    sw        sp, offThread_jitResumeNSP(rSELF)
    sw        a1, offThread_jitResumeDPC(rSELF)
    li        a1, 1
    sw        a1, offThread_singleStepCount(rSELF) # just step once
    move      a0, rSELF
    li        a1, kSubModeCountedStep
    JAL(dvmEnableSubMode)                   # (self, subMode)
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used for callees.
 */
    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
    lw        gp, STACK_OFFSET_GP(sp)
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNoChain)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) # set the inJitCodeCache flag
    move      a1, rPC                      # arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    beqz      a0, 2f                       # 0 means translation does not exist
    jr        a0

/*
 * Return from the translation cache and immediately request
 * a translation for the exit target.  Commonly used following
 * invokes.
 */
    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    lw        gp, STACK_OFFSET_GP(sp)
    lw        rPC, (ra)                    #  get our target PC
    subu      rINST, ra, 8                 #  save start of chain branch
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # @ (pc, self)
    sw        v0, offThread_inJitCodeCache(rSELF) # set the inJitCodeCache flag
    beqz      v0, 2f
    move      a0, v0
    move      a1, rINST
    JAL(dvmJitChain)                       #  v0 <- dvmJitChain(codeAddr, chainAddr)
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    move      a0, v0
    beqz      a0, toInterpreter            #  didn't chain - resume with interpreter

    jr        a0                           #  continue native execution

/* No translation, so request one if profiling isn't disabled */
2:
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    lw        a0, offThread_pJitProfTable(rSELF)
    FETCH_INST()
    li        t0, kJitTSelectRequestHot
    movn      a2, t0, a0                   #  ask for trace selection
    bnez      a0, common_selectTrace
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)

/*
 * Return from the translation cache to the interpreter.
 * The return was done with a BLX from thumb mode, and
 * the following 32-bit word contains the target rPC value.
 * Note that lr (r14) will have its low-order bit set to denote
 * its thumb-mode origin.
 *
 * We'll need to stash our lr origin away, recover the new
 * target and then check to see if there is a translation available
 * for our new target.  If so, we do a translation chain and
 * go back to native execution.  Otherwise, it's back to the
 * interpreter (after treating this entry as a potential
 * trace start).
 */
    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
    lw        gp, STACK_OFFSET_GP(sp)
    lw        rPC, (ra)                    #  get our target PC
    subu      rINST, ra, 8                 #  save start of chain branch
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNormal)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)           # @ (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    beqz      a0, toInterpreter            #  go if not, otherwise do chain
    move      a1, rINST
    JAL(dvmJitChain)                       #  v0 <- dvmJitChain(codeAddr, chainAddr)
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    move      a0, v0
    beqz      a0, toInterpreter            #  didn't chain - resume with interpreter

    jr        a0                           #  continue native execution

/*
 * Return from the translation cache to the interpreter to do method invocation.
 * Check if translation exists for the callee, but don't chain to it.
 */
    .global dvmJitToInterpNoChainNoProfile
dvmJitToInterpNoChainNoProfile:
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNoChain)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    beqz      a0, footer235

    jr        a0                           #  continue native execution if so
footer235:
    EXPORT_PC()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

/*
 * Return from the translation cache to the interpreter to do method invocation.
 * Check if translation exists for the callee, but don't chain to it.
 */

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
    lw        gp, STACK_OFFSET_GP(sp)
#if defined(WITH_JIT_TUNING)
    JAL(dvmBumpNoChain)
#endif
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        a0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET
    beqz      a0, 1f
    jr        a0                           #  continue native execution if so
1:
#endif                                  /*  WITH_SELF_VERIFICATION */

/*
 * No translation, restore interpreter regs and start interpreting.
 * rSELF & rFP were preserved in the translated code, and rPC has
 * already been restored by the time we get here.  We'll need to set
 * up rIBASE & rINST, and load the address of the JitTable into r0.
 */

toInterpreter:
    EXPORT_PC()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    lw        a0, offThread_pJitProfTable(rSELF)
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    # NOTE: intended fallthrough

/*
 * Similar to common_updateProfile, but tests for null pJitProfTable
 * r0 holds pJifProfTAble, rINST is loaded, rPC is current and
 * rIBASE has been recently refreshed.
 */

common_testUpdateProfile:

    beqz      a0, 4f

/*
 * Common code to update potential trace start counter, and initiate
 * a trace-build if appropriate.
 * On entry here:
 *    r0    <= pJitProfTable (verified non-NULL)
 *    rPC   <= Dalvik PC
 *    rINST <= next instruction
 */
common_updateProfile:
    srl       a3, rPC, 12                  #  cheap, but fast hash function
    xor       a3, a3, rPC
    andi      a3, a3, JIT_PROF_SIZE-1      #  eliminate excess bits
    addu      t1, a0, a3
    lbu       a1, (t1)                     #  get counter
    GET_INST_OPCODE(t0)
    subu      a1, a1, 1                    #  decrement counter
    sb        a1, (t1)                     #  and store it
    beqz      a1, 1f
    GOTO_OPCODE(t0)                        #  if not threshold, fallthrough otherwise
1:
    /* Looks good, reset the counter */
    lw        a1, offThread_jitThreshold(rSELF)
    sb        a1, (t1)
    EXPORT_PC()
    move      a0, rPC
    move      a1, rSELF
    JAL(dvmJitGetTraceAddrThread)          # (pc, self)
    move      a0, v0
    sw        v0, offThread_inJitCodeCache(rSELF) #  set the inJitCodeCache flag
    move      a1, rPC                      #  arg1 of translation may need this
    move      ra, zero                     #  in case target is HANDLER_INTERPRET

#if !defined(WITH_SELF_VERIFICATION)
    li        t0, kJitTSelectRequest       #  ask for trace selection
    movz      a2, t0, a0
    beqz      a0, common_selectTrace
    jr        a0                           #  jump to the translation
#else

    bne       a0, zero, skip_ask_for_trace_selection
    li        a2, kJitTSelectRequest       #  ask for trace selection
    j         common_selectTrace

skip_ask_for_trace_selection:
    /*
     * At this point, we have a target translation.  However, if
     * that translation is actually the interpret-only pseudo-translation
     * we want to treat it the same as no translation.
     */
    move      rBIX, a0                     #  save target
    jal       dvmCompilerGetInterpretTemplate
    # special case?
    bne       v0, rBIX, jitSVShadowRunStart  #  set up self verification shadow space
    # Need to clear the inJitCodeCache flag
    sw        zero, offThread_inJitCodeCache(rSELF) #  back to the interp land
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
    /* no return */
#endif

/*
 * On entry:
 *  r2 is jit state.
 */

common_selectTrace:
    lhu        a0, offThread_subMode(rSELF)
    andi       a0, (kSubModeJitTraceBuild | kSubModeJitSV)
    bnez       a0, 3f                      # already doing JIT work, continue
    sw         a2, offThread_jitState(rSELF)
    move       a0, rSELF

/*
 * Call out to validate trace-building request.  If successful,
 * rIBASE will be swapped to to send us into single-stepping trace
 * building mode, so we need to refresh before we continue.
 */

    EXPORT_PC()
    SAVE_PC_TO_SELF()
    SAVE_FP_TO_SELF()
    JAL(dvmJitCheckTraceRequest)
3:
    FETCH_INST()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
4:
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)
    /* no return */
#endif

#if defined(WITH_SELF_VERIFICATION)

/*
 * Save PC and registers to shadow memory for self verification mode
 * before jumping to native translation.
 * On entry:
 *    rPC, rFP, rSELF: the values that they should contain
 *    r10: the address of the target translation.
 */
jitSVShadowRunStart:
    move      a0, rPC                      #  r0 <- program counter
    move      a1, rFP                      #  r1 <- frame pointer
    move      a2, rSELF                    #  r2 <- InterpState pointer
    move      a3, rBIX                     #  r3 <- target translation
    jal       dvmSelfVerificationSaveState #  save registers to shadow space
    lw        rFP, offShadowSpace_shadowFP(v0) #  rFP <- fp in shadow space
    jr        rBIX                         #  jump to the translation

/*
 * Restore PC, registers, and interpState to original values
 * before jumping back to the interpreter.
 */
jitSVShadowRunEnd:
    move      a1, rFP                      #  pass ending fp
    move      a3, rSELF                    #  pass self ptr for convenience
    jal       dvmSelfVerificationRestoreState #  restore pc and fp values
    LOAD_PC_FP_FROM_SELF()                 #  restore pc, fp
    lw        a1, offShadowSpace_svState(a0) #  get self verification state
    beq       a1, zero, 1f                 #  check for punt condition

    # Setup SV single-stepping
    move      a0, rSELF
    li        a1, kSubModeJitSV
    JAL(dvmEnableSubMode)                  # (self, subMode)
    li        a2, kJitSelfVerification     #  ask for self verification
    sw        a2, offThread_jitState(rSELF)
    # Intentional fallthrough

1:
    # exit to interpreter without check
    EXPORT_PC()
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
#endif

/*
 * The equivalent of "goto bail", this calls through the "bail handler".
 * It will end this interpreter activation, and return to the caller
 * of dvmMterpStdRun.
 *
 * State registers will be saved to the "thread" area before bailing
 * debugging purposes
 */
    .ent common_gotoBail
common_gotoBail:
    SAVE_PC_FP_TO_SELF()                   # export state to "thread"
    move      a0, rSELF                    # a0 <- self ptr
    b         dvmMterpStdBail              # call(self, changeInterp)
    .end common_gotoBail

/*
 * The JIT's invoke method needs to remember the callsite class and
 * target pair.  Save them here so that they are available to
 * dvmCheckJit following the interpretation of this invoke.
 */
#if defined(WITH_JIT)
save_callsiteinfo:
    beqz    rOBJ, 1f
    lw      rOBJ, offObject_clazz(rOBJ)
1:
    sw      a0, offThread_methodToCall(rSELF)
    sw      rOBJ, offThread_callsiteClass(rSELF)
    jr      ra
#endif

/*
 * Common code for method invocation with range.
 *
 * On entry:
 *  a0 is "Method* methodToCall", the method we're trying to call
 */
common_invokeMethodRange:
.LinvokeNewRange:
#if defined(WITH_JIT)
    lhu      a1, offThread_subMode(rSELF)
    andi     a1, kSubModeJitTraceBuild
    beqz     a1, 1f
    JAL(save_callsiteinfo)
#endif
    # prepare to copy args to "outs" area of current frame
1:
    GET_OPA(a2)
    SAVEAREA_FROM_FP(rBIX, rFP)              #  rBIX <- stack save area
    beqz      a2, .LinvokeArgsDone
    FETCH(a1, 2)                           #  a1 <- CCCC
.LinvokeRangeArgs:
    # a0=methodToCall, a1=CCCC, a2=count, rBIX=outs
    # (very few methods have > 10 args; could unroll for common cases)
    EAS2(a3, rFP, a1)
    sll       t0, a2, 2
    subu      rBIX, rBIX, t0

1:
    lw        a1, 0(a3)
    addu      a3, a3, 4
    subu      a2, a2, 1
    sw        a1, 0(rBIX)
    addu      rBIX, 4
    bnez      a2, 1b
    b         .LinvokeArgsDone

/*
 * Common code for method invocation without range.
 *
 * On entry:
 *  a0 is "Method* methodToCall", "rOBJ is this"
 */
common_invokeMethodNoRange:
.LinvokeNewNoRange:
#if defined(WITH_JIT)
    lhu      a1, offThread_subMode(rSELF)
    andi     a1, kSubModeJitTraceBuild
    beqz     a1, 1f
    JAL(save_callsiteinfo)
#endif

    # prepare to copy args to "outs" area of current frame
1:
    GET_OPB(a2)
    SAVEAREA_FROM_FP(rBIX, rFP)
    beqz      a2, .LinvokeArgsDone
    FETCH(a1, 2)

    # a0=methodToCall, a1=GFED, a2=count,
.LinvokeNonRange:
    beq       a2, 0, 0f
    beq       a2, 1, 1f
    beq       a2, 2, 2f
    beq       a2, 3, 3f
    beq       a2, 4, 4f
    beq       a2, 5, 5f

5:
    and       t0, rINST, 0x0f00
    ESRN(t2, rFP, t0, 6)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

4:
    and       t0, a1, 0xf000
    ESRN(t2, rFP, t0, 10)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

3:
    and       t0, a1, 0x0f00
    ESRN(t2, rFP, t0, 6)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

2:
    and       t0, a1, 0x00f0
    ESRN(t2, rFP, t0, 2)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

1:
    and       t0, a1, 0x000f
    EASN(t2, rFP, t0, 2)
    lw        a3, (t2)
    subu      rBIX, 4
    sw        a3, 0(rBIX)

0:
    #fall through .LinvokeArgsDone


.LinvokeArgsDone:                          #  a0=methodToCall
    lhu       rOBJ, offMethod_registersSize(a0)
    lhu       a3, offMethod_outsSize(a0)
    lw        a2, offMethod_insns(a0)
    lw        rINST, offMethod_clazz(a0)
    # find space for the new stack frame, check for overflow
    SAVEAREA_FROM_FP(a1, rFP)              # a1 <- stack save area
    sll       t0, rOBJ, 2                    #  a1 <- newFp (old savearea - regsSize)
    subu      a1, a1, t0
    SAVEAREA_FROM_FP(rBIX, a1)
    lw        rOBJ, offThread_interpStackEnd(rSELF) #  t3 <- interpStackEnd
    sll       t2, a3, 2
    subu      t0, rBIX, t2
    lhu       ra, offThread_subMode(rSELF)
    lw        a3, offMethod_accessFlags(a0) #  a3 <- methodToCall->accessFlags
    bltu      t0, rOBJ, .LstackOverflow      #  yes, this frame will overflow stack


    # set up newSaveArea
#ifdef EASY_GDB
    SAVEAREA_FROM_FP(t0, rFP)
    sw        t0, offStackSaveArea_prevSave(rBIX)
#endif
    sw        rFP, (offStackSaveArea_prevFrame)(rBIX)
    sw        rPC, (offStackSaveArea_savedPc)(rBIX)
#if defined(WITH_JIT)
    sw        zero, (offStackSaveArea_returnAddr)(rBIX)
#endif
    sw        a0, (offStackSaveArea_method)(rBIX)
    # Profiling?
    bnez       ra, 2f
1:
    and       t2, a3, ACC_NATIVE
    bnez      t2, .LinvokeNative
    lhu       rOBJ, (a2)           # rOBJ -< load Inst from New PC
    lw        a3, offClassObject_pDvmDex(rINST)
    move      rPC, a2              # Publish new rPC
    # Update state values for the new method
    # a0=methodToCall, a1=newFp, a3=newMethodClass, rOBJ=newINST
    sw        a0, offThread_method(rSELF)
    sw        a3, offThread_methodClassDex(rSELF)
    li        a2, 1
    sw        a2, offThread_debugIsMethodEntry(rSELF)

#if defined(WITH_JIT)
    lw        a0, offThread_pJitProfTable(rSELF)
    move      rFP, a1                    # fp = newFp
    GET_PREFETCHED_OPCODE(t0, rOBJ)      # extract prefetched opcode from rOBJ
    move      rINST, rOBJ                # publish new rINST
    sw        a1, offThread_curFrame(rSELF)
    bnez      a0, common_updateProfile
    GOTO_OPCODE(t0)
#else
    move      rFP, a1
    GET_PREFETCHED_OPCODE(t0, rOBJ)
    move      rINST, rOBJ
    sw        a1, offThread_curFrame(rSELF)
    GOTO_OPCODE(t0)
#endif

2:
    # Profiling - record method entry.  a0: methodToCall
    STACK_STORE(a0, 0)
    STACK_STORE(a1, 4)
    STACK_STORE(a2, 8)
    STACK_STORE(a3, 12)
    sw       rPC, offThread_pc(rSELF)          # update interpSave.pc
    move     a1, a0
    move     a0, rSELF
    JAL(dvmReportInvoke)
    STACK_LOAD(a3, 12)                         # restore a0-a3
    STACK_LOAD(a2, 8)
    STACK_LOAD(a1, 4)
    STACK_LOAD(a0, 0)
    b        1b
.LinvokeNative:
    # Prep for the native call
    # a0=methodToCall, a1=newFp, rBIX=newSaveArea
    lhu       ra, offThread_subMode(rSELF)
    lw        t3, offThread_jniLocal_topCookie(rSELF)
    sw        a1, offThread_curFrame(rSELF)
    sw        t3, offStackSaveArea_localRefCookie(rBIX) # newFp->localRefCookie=top
    move      a2, a0
    move      a0, a1
    addu      a1, rSELF, offThread_retval
    move      a3, rSELF
#ifdef ASSIST_DEBUGGER
    /* insert fake function header to help gdb find the stack frame */
    b         .Lskip
    .ent dalvik_mterp
dalvik_mterp:
    STACK_STORE_FULL()
.Lskip:
#endif
    bnez      ra, 11f                          # Any special SubModes active?
    lw        t9, offMethod_nativeFunc(a2)
    jalr      t9
    lw        gp, STACK_OFFSET_GP(sp)
7:
    # native return; rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw        a0, offStackSaveArea_localRefCookie(rBIX)
    lw        a1, offThread_exception(rSELF)
    sw        rFP, offThread_curFrame(rSELF)
    sw        a0, offThread_jniLocal_topCookie(rSELF)    # new top <- old top
    bnez      a1, common_exceptionThrown

    FETCH_ADVANCE_INST(3)
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
11:
    # a0=newFp, a1=&retval, a2=methodToCall, a3=self, ra=subModes
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)
    move      a0, a2                    # a0 <- methodToCall
    move      a1, rSELF
    move      a2, rFP
    JAL(dvmReportPreNativeInvoke)       # (methodToCall, self, fp)
    SCRATCH_LOAD(a3, 12)                         # restore a0-a3
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    # Call the native method
    lw       t9, offMethod_nativeFunc(a2)      # t9<-methodToCall->nativeFunc
    jalr     t9
    lw       gp, STACK_OFFSET_GP(sp)

    # Restore the pre-call arguments
    SCRATCH_LOAD(a3, 12)                         # restore a0-a3
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    # Finish up any post-invoke subMode requirements
    move      a0, a2
    move      a1, rSELF
    move      a2, rFP
    JAL(dvmReportPostNativeInvoke)      # (methodToCall, self, fp)
    b         7b


.LstackOverflow:       # a0=methodToCall
    move      a1, a0                    #  a1 <- methodToCall
    move      a0, rSELF                 # a0 <- self
    JAL(dvmHandleStackOverflow)         #  dvmHandleStackOverflow(self, methodToCall)
    b         common_exceptionThrown
#ifdef ASSIST_DEBUGGER
    .end dalvik_mterp
#endif

    /*
     * Common code for method invocation, calling through "glue code".
     *
     * TODO: now that we have range and non-range invoke handlers, this
     *       needs to be split into two.  Maybe just create entry points
     *       that set r9 and jump here?
     *
     * On entry:
     *  r0 is "Method* methodToCall", the method we're trying to call
     *  r9 is "bool methodCallRange", indicating if this is a /range variant
     */

/*
 * Common code for handling a return instruction.
 *
 * This does not return.
 */
common_returnFromMethod:
.LreturnNew:
    lhu       t0, offThread_subMode(rSELF)
    SAVEAREA_FROM_FP(a0, rFP)
    lw        rOBJ, offStackSaveArea_savedPc(a0) # rOBJ = saveArea->savedPc
    bnez      t0, 19f
14:
    lw        rFP, offStackSaveArea_prevFrame(a0) # fp = saveArea->prevFrame
    lw        a2, (offStackSaveArea_method - sizeofStackSaveArea)(rFP)
                                               # a2<- method we're returning to
    # is this a break frame?
    beqz      a2, common_gotoBail              # break frame, bail out completely

    lw        rBIX, offMethod_clazz(a2)        # rBIX<- method->clazz
    lw        rIBASE, offThread_curHandlerTable(rSELF) # refresh rIBASE
    PREFETCH_ADVANCE_INST(rINST, rOBJ, 3)      # advance rOBJ, update new rINST
    sw        a2, offThread_method(rSELF)      # self->method = newSave->method
    lw        a1, offClassObject_pDvmDex(rBIX) # r1<- method->clazz->pDvmDex
    sw        rFP, offThread_curFrame(rSELF)   # curFrame = fp
#if defined(WITH_JIT)
    lw         rBIX, offStackSaveArea_returnAddr(a0)
    move       rPC, rOBJ                       # publish new rPC
    sw         a1, offThread_methodClassDex(rSELF)
    sw         rBIX, offThread_inJitCodeCache(rSELF) # may return to JIT'ed land
    beqz       rBIX, 15f                       # caller is compiled code
    move       t9, rBIX
    jalr       t9
    lw         gp, STACK_OFFSET_GP(sp)
15:
    GET_INST_OPCODE(t0)                        # extract opcode from rINST
    GOTO_OPCODE(t0)                            # jump to next instruction
#else
    GET_INST_OPCODE(t0)                        # extract opcode from rINST
    move       rPC, rOBJ                       # publish new rPC
    sw         a1, offThread_methodClassDex(rSELF)
    GOTO_OPCODE(t0)
#endif

19:
    # Handle special actions
    # On entry, a0: StackSaveArea
    lw         a1, offStackSaveArea_prevFrame(a0) # a1<- prevFP
    sw         rPC, offThread_pc(rSELF)        # update interpSave.pc
    sw         a1, offThread_curFrame(rSELF)   # update interpSave.curFrame
    move       a0, rSELF
    JAL(dvmReportReturn)
    SAVEAREA_FROM_FP(a0, rFP)                  # restore StackSaveArea
    b          14b

    .if 0
    /*
     * Return handling, calls through "glue code".
     */
.LreturnOld:
    SAVE_PC_FP_TO_SELF()                       # export state
    move       a0, rSELF                       # arg to function
    JAL(dvmMterp_returnFromMethod)
    b          common_resumeAfterGlueCall
    .endif

/*
 * Somebody has thrown an exception.  Handle it.
 *
 * If the exception processing code returns to us (instead of falling
 * out of the interpreter), continue with whatever the next instruction
 * now happens to be.
 *
 * This does not return.
 */
    .global dvmMterpCommonExceptionThrown
dvmMterpCommonExceptionThrown:
common_exceptionThrown:
.LexceptionNew:

    EXPORT_PC()
    move     a0, rSELF
    JAL(dvmCheckSuspendPending)
    lw       rOBJ, offThread_exception(rSELF)
    move     a1, rSELF
    move     a0, rOBJ
    JAL(dvmAddTrackedAlloc)
    lhu      a2, offThread_subMode(rSELF)
    sw       zero, offThread_exception(rSELF)

    # Special subMode?
    bnez     a2, 7f                     # any special subMode handling needed?
8:
    /* set up args and a local for "&fp" */
    sw       rFP, 20(sp)                 #  store rFP => tmp
    addu     t0, sp, 20                  #  compute &tmp
    sw       t0, STACK_OFFSET_ARG04(sp)  #  save it in arg4 as per ABI
    li       a3, 0                       #  a3 <- false
    lw       a1, offThread_method(rSELF)
    move     a0, rSELF
    lw       a1, offMethod_insns(a1)
    move     a2, rOBJ
    subu     a1, rPC, a1
    sra      a1, a1, 1

    /* call, r0 gets catchRelPc (a code-unit offset) */
    JAL(dvmFindCatchBlock)           # call(self, relPc, exc, scan?, &fp)
    lw        rFP, 20(sp)            # retrieve the updated rFP

    /* update frame pointer and check result from dvmFindCatchBlock */
    move      a0, v0
    bltz      v0, .LnotCaughtLocally

    /* fix earlier stack overflow if necessary; Preserve a0 */
    lbu       a1, offThread_stackOverflowed(rSELF)
    beqz      a1, 1f
    move      rBIX, a0
    move      a0, rSELF
    move      a1, rOBJ
    JAL(dvmCleanupStackOverflow)
    move      a0, rBIX

1:

/* adjust locals to match self->interpSave.curFrame and updated PC */
    SAVEAREA_FROM_FP(a1, rFP)           # a1<- new save area
    lw        a1, offStackSaveArea_method(a1)
    sw        a1, offThread_method(rSELF)
    lw        a2, offMethod_clazz(a1)
    lw        a3, offMethod_insns(a1)
    lw        a2, offClassObject_pDvmDex(a2)
    EAS1(rPC, a3, a0)
    sw        a2, offThread_methodClassDex(rSELF)

    /* release the tracked alloc on the exception */
    move      a0, rOBJ
    move      a1, rSELF
    JAL(dvmReleaseTrackedAlloc)

    /* restore the exception if the handler wants it */
    lw        rIBASE, offThread_curHandlerTable(rSELF)
    FETCH_INST()
    GET_INST_OPCODE(t0)
    bne       t0, OP_MOVE_EXCEPTION, 2f
    sw        rOBJ, offThread_exception(rSELF)
2:
    GOTO_OPCODE(t0)

    # Manage debugger bookkeeping
7:
    sw        rPC, offThread_pc(rSELF)
    sw        rFP, offThread_curFrame(rSELF)
    move      a0, rSELF
    move      a1, rOBJ
    JAL(dvmReportExceptionThrow)
    b         8b

.LnotCaughtLocally:                     #  rOBJ = exception
    /* fix stack overflow if necessary */
    lbu       a1, offThread_stackOverflowed(rSELF)
    beqz      a1, 3f
    move      a0, rSELF
    move      a1, rOBJ
    JAL(dvmCleanupStackOverflow)           #  dvmCleanupStackOverflow(self, exception)

3:
    # may want to show "not caught locally" debug messages here
#if DVM_SHOW_EXCEPTION >= 2
    /* call __android_log_print(prio, tag, format, ...) */
    /* "Exception %s from %s:%d not caught locally" */
    lw        a0, offThread_method(rSELF)
    lw        a1, offMethod_insns(a0)
    subu      a1, rPC, a1
    sra       a1, a1, 1
    JAL(dvmLineNumFromPC)
    sw        v0, 20(sp)
    # dvmGetMethodSourceFile(method)
    lw        a0, offThread_method(rSELF)
    JAL(dvmGetMethodSourceFile)
    sw        v0, 16(sp)
    # exception->clazz->descriptor
    lw        a3, offObject_clazz(rOBJ)
    lw        a3, offClassObject_descriptor(a3)
    la        a2, .LstrExceptionNotCaughtLocally
    la        a1, .LstrLogTag
    li        a0, 3
    JAL(__android_log_print)
#endif
    sw        rOBJ, offThread_exception(rSELF)
    move      a0, rOBJ
    move      a1, rSELF
    JAL(dvmReleaseTrackedAlloc)
    b         common_gotoBail

    /*
     * Exception handling, calls through "glue code".
     */
    .if     0
.LexceptionOld:
    SAVE_PC_TO_SELF()                # export state
    SAVE_FP_TO_SELF()
    move     a0, rSELF               # arg to function
    JAL(dvmMterp_exceptionThrown)
    b       common_resumeAfterGlueCall
    .endif

#if defined(WITH_JIT)
    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including the current
     * instruction.
     *
     * On entry:
     *     rBIX: &dvmDex->pResFields[field]
     *     a0:  field pointer (must preserve)
     */
common_verifyField:
     lhu     a3, offThread_subMode(rSELF)
     andi    a3, kSubModeJitTraceBuild
     bnez    a3, 1f                 # Not building trace, continue
     jr      ra
1:
     lw      a1, (rBIX)
     beqz    a1, 2f                 # resolution complete ?
     jr      ra
2:
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)
    SCRATCH_STORE(ra, 16)
    move    a0, rSELF
    move    a1, rPC
    JAL(dvmJitEndTraceSelect)        #(self,pc) end trace before this inst)
    SCRATCH_LOAD(a0, 0)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(ra, 16)
    jr      ra                       # return
#endif

/*
 * After returning from a "glued" function, pull out the updated
 * values and start executing at the next instruction.
 */
common_resumeAfterGlueCall:
    LOAD_PC_FP_FROM_SELF()           #  pull rPC and rFP out of thread
    lw      rIBASE, offThread_curHandlerTable(rSELF) # refresh
    FETCH_INST()                     #  load rINST from rPC
    GET_INST_OPCODE(t0)              #  extract opcode from rINST
    GOTO_OPCODE(t0)                  #  jump to next instruction

/*
 * Invalid array index. Note that our calling convention is strange; we use a1
 * and a3 because those just happen to be the registers all our callers are
 * using. We move a3 before calling the C function, but a1 happens to match.
 * a1: index
 * a3: size
 */
common_errArrayIndex:
    EXPORT_PC()
    move      a0, a3
    JAL(dvmThrowArrayIndexOutOfBoundsException)
    b         common_exceptionThrown

/*
 * Integer divide or mod by zero.
 */
common_errDivideByZero:
    EXPORT_PC()
    la     a0, .LstrDivideByZero
    JAL(dvmThrowArithmeticException)
    b       common_exceptionThrown

/*
 * Attempt to allocate an array with a negative size.
 * On entry: length in a1
 */
common_errNegativeArraySize:
    EXPORT_PC()
    move    a0, a1                                # arg0 <- len
    JAL(dvmThrowNegativeArraySizeException)    # (len)
    b       common_exceptionThrown

/*
 * Invocation of a non-existent method.
 * On entry: method name in a1
 */
common_errNoSuchMethod:
    EXPORT_PC()
    move     a0, a1
    JAL(dvmThrowNoSuchMethodError)
    b       common_exceptionThrown

/*
 * We encountered a null object when we weren't expecting one.  We
 * export the PC, throw a NullPointerException, and goto the exception
 * processing code.
 */
common_errNullObject:
    EXPORT_PC()
    li      a0, 0
    JAL(dvmThrowNullPointerException)
    b       common_exceptionThrown

/*
 * For debugging, cause an immediate fault. The source address will be in ra. Use a jal to jump here.
 */
common_abort:
    lw      zero,-4(zero)            #  generate SIGSEGV

/*
 * Spit out a "we were here", preserving all registers.
 */
    .macro SQUEAK num
common_squeak\num:
    STACK_STORE_RA();
    la        a0, .LstrSqueak
    LOAD_IMM(a1, \num);
    JAL(printf);
    STACK_LOAD_RA();
    RETURN;
    .endm

    SQUEAK 0
    SQUEAK 1
    SQUEAK 2
    SQUEAK 3
    SQUEAK 4
    SQUEAK 5

/*
 * Spit out the number in a0, preserving registers.
 */
common_printNum:
    STACK_STORE_RA()
    MOVE_REG(a1, a0)
    la        a0, .LstrSqueak
    JAL(printf)
    STACK_LOAD_RA()
    RETURN

/*
 * Print a newline, preserving registers.
 */
common_printNewline:
    STACK_STORE_RA()
    la        a0, .LstrNewline
    JAL(printf)
    STACK_LOAD_RA()
    RETURN

    /*
     * Print the 32-bit quantity in a0 as a hex value, preserving registers.
     */
common_printHex:
    STACK_STORE_RA()
    MOVE_REG(a1, a0)
    la        a0, .LstrPrintHex
    JAL(printf)
    STACK_LOAD_RA()
RETURN;

/*
 * Print the 64-bit quantity in a0-a1, preserving registers.
 */
common_printLong:
    STACK_STORE_RA()
    MOVE_REG(a3, a1)
    MOVE_REG(a2, a0)
    la        a0, .LstrPrintLong
    JAL(printf)
    STACK_LOAD_RA()
    RETURN;

/*
 * Print full method info.  Pass the Method* in a0.  Preserves regs.
 */
common_printMethod:
    STACK_STORE_RA()
    JAL(dvmMterpPrintMethod)
    STACK_LOAD_RA()
    RETURN

/*
 * Call a C helper function that dumps regs and possibly some
 * additional info.  Requires the C function to be compiled in.
 */
    .if 0
common_dumpRegs:
    STACK_STORE_RA()
    JAL(dvmMterpDumpMipsRegs)
    STACK_LOAD_RA()
    RETURN
    .endif

/*
 * Zero-terminated ASCII string data.
 */
    .data

.LstrBadEntryPoint:
    .asciiz "Bad entry point %d\n"
.LstrDivideByZero:
    .asciiz "divide by zero"
.LstrFilledNewArrayNotImpl:
    .asciiz "filled-new-array only implemented for 'int'"
.LstrLogTag:
    .asciiz  "mterp"
.LstrExceptionNotCaughtLocally:
    .asciiz  "Exception %s from %s:%d not caught locally\n"

.LstrNewline:
    .asciiz "\n"
.LstrSqueak:
    .asciiz "<%d>"
.LstrPrintHex:
    .asciiz "<0x%x>"
.LstrPrintLong:
    .asciiz "<%lld>"
