
%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
%verify "null object"
    /*
     * Handle a virtual method call.
     *
     * for: invoke-virtual, invoke-virtual/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,%eax
    movzwl    2(rPC),%ecx                 # ecx<- BBBB
    movl      offThread_methodClassDex(%eax),%eax  # eax<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%eax),%eax   # eax<- pDvmDex->pResMethods
    movl      (%eax,%ecx,4),%eax          # eax<- resolved baseMethod
    testl     %eax,%eax                   # already resolved?
    jne       .L${opcode}_continue        # yes, continue
    movl      rSELF,%eax
    movl      %ecx,OUT_ARG1(%esp)         # arg1<- ref
    movl      offThread_method(%eax),%eax   # eax<- self->method
    movl      offMethod_clazz(%eax),%eax  # ecx<- method->clazz
    movl      %eax,OUT_ARG0(%esp)         # arg0<- clazz
    movl      $$METHOD_VIRTUAL,OUT_ARG2(%esp) # arg2<- flags
    call      dvmResolveMethod            # eax<- call(clazz, ref, flags)
    testl     %eax,%eax                   # got null?
    jne       .L${opcode}_continue        # no, continue
    jmp       common_exceptionThrown      # yes, handle exception

    /* At this point:
     *   eax = resolved base method
     *   ecx = scratch
     */
.L${opcode}_continue:
    movzwl    4(rPC),%ecx               # ecx<- GFED or CCCC
    .if       (!$isrange)
    andl      $$0xf,%ecx                # ecx<- D (or stays CCCC)
    .endif
    GET_VREG_R  %ecx %ecx               # ecx<- "this"
    movzwl    offMethod_methodIndex(%eax),%eax  # eax<- baseMethod->methodIndex
    testl     %ecx,%ecx                 # null this?
    je        common_errNullObject      # go if so
    movl      offObject_clazz(%ecx),%edx  # edx<- thisPtr->clazz
    movl      offClassObject_vtable(%edx),%edx # edx<- thisPtr->clazz->vtable
    movl      (%edx,%eax,4),%eax        # eax<- vtable[methodIndex]
    jmp       common_invokeMethod${routine}
