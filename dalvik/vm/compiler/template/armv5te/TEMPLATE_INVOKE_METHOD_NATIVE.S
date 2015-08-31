    @ r0 = methodToCall, r1 = returnCell, rPC = dalvikCallsite
    @ r7 = methodToCall->registersSize
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags]        @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    lr                          @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]

    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    ldr     r8, [r0, #offMethod_nativeFunc] @ r8<- method->nativeFunc
#if !defined(WITH_SELF_VERIFICATION)
    bxne    lr                          @ bail to the interpreter
#else
    bx      lr                          @ bail to interpreter unconditionally
#endif

    @ go ahead and transfer control to the native code
    ldr     r9, [rSELF, #offThread_jniLocal_topCookie]@r9<-thread->localRef->...
    mov     r2, #0
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    str     r2, [rSELF, #offThread_inJitCodeCache] @ not in the jit code cache
    str     r9, [r1, #(offStackSaveArea_localRefCookie - sizeofStackSaveArea)]
                                        @ newFp->localRefCookie=top
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- new stack save area

    mov     r2, r0                        @ arg2<- methodToCall
    mov     r0, r1                        @ arg0<- newFP
    add     r1, rSELF, #offThread_retval  @ arg1<- &retval
    mov     r3, rSELF                     @ arg3<- self
#if defined(TEMPLATE_INLINE_PROFILING)
    @ r2=methodToCall, r6=rSELF
    stmfd   sp!, {r2,r6}                @ to be consumed after JNI return
    stmfd   sp!, {r0-r3}                @ preserve r0-r3
    mov     r0, r2
    mov     r1, r6
    @ r0=JNIMethod, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}                @ restore r0-r3
#endif

    blx     r8                          @ off to the native code

#if defined(TEMPLATE_INLINE_PROFILING)
    ldmfd   sp!, {r0-r1}                @ restore r2 and r6
    @ r0=JNIMethod, r1=rSELF
    ldr     ip, .LdvmFastNativeMethodTraceExit
    blx     ip
#endif
    @ native return; r10=newSaveArea
    @ equivalent to dvmPopJniLocals
    ldr     r2, [r10, #offStackSaveArea_returnAddr] @ r2 = chaining cell ret
    ldr     r0, [r10, #offStackSaveArea_localRefCookie] @ r0<- saved->top
    ldr     r1, [rSELF, #offThread_exception] @ check for exception
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = fp
    cmp     r1, #0                      @ null?
    str     r0, [rSELF, #offThread_jniLocal_topCookie] @ new top <- old top
    ldr     r0, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]

    @ r0 = dalvikCallsitePC
    bne     .LhandleException           @ no, handle exception

    str     r2, [rSELF, #offThread_inJitCodeCache] @ set the mode properly
    cmp     r2, #0                      @ return chaining cell still exists?
    bxne    r2                          @ yes - go ahead

    @ continue executing the next instruction through the interpreter
    ldr     r1, .LdvmJitToInterpTraceSelectNoChain @ defined in footer.S
    add     rPC, r0, #6                 @ reconstruct new rPC (advance 6 bytes)
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    mov     pc, r1
