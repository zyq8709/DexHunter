%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
%verify "null object"
    /*
     * Handle an interface method call.
     *
     * for: invoke-interface, invoke-interface/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    FETCH(r2, 2)                        @ r2<- FEDC or CCCC
    FETCH(r1, 1)                        @ r1<- BBBB
    .if     (!$isrange)
    and     r2, r2, #15                 @ r2<- C (or stays CCCC)
    .endif
    EXPORT_PC()                         @ must export for invoke
    GET_VREG(r9, r2)                    @ r9<- first arg ("this")
    ldr     r3, [rSELF, #offThread_methodClassDex]    @ r3<- methodClassDex
    cmp     r9, #0                      @ null obj?
    ldr     r2, [rSELF, #offThread_method]  @ r2<- method
    beq     common_errNullObject        @ yes, fail
    ldr     r0, [r9, #offObject_clazz]  @ r0<- thisPtr->clazz
    bl      dvmFindInterfaceMethodInCache @ r0<- call(class, ref, method, dex)
    cmp     r0, #0                      @ failed?
    beq     common_exceptionThrown      @ yes, handle exception
    b       common_invokeMethod${routine} @ (r0=method, r9="this")
