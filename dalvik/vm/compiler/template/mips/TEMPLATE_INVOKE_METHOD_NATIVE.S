    # a0 = methodToCall, a1 = returnCell, rPC = dalvikCallsite
    lh     t7, offMethod_registersSize(a0)        # t7<- methodToCall->regsSize
    lw     t9, offThread_interpStackEnd(rSELF)    # t9<- interpStackEnd
    lbu    t8, offThread_breakFlags(rSELF)        # t8<- breakFlags
    move   a3, a1                                 # a3<- returnCell
    SAVEAREA_FROM_FP(a1, rFP)                     # a1<- stack save area
    sll    t6, t7, 2                              # multiply regsSize by 4 (4 bytes per reg)
    sub    a1, a1, t6                             # a1<- newFp(old savearea-regsSize)
    SAVEAREA_FROM_FP(t0, a1)                      # t0<- stack save area
    bgeu   t0, t9, 1f                             # bottom < interpStackEnd?
    RETURN                                        # return to raise stack overflow excep.

1:
    # a1 = newFP, a0 = methodToCall, a3 = returnCell, rPC = dalvikCallsite
    sw     rPC, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)
    sw     rPC, (offStackSaveArea_savedPc - sizeofStackSaveArea)(a1)
    lw     rPC, offMethod_insns(a0)               # rPC<- methodToCall->insns

    # set up newSaveArea
    sw     rFP, (offStackSaveArea_prevFrame - sizeofStackSaveArea)(a1)
    sw     a3, (offStackSaveArea_returnAddr - sizeofStackSaveArea)(a1)
    sw     a0, (offStackSaveArea_method - sizeofStackSaveArea)(a1)
    lw     rTEMP, offMethod_nativeFunc(a0)        # t9<- method->nativeFunc
#if !defined(WITH_SELF_VERIFICATION)
    beqz   t8, 2f                                 # breakFlags != 0
    RETURN                                        # bail to the interpreter
2:
#else
    RETURN                                        # bail to the interpreter unconditionally
#endif

    # go ahead and transfer control to the native code
    lw     t6, offThread_jniLocal_topCookie(rSELF)  # t6<- thread->localRef->...
    sw     a1, offThread_curFrame(rSELF)          # self->curFrame = newFp
    sw     zero, offThread_inJitCodeCache(rSELF)  # not in the jit code cache
    sw     t6, (offStackSaveArea_localRefCookie - sizeofStackSaveArea)(a1)
                                                  # newFp->localRefCookie=top
    SAVEAREA_FROM_FP(rBIX, a1)                    # rBIX<- new stack save area
    move   a2, a0                                 # a2<- methodToCall
    move   a0, a1                                 # a0<- newFp
    add    a1, rSELF, offThread_retval            # a1<- &retval
    move   a3, rSELF                              # a3<- self
#if defined(TEMPLATE_INLINE_PROFILING)
    # a2: methodToCall
    # preserve rTEMP,a0-a3
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(a3, 12)
    SCRATCH_STORE(rTEMP, 16)

    move   a0, a2
    move   a1, rSELF
    # a0=JNIMethod, a1=rSELF
    la      t9, dvmFastMethodTraceEnter
    JALR(t9)                                      # off to the native code
    lw     gp, STACK_OFFSET_GP(sp)

    # restore rTEMP,a0-a3
    SCRATCH_LOAD(rTEMP, 16)
    SCRATCH_LOAD(a3, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)

    move   rOBJ, a2                               # save a2
#endif
    move   t9, rTEMP
    JALR(t9)                                   # off to the native code
    lw     gp, STACK_OFFSET_GP(sp)

#if defined(TEMPLATE_INLINE_PROFILING)
    move   a0, rOBJ
    move   a1, rSELF
    # a0=JNIMethod, a1=rSELF
    la      t9, dvmFastNativeMethodTraceExit
    JALR(t9)
    lw     gp, STACK_OFFSET_GP(sp)
#endif

    # native return; rBIX=newSaveArea
    # equivalent to dvmPopJniLocals
    lw     a2, offStackSaveArea_returnAddr(rBIX)     # a2 = chaining cell ret addr
    lw     a0, offStackSaveArea_localRefCookie(rBIX) # a0<- saved->top
    lw     a1, offThread_exception(rSELF)            # check for exception
    sw     rFP, offThread_curFrame(rSELF)            # self->curFrame = fp
    sw     a0, offThread_jniLocal_topCookie(rSELF)   # new top <- old top
    lw     a0, (offStackSaveArea_currentPc - sizeofStackSaveArea)(rFP)

    # a0 = dalvikCallsitePC
    bnez   a1, .LhandleException                     # handle exception if any

    sw     a2, offThread_inJitCodeCache(rSELF)       # set the mode properly
    beqz   a2, 3f
    jr     a2                                        # go if return chaining cell still exist

3:
    # continue executing the next instruction through the interpreter
    la     a1, .LdvmJitToInterpTraceSelectNoChain    # defined in footer.S
    lw     a1, (a1)
    add    rPC, a0, 3*2                              # reconstruct new rPC (advance 3 dalvik instr)

#if defined(WITH_JIT_TUNING)
    li     a0, kCallsiteInterpreted
#endif
    jr     a1
