    /*
     * Unwind a frame from the Dalvik stack for compiled OP_RETURN_XXX.
     * If the stored value in returnAddr
     * is non-zero, the caller is compiled by the JIT thus return to the
     * address in the code cache following the invoke instruction. Otherwise
     * return to the special dvmJitToInterpNoChain entry point.
     */
#if defined(TEMPLATE_INLINE_PROFILING)
    # preserve a0-a2 and ra
    SCRATCH_STORE(a0, 0)
    SCRATCH_STORE(a1, 4)
    SCRATCH_STORE(a2, 8)
    SCRATCH_STORE(ra, 12)

    # a0=rSELF
    move    a0, rSELF
    la      t9, dvmFastMethodTraceExit
    JALR(t9)
    lw      gp, STACK_OFFSET_GP(sp)

    # restore a0-a2 and ra
    SCRATCH_LOAD(ra, 12)
    SCRATCH_LOAD(a2, 8)
    SCRATCH_LOAD(a1, 4)
    SCRATCH_LOAD(a0, 0)
#endif
    SAVEAREA_FROM_FP(a0, rFP)           # a0<- saveArea (old)
    lw      t0, offStackSaveArea_prevFrame(a0)     # t0<- saveArea->prevFrame
    lbu     t1, offThread_breakFlags(rSELF)        # t1<- breakFlags
    lw      rPC, offStackSaveArea_savedPc(a0)      # rPC<- saveArea->savedPc
#if !defined(WITH_SELF_VERIFICATION)
    lw      t2,  offStackSaveArea_returnAddr(a0)   # t2<- chaining cell ret
#else
    move    t2, zero                               # disable chaining
#endif
    lw      a2, offStackSaveArea_method - sizeofStackSaveArea(t0)
                                                   # a2<- method we're returning to
#if !defined(WITH_SELF_VERIFICATION)
    beq     a2, zero, 1f                           # bail to interpreter
#else
    bne     a2, zero, 2f
    JALR(ra)                                       # punt to interpreter and compare state
    # DOUG: assume this does not return ???
2:
#endif
    la      t4, .LdvmJitToInterpNoChainNoProfile   # defined in footer.S
    lw      a1, (t4)
    move    rFP, t0                                # publish new FP
    beq     a2, zero, 4f
    lw      t0, offMethod_clazz(a2)                # t0<- method->clazz
4:

    sw      a2, offThread_method(rSELF)            # self->method = newSave->method
    lw      a0, offClassObject_pDvmDex(t0)         # a0<- method->clazz->pDvmDex
    sw      rFP, offThread_curFrame(rSELF)         # self->curFrame = fp
    add     rPC, rPC, 3*2                          # publish new rPC
    sw      a0, offThread_methodClassDex(rSELF)
    movn    t2, zero, t1                           # check the breadFlags and
                                                   # clear the chaining cell address
    sw      t2, offThread_inJitCodeCache(rSELF)    # in code cache or not
    beq     t2, zero, 3f                           # chaining cell exists?
    JALR(t2)                                       # jump to the chaining cell
    # DOUG: assume this does not return ???
3:
#if defined(WITH_JIT_TUNING)
    li      a0, kCallsiteInterpreted
#endif
    j       a1                                     # callsite is interpreted
1:
    sw      zero, offThread_inJitCodeCache(rSELF)  # reset inJitCodeCache
    SAVE_PC_TO_SELF()                              # SAVE_PC_FP_TO_SELF()
    SAVE_FP_TO_SELF()
    la      t4, .LdvmMterpStdBail                  # defined in footer.S
    lw      a2, (t4)
    move    a0, rSELF                              # Expecting rSELF in a0
    JALR(a2)                                       # exit the interpreter
    # DOUG: assume this does not return ???
