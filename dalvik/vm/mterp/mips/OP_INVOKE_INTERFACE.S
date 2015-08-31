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
    FETCH(a2, 2)                           #  a2 <- FEDC or CCCC
    FETCH(a1, 1)                           #  a1 <- BBBB
    .if (!$isrange)
    and       a2, 15                       #  a2 <- C (or stays CCCC)
    .endif
    EXPORT_PC()                            #  must export for invoke
    GET_VREG(rOBJ, a2)                     #  rOBJ <- first arg ("this")
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- methodClassDex
    LOAD_rSELF_method(a2)                  #  a2 <- method
    # null obj?
    beqz      rOBJ, common_errNullObject   #  yes, fail
    LOAD_base_offObject_clazz(a0, rOBJ)      #  a0 <- thisPtr->clazz
    JAL(dvmFindInterfaceMethodInCache)     #  v0 <- call(class, ref, method, dex)
    move      a0, v0
    # failed?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         common_invokeMethod${routine} #  (a0=method, rOBJ="this")
