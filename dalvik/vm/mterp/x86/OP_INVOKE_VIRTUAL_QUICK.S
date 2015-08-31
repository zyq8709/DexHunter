%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "null object"
    /*
     * Handle an optimized virtual method call.
     *
     * for: [opt] invoke-virtual-quick, invoke-virtual-quick/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movzwl    4(rPC),%ecx               # eax<- FEDC or CCCC
    movzwl    2(rPC),%edx               # ecx<- BBBB
    .if     (!$isrange)
    andl      $$0xf,%ecx                # eax<- C (or stays CCCC)
    .endif
    GET_VREG_R  %ecx %ecx               # ecx<- vC ("this" ptr)
    testl     %ecx,%ecx                 # null?
    je        common_errNullObject      # yep, throw exception
    movl      offObject_clazz(%ecx),%eax # eax<- thisPtr->clazz
    movl      offClassObject_vtable(%eax),%eax # eax<- thisPtr->clazz->vtable
    EXPORT_PC                           # might throw later - get ready
    movl      (%eax,%edx,4),%eax        # eax<- vtable[BBBB]
    jmp       common_invokeMethod${routine}
