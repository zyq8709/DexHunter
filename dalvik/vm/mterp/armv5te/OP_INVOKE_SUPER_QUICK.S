%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r10, 2)                       @ r10<- GFED or CCCC
    ldr     r2, [rSELF, #offThread_method]    @ r2<- current method
    .if     (!$isrange)
    and     r10, r10, #15               @ r10<- D (or stays CCCC)
    .endif
    FETCH(r1, 1)                        @ r1<- BBBB
    ldr     r2, [r2, #offMethod_clazz]  @ r2<- method->clazz
    EXPORT_PC()                         @ must export for invoke
    ldr     r2, [r2, #offClassObject_super]     @ r2<- method->clazz->super
    GET_VREG(r9, r10)                   @ r9<- "this"
    ldr     r2, [r2, #offClassObject_vtable]    @ r2<- ...clazz->super->vtable
    cmp     r9, #0                      @ null "this" ref?
    ldr     r0, [r2, r1, lsl #2]        @ r0<- super->vtable[BBBB]
    beq     common_errNullObject        @ "this" is null, throw exception
    bl      common_invokeMethod${routine} @ (r0=method, r9="this")
