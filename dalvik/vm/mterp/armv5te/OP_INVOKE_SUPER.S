%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- pDvmDex
    .if     (!$isrange)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r3, [r3, #offDvmDex_pResMethods]    @ r3<- pDvmDex->pResMethods
    GET_VREG(r9, r10)                   @ r9<- "this" ptr
    ldr     r0, [r3, r1, lsl #2]        @ r0<- resolved baseMethod
    cmp     r9, #0                      @ null "this"?
    ldr     r10, [rSELF, #offThread_method] @ r10<- current method
    beq     common_errNullObject        @ null "this", throw exception
    cmp     r0, #0                      @ already resolved?
    ldr     r10, [r10, #offMethod_clazz]  @ r10<- method->clazz
    EXPORT_PC()                         @ must export for invoke
    bne     .L${opcode}_continue        @ resolved, continue on
    b       .L${opcode}_resolve         @ do resolve now
%break

    /*
     * At this point:
     *  r0 = resolved base method
     *  r10 = method->clazz
     */
.L${opcode}_continue:
    ldr     r1, [r10, #offClassObject_super]    @ r1<- method->clazz->super
    ldrh    r2, [r0, #offMethod_methodIndex]    @ r2<- baseMethod->methodIndex
    ldr     r3, [r1, #offClassObject_vtableCount]   @ r3<- super->vtableCount
    EXPORT_PC()                         @ must export for invoke
    cmp     r2, r3                      @ compare (methodIndex, vtableCount)
    bcs     .L${opcode}_nsm             @ method not present in superclass
    ldr     r1, [r1, #offClassObject_vtable]    @ r1<- ...clazz->super->vtable
    ldr     r0, [r1, r2, lsl #2]        @ r3<- vtable[methodIndex]
    bl      common_invokeMethod${routine} @ continue on

.L${opcode}_resolve:
    mov     r0, r10                     @ r0<- method->clazz
    mov     r2, #METHOD_VIRTUAL         @ resolver method type
    bl      dvmResolveMethod            @ r0<- call(clazz, ref, flags)
    cmp     r0, #0                      @ got null?
    bne     .L${opcode}_continue        @ no, continue
    b       common_exceptionThrown      @ yes, handle exception

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  r0 = resolved base method
     */
.L${opcode}_nsm:
    ldr     r1, [r0, #offMethod_name]   @ r1<- method name
    b       common_errNoSuchMethod
