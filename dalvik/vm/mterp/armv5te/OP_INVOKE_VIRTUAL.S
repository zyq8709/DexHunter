%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
%verify "null object"
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved baseMethod
    .if     (!$isrange)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    cmp     r0, #0                      @ already resolved?
    EXPORT_PC()                         @ must export for invoke
    bne     .L${opcode}_continue        @ yes, continue on
    ldr     r3, [rSELF, #offThread_method] @ r3<- self->method
    ldr     r0, [r3, #offMethod_clazz]  @ r0<- method->clazz
    mov     r2, #METHOD_VIRTUAL         @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .L${opcode}_continue        @ no, continue
    b       common_exceptionThrown      @ yes, handle exception
%break

    /*
     * At this point:
     *  r0 = resolved base method
     *  r10 = C or CCCC (index of first arg, which is the "this" ptr)
     */
.L${opcode}_continue:
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    ldrh    r2, [r0, #offMethod_methodIndex]    @ r2<- baseMethod->methodIndex
    cmp     r9, #0                      @ is "this" null?
    beq     common_errNullObject        @ null "this", throw exception
    ldr     r3, [r9, #offObject_clazz]  @ r3<- thisPtr->clazz
    ldr     r3, [r3, #offClassObject_vtable]    @ r3<- thisPtr->clazz->vtable
    ldr     r0, [r3, r2, lsl #2]        @ r3<- vtable[methodIndex]
    bl      common_invokeMethod${routine} @ (r0=method, r9="this")
