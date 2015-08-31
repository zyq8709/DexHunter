%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a direct method call.
     *
     * (We could defer the "is 'this' pointer null" test to the common
     * method invocation code, and use a flag to indicate that static
     * calls don't count.  If we do this as part of copying the arguments
     * out we could avoiding loading the first arg twice.)
     *
     * for: invoke-direct, invoke-direct/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved methodToCall
    .if     (!$isrange)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    beq     .L${opcode}_resolve         @ not resolved, do it now
.L${opcode}_finish:
    cmp     r9, #0                      @ null "this" ref?
    bne     common_invokeMethod${routine}   @ r0=method, r9="this"
    b       common_errNullObject        @ yes, throw exception
%break

    /*
     * On entry:
     *  r1 = reference (BBBB or CCCC)
     *  r10 = "this" register
     */
.L${opcode}_resolve:
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_DIRECT          @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .L${opcode}_finish          @ no, continue
    b       common_exceptionThrown      @ yes, handle exception
