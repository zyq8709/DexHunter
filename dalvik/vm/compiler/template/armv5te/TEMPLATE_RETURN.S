    /*
     * Unwind a frame from the Dalvik stack for compiled OP_RETURN_XXX.
     * If the stored value in returnAddr
     * is non-zero, the caller is compiled by the JIT thus return to the
     * address in the code cache following the invoke instruction. Otherwise
     * return to the special dvmJitToInterpNoChain entry point.
     */
#if defined(TEMPLATE_INLINE_PROFILING)
    stmfd   sp!, {r0-r2,lr}             @ preserve live registers
    mov     r0, r6
    @ r0=rSELF
    ldr     ip, .LdvmFastMethodTraceExit
    blx     ip
    ldmfd   sp!, {r0-r2,lr}             @ restore live registers
#endif
    SAVEAREA_FROM_FP(r0, rFP)           @ r0<- saveArea (old)
    ldr     r10, [r0, #offStackSaveArea_prevFrame] @ r10<- saveArea->prevFrame
    ldrb    r8, [rSELF, #offThread_breakFlags] @ r8<- breakFlags
    ldr     rPC, [r0, #offStackSaveArea_savedPc] @ rPC<- saveArea->savedPc
    ldr     r2, [r10, #(offStackSaveArea_method - sizeofStackSaveArea)]
#if !defined(WITH_SELF_VERIFICATION)
    ldr     r9,  [r0, #offStackSaveArea_returnAddr] @ r9<- chaining cell ret
#else
    mov     r9, #0                      @ disable chaining
#endif
                                        @ r2<- method we're returning to
    cmp     r2, #0                      @ break frame?
#if !defined(WITH_SELF_VERIFICATION)
    beq     1f                          @ bail to interpreter
#else
    blxeq   lr                          @ punt to interpreter and compare state
#endif
    ldr     r1, .LdvmJitToInterpNoChainNoProfile @ defined in footer.S
    mov     rFP, r10                    @ publish new FP
    ldr     r10, [r2, #offMethod_clazz] @ r10<- method->clazz

    str     r2, [rSELF, #offThread_method]@ self->method = newSave->method
    ldr     r0, [r10, #offClassObject_pDvmDex] @ r0<- method->clazz->pDvmDex
    str     rFP, [rSELF, #offThread_curFrame] @ curFrame = fp
    add     rPC, rPC, #6                @ publish new rPC (advance 6 bytes)
    str     r0, [rSELF, #offThread_methodClassDex]
    cmp     r8, #0                      @ check the break flags
    movne   r9, #0                      @ clear the chaining cell address
    str     r9, [rSELF, #offThread_inJitCodeCache] @ in code cache or not
    cmp     r9, #0                      @ chaining cell exists?
    blxne   r9                          @ jump to the chaining cell
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    mov     pc, r1                      @ callsite is interpreted
1:
    mov     r0, #0
    str     r0, [rSELF, #offThread_inJitCodeCache] @ reset inJitCodeCache
    stmia   rSELF, {rPC, rFP}           @ SAVE_PC_FP_TO_SELF()
    ldr     r2, .LdvmMterpStdBail       @ defined in footer.S
    mov     r0, rSELF                   @ Expecting rSELF in r0
    blx     r2                          @ exit the interpreter
