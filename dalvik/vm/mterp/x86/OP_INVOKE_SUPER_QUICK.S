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
    movl      rSELF,%ecx
    movzwl    4(rPC),%eax               # eax<- GFED or CCCC
    movl      offThread_method(%ecx),%ecx # ecx<- current method
    .if       (!$isrange)
    andl      $$0xf,%eax                # eax<- D (or stays CCCC)
    .endif
    movl      offMethod_clazz(%ecx),%ecx # ecx<- method->clazz
    GET_VREG_R  %eax %eax               # eax<- "this"
    movl      offClassObject_super(%ecx),%ecx # ecx<- method->clazz->super
    testl     %eax,%eax                 # null "this"?
    je        common_errNullObject      # "this" is null, throw exception
    movl       %eax, TMP_SPILL1(%ebp)
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offClassObject_vtable(%ecx),%ecx # ecx<- vtable
    EXPORT_PC
    movl      (%ecx,%eax,4),%eax        # eax<- super->vtable[BBBB]
    movl      TMP_SPILL1(%ebp), %ecx
    jmp       common_invokeMethod${routine}
