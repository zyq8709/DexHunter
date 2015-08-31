%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "null object"
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(a3, 2)                           #  a3 <- FEDC or CCCC
    FETCH(a1, 1)                           #  a1 <- BBBB
    .if (!$isrange)
    and       a3, a3, 15                   #  a3 <- C (or stays CCCC)
    .endif
    GET_VREG(rOBJ, a3)                     #  rOBJ <- vC ("this" ptr)
    # is "this" null?
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    LOAD_base_offObject_clazz(a2, rOBJ)    #  a2 <- thisPtr->clazz
    LOAD_base_offClassObject_vtable(a2, a2) #  a2 <- thisPtr->clazz->vtable
    EXPORT_PC()                            #  invoke must export
    LOAD_eas2(a0, a2, a1)                  #  a0 <- vtable[BBBB]
    b         common_invokeMethod${routine} #  (a0=method, r9="this")
