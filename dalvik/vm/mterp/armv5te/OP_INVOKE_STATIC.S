%default { "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    mov     r9, #0                      @ null "this" in delay slot
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved methodToCall
#if defined(WITH_JIT)
    add     r10, r3, r1, lsl #2         @ r10<- &resolved_methodToCall
#endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    bne     common_invokeMethod${routine} @ yes, continue on
    b       .L${opcode}_resolve
%break


.L${opcode}_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_STATIC          @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
#if defined(WITH_JIT)
    /*
     * Check to see if we're actively building a trace.  If so,
     * we need to keep this instruction out of it.
     * r10: &resolved_methodToCall
     */
    ldrh    r2, [rSELF, #offThread_subMode]
    beq     common_exceptionThrown            @ null, handle exception
    ands    r2, #kSubModeJitTraceBuild        @ trace under construction?
    beq     common_invokeMethod${routine}     @ no (r0=method, r9="this")
    ldr     r1, [r10]                         @ reload resolved method
    cmp     r1, #0                            @ finished resolving?
    bne     common_invokeMethod${routine}     @ yes (r0=method, r9="this")
    mov     r10, r0                           @ preserve method
    mov     r0, rSELF
    mov     r1, rPC
    bl      dvmJitEndTraceSelect              @ (self, pc)
    mov     r0, r10
    b       common_invokeMethod${routine}     @ whew, finally!
#else
    bne     common_invokeMethod${routine}     @ (r0=method, r9="this")
    b       common_exceptionThrown            @ yes, handle exception
#endif
