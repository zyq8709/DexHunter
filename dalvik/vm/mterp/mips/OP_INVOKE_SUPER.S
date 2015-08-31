%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op vAA, {vCCCC..v(CCCC+AA-1)}, meth  /* BBBB */
    FETCH(t0, 2)                           #  t0 <- GFED or CCCC
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    .if (!$isrange)
    and       t0, t0, 15                   #  t0 <- D (or stays CCCC)
    .endif
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    GET_VREG(rOBJ, t0)                     #  rOBJ <- "this" ptr
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved baseMethod
    # null "this"?
    LOAD_rSELF_method(t1)                  #  t1 <- current method
    beqz      rOBJ, common_errNullObject   #  null "this", throw exception
    # cmp a0, 0; already resolved?
    LOAD_base_offMethod_clazz(rBIX, t1)    #  rBIX <- method->clazz
    EXPORT_PC()                            #  must export for invoke
    bnez      a0, .L${opcode}_continue     #  resolved, continue on

    move      a0, rBIX                     #  a0 <- method->clazz
    li        a2, METHOD_VIRTUAL           #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
    # got null?
    beqz      v0, common_exceptionThrown   #  yes, handle exception
    b         .L${opcode}_continue
%break

    /*
     * At this point:
     *  a0 = resolved base method
     *  rBIX = method->clazz
     */
.L${opcode}_continue:
    LOAD_base_offClassObject_super(a1, rBIX) #  a1 <- method->clazz->super
    LOADu2_offMethod_methodIndex(a2, a0)   #  a2 <- baseMethod->methodIndex
    LOAD_base_offClassObject_vtableCount(a3, a1) #  a3 <- super->vtableCount
    EXPORT_PC()                            #  must export for invoke
    # compare (methodIndex, vtableCount)
    bgeu      a2, a3, .L${opcode}_nsm      #  method not present in superclass
    LOAD_base_offClassObject_vtable(a1, a1) #  a1 <- ...clazz->super->vtable
    LOAD_eas2(a0, a1, a2)                  #  a0 <- vtable[methodIndex]
    b         common_invokeMethod${routine} #  continue on

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  a0 = resolved base method
     */
.L${opcode}_nsm:
    LOAD_base_offMethod_name(a1, a0)       #  a1 <- method name
    b         common_errNoSuchMethod

