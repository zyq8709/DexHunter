%default { "chaintgt" : ".LinvokeChain" }
    /*
     * For monomorphic callsite, setup the Dalvik frame and return to the
     * Thumb code through the link register to transfer control to the callee
     * method through a dedicated chaining cell.
     */
    @ r0 = methodToCall, r1 = returnCell, r2 = methodToCall->outsSize
    @ rPC = dalvikCallsite, r7 = methodToCall->registersSize
    @ methodToCall is guaranteed to be non-native
$chaintgt:
    ldr     r9, [rSELF, #offThread_interpStackEnd]    @ r9<- interpStackEnd
    ldrb    r8, [rSELF, #offThread_breakFlags]        @ r8<- breakFlags
    add     r3, r1, #1  @ Thumb addr is odd
    SAVEAREA_FROM_FP(r1, rFP)           @ r1<- stack save area
    sub     r1, r1, r7, lsl #2          @ r1<- newFp (old savearea - regsSize)
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- stack save area
    add     r12, lr, #2                 @ setup the punt-to-interp address
    sub     r10, r10, r2, lsl #2        @ r10<- bottom (newsave - outsSize)
    cmp     r10, r9                     @ bottom < interpStackEnd?
    bxlo    r12                         @ return to raise stack overflow excep.
    @ r1 = newFP, r0 = methodToCall, r3 = returnCell, rPC = dalvikCallsite
    ldr     r9, [r0, #offMethod_clazz]      @ r9<- method->clazz
    str     rPC, [rFP, #(offStackSaveArea_currentPc - sizeofStackSaveArea)]
    str     rPC, [r1, #(offStackSaveArea_savedPc - sizeofStackSaveArea)]

    @ set up newSaveArea
    str     rFP, [r1, #(offStackSaveArea_prevFrame - sizeofStackSaveArea)]
    str     r3, [r1, #(offStackSaveArea_returnAddr - sizeofStackSaveArea)]
    str     r0, [r1, #(offStackSaveArea_method - sizeofStackSaveArea)]
    cmp     r8, #0                      @ breakFlags != 0
    bxne    r12                         @ bail to the interpreter

    ldr     r3, [r9, #offClassObject_pDvmDex] @ r3<- method->clazz->pDvmDex

    @ Update "thread" values for the new method
    str     r0, [rSELF, #offThread_method]    @ self->method = methodToCall
    str     r3, [rSELF, #offThread_methodClassDex] @ self->methodClassDex = ...
    mov     rFP, r1                         @ fp = newFp
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = newFp
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r2,lr}             @ preserve clobbered live registers
    mov     r1, r6
    @ r0=methodToCall, r1=rSELF
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r2,lr}             @ restore registers
#endif

    bx      lr                              @ return to the callee-chaining cell
