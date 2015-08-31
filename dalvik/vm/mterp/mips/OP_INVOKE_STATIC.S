%default { "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    # op vB, {vD, vE, vF, vG, vA}, class   /* CCCC */
    # op {vCCCC..v(CCCC+AA-1)}, meth       /* BBBB */
    LOAD_rSELF_methodClassDex(a3)          #  a3 <- pDvmDex
    FETCH(a1, 1)                           #  a1 <- BBBB
    LOAD_base_offDvmDex_pResMethods(a3, a3) #  a3 <- pDvmDex->pResMethods
    li      rOBJ, 0                        #  null "this" in delay slot
    LOAD_eas2(a0, a3, a1)                  #  a0 <- resolved methodToCall
#if defined(WITH_JIT)
    EAS2(rBIX, a3, a1)                     #  rBIX<- &resolved_metherToCall
#endif
    EXPORT_PC()                            #  must export for invoke
    # already resolved?
    bnez      a0, common_invokeMethod${routine} #  yes, continue on
    b         .L${opcode}_resolve
%break

.L${opcode}_resolve:
    LOAD_rSELF_method(a3)                  #  a3 <- self->method
    LOAD_base_offMethod_clazz(a0, a3)      #  a0 <- method->clazz
    li        a2, METHOD_STATIC            #  resolver method type
    JAL(dvmResolveMethod)                  #  v0 <- call(clazz, ref, flags)
    move      a0, v0
#if defined(WITH_JIT)
    /*
     * Check to see if we're actively building a trace.  If so,
     * we need to keep this instruction out of it.
     * rBIX: &resolved_methodToCall
     */
    lhu       a2, offThread_subMode(rSELF)
    beqz      v0, common_exceptionThrown   #  null, handle exception
    and       a2, kSubModeJitTraceBuild    #  trace under construction?
    beqz      a2, common_invokeMethod${routine} #  no, (a0=method, rOBJ="this")
    lw        a1, 0(rBIX)                  #  reload resolved method
    # finished resloving?
    bnez      a1, common_invokeMethod${routine} #  yes, (a0=method, rOBJ="this")
    move      rBIX, a0                     #  preserve method
    move      a0, rSELF
    move      a1, rPC
    JAL(dvmJitEndTraceSelect)              #  (self, pc)
    move      a0, rBIX
    b         common_invokeMethod${routine} #  whew, finally!
#else
    # got null?
    bnez      v0, common_invokeMethod${routine} #  (a0=method, rOBJ="this")
    b         common_exceptionThrown       #  yes, handle exception
#endif
