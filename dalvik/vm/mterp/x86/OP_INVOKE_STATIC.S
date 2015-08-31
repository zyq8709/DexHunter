%default { "routine":"NoRange","isrange":"0" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a static method call.
     *
     * for: invoke-static, invoke-static/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx  # ecx<- pDvmDex->pResMethods
#if defined(WITH_JIT)
    movl     %edx, TMP_SPILL1(%ebp)
    lea      (%ecx,%eax,4), %edx
    movl     %edx, TMP_SPILL2(%ebp)
    movl     TMP_SPILL1(%ebp), %edx
#endif
    movl      (%ecx,%eax,4),%eax        # eax<- resolved methodToCall
    movl      $$0, %ecx                 # make "this" null
    testl     %eax,%eax
    jne       common_invokeMethod${routine}

    movl      rSELF,%ecx
    movl      offThread_method(%ecx),%ecx # ecx<- self->method
    movzwl    2(rPC),%eax
    movl      offMethod_clazz(%ecx),%ecx# ecx<- method->clazz
    movl      %eax,OUT_ARG1(%esp)       # arg1<- BBBB
    movl      %ecx,OUT_ARG0(%esp)       # arg0<- clazz
    movl      $$METHOD_STATIC,%eax
    movl      %eax,OUT_ARG2(%esp)       # arg2<- flags
    SPILL(rIBASE)
    call      dvmResolveMethod          # call(clazz,ref,flags)
    UNSPILL(rIBASE)
    testl     %eax,%eax                 # got null?
#if defined(WITH_JIT)
    movl      TMP_SPILL1(%ebp), %edx
    movl      rSELF,%ecx
    movzwl    offThread_subMode(%ecx), %ecx
    je        common_exceptionThrown    # null, handle exception
    andl      $$kSubModeJitTraceBuild, %ecx # is trace under construction?
    movl      $$0, %ecx                 # make "this" null
    je        common_invokeMethod${routine} # no (%eax=method, %ecx="this")
    movl      TMP_SPILL2(%ebp), %edx
    cmpl      $$0, (%edx)                  # finished resolving
    movl      TMP_SPILL1(%ebp), %edx
    jne        common_invokeMethod${routine} # yes (%eax=method, %ecx="this")
    movl      rSELF, %edx
    movl      %edx, OUT_ARG0(%esp)
    movl      rPC, OUT_ARG1(%esp)
    movl      %eax, TMP_SPILL2(%ebp)
    movl      %ecx, TMP_SPILL3(%ebp)
    SPILL(rIBASE)
    call      dvmJitEndTraceSelect
    UNSPILL(rIBASE)
    movl      TMP_SPILL1(%ebp), %edx
    movl      TMP_SPILL2(%ebp), %eax
    movl      TMP_SPILL3(%ebp), %ecx
    jmp       common_invokeMethod${routine}
#else
    movl      $$0, %ecx                 # make "this" null
    jne       common_invokeMethod${routine}
    jmp       common_exceptionThrown
#endif

