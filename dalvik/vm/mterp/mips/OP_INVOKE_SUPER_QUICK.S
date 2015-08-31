%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle an optimized "super" method call.
     *
     * for: [opt] invoke-super-quick, invoke-super-quick/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(t0, 2)                           #  t0 <- GFED or CCCC
    LOAD_rSELF_method(a2)                  #  a2 <- current method
    .if (!$isrange)
    and       t0, t0, 15                   #  t0 <- D (or stays CCCC)
    .endif
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offMethod_clazz(a2, a2)      #  a2 <- method->clazz
    EXPORT_PC()                            #  must export for invoke
    LOAD_base_offClassObject_super(a2, a2) #  a2 <- method->clazz->super
    GET_VREG(rOBJ, t0)                     #  rOBJ <- "this"
    LOAD_base_offClassObject_vtable(a2, a2) #  a2 <- ...clazz->super->vtable
    # is "this" null ?
    LOAD_eas2(a0, a2, a1)                  #  a0 <- super->vtable[BBBB]
    beqz      rOBJ, common_errNullObject   #  "this" is null, throw exception
    b         common_invokeMethod${routine} #  (a0=method, rOBJ="this")

