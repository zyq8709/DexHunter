    /*
     * For polymorphic callsites - setup the Dalvik frame and load Dalvik PC
     * into rPC then jump to dvmJitToInterpNoChain to dispatch the
     * runtime-resolved callee.
     */
    @ r0 = methodToCall, r1 = returnCell, rPC = dalvikCallsite
    ldrh    r7, [r0, #offMethod_registersSize]  @ r7<- methodToCall->regsSize
    ldrh    r2, [r0, #offMethod_outsSize]  @ r2<- methodToCall->outsSize
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags] @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    sub     r10, r10, r2, lsl #2        @ r10<- bottom (newsave - outsSize)
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    lr                          @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    ldr     r9, [r0, #offMethod_clazz]      @ r9<- method->clazz
    ldr     r10, [r0, #offMethod_accessFlags] @ r10<- methodToCall->accessFlags
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]
    ldr     rPC, [r0, #offMethod_insns]     @ rPC<- methodToCall->insns


    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    bxne    lr                          @ bail to the interpreter
    tst     r10, #ACC_NATIVE
#if !defined(WITH_SELF_VERIFICATION)
    bne     .LinvokeNative
#else
    bxne    lr                          @ bail to the interpreter
#endif

    ldr     r10, .LdvmJitToInterpTraceSelectNoChain
    ldr     r3, [r9, #offClassObject_pDvmDex] @ r3<- method->clazz->pDvmDex

    @ Update "thread" values for the new method
    str     r0, [rSELF, #offThread_method]    @ self->method = methodToCall
    str     r3, [rSELF, #offThread_methodClassDex] @ self->methodClassDex = ...
    mov     rFP, r1                         @ fp = newFp
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r3}                    @ preserve r0-r3
    mov     r1, r6
    @ r0=methodToCall, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}                    @ restore r0-r3
#endif

    @ Start executing the callee
#if defined(WITH_JIT_TUNING)
    mov     r0, #kInlineCacheMiss
#endif
    bx      r10                         @ dvmJitToInterpTraceSelectNoChain
