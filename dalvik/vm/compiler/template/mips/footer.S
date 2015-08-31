/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .section .data.rel.ro
    .align  4
.LinvokeNative:
    # Prep for the native call
    # a1 = newFP, a0 = methodToCall
    lw     t9, offThread_jniLocal_topCookie(rSELF)  # t9<- thread->localRef->...
    sw     zero, offThread_inJitCodeCache(rSELF)    # not in jit code cache
    sw     a1, offThread_curFrame(rSELF)            # self->curFrame = newFp
    sw     t9, (offStackSaveArea_localRefCookie - sizeofStackSaveArea)(a1)
                                                 # newFp->localRefCookie=top
    lhu     ra, offThread_subMode(rSELF)
    SAVEAREA_FROM_FP(rBIX, a1)                   # rBIX<- new stack save area

    move    a2, a0                               # a2<- methodToCall
    move    a0, a1                               # a0<- newFp
    add     a1, rSELF, offThread_retval          # a1<- &retval
    move    a3, rSELF                            # a3<- self
    andi    ra, kSubModeMethodTrace
    beqz    ra, 121f
    # a2: methodToCall
    # preserve a0-a3
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)

    move    a0, a2
    move    a1, rSELF
    la      t9, dvmFastMethodTraceEnter
    JALR(t9)
    lw      gp, STACK_OFFSET_GP(sp)

    # restore a0-a3
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    lw      t9, offMethod_nativeFunc(a2)
    JALR(t9)                                      # call methodToCall->nativeFunc
    lw      gp, STACK_OFFSET_GP(sp)

    # restore a2 again
    SCRATCH_LOAD(a2, 8)

    move    a0, a2
    move    a1, rSELF
    la      t9, dvmFastNativeMethodTraceExit
    JALR(t9)
    lw      gp, STACK_OFFSET_GP(sp)
    b       212f

121:
    lw      t9, offMethod_nativeFunc(a2)
    JALR(t9)                                     # call methodToCall->nativeFunc
    lw      gp, STACK_OFFSET_GP(sp)

212:
    # native return; rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw     a2, offStackSaveArea_returnAddr(rBIX)     # a2 = chaining cell ret addr
    lw     a0, offStackSaveArea_localRefCookie(rBIX) # a0<- saved->top
    lw     a1, offThread_exception(rSELF)            # check for exception
    sw     rFP, offThread_curFrame(rSELF)            # self->curFrame = fp
    sw     a0, offThread_jniLocal_topCookie(rSELF)   # new top <- old top
    lw     a0, offStackSaveArea_savedPc(rBIX)        # reload rPC

    # a0 = dalvikCallsitePC
    bnez   a1, .LhandleException                     # handle exception if any

    sw     a2, offThread_inJitCodeCache(rSELF)       # set the mode properly
    beqz   a2, 3f
    jr     a2                                        # go if return chaining cell still exist

3:
    # continue executing the next instruction through the interpreter
    la     a1, .LdvmJitToInterpTraceSelectNoChain    # defined in footer.S
    lw     a1, (a1)
    add    rPC, a0, 3*2                              # reconstruct new rPC

#if defined(WITH_JIT_TUNING)
    li     a0, kCallsiteInterpreted
#endif
    jr     a1


/*
 * On entry:
 * a0  Faulting Dalvik PC
 */
.LhandleException:
#if defined(WITH_SELF_VERIFICATION)
    la     t0, .LdeadFood
    lw     t0, (t0)                  # should not see this under self-verification mode
    jr     t0
.LdeadFood:
    .word   0xdeadf00d
#endif
    sw     zero, offThread_inJitCodeCache(rSELF)  # in interpreter land
    la     a1, .LdvmMterpCommonExceptionThrown  # PIC way of getting &func
    lw     a1, (a1)
    la     rIBASE, .LdvmAsmInstructionStart     # PIC way of getting &func
    lw     rIBASE, (rIBASE)
    move   rPC, a0                              # reload the faulting Dalvid address
    jr     a1                                   # branch to dvmMterpCommonExeceptionThrown

    .align  4
.LdvmAsmInstructionStart:
    .word   dvmAsmInstructionStart
.LdvmJitToInterpNoChainNoProfile:
    .word   dvmJitToInterpNoChainNoProfile
.LdvmJitToInterpTraceSelectNoChain:
    .word   dvmJitToInterpTraceSelectNoChain
.LdvmJitToInterpNoChain:
    .word   dvmJitToInterpNoChain
.LdvmMterpStdBail:
    .word   dvmMterpStdBail
.LdvmMterpCommonExceptionThrown:
    .word   dvmMterpCommonExceptionThrown
.LdvmLockObject:
    .word   dvmLockObject
#if defined(WITH_JIT_TUNING)
.LdvmICHitCount:
    .word   gDvmICHitCount
#endif
#if defined(WITH_SELF_VERIFICATION)
.LdvmSelfVerificationMemOpDecode:
    .word   dvmSelfVerificationMemOpDecode
#endif

    .global dvmCompilerTemplateEnd
dvmCompilerTemplateEnd:

#endif /* WITH_JIT */
