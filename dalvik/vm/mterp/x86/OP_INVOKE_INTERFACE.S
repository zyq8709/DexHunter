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
    movzwl     4(rPC),%eax              # eax<- FEDC or CCCC
    movl       rSELF,%ecx
    .if        (!$isrange)
    andl       $$0xf,%eax               # eax<- C (or stays CCCC)
    .endif
    GET_VREG_R   %eax %eax              # eax<- "this"
    EXPORT_PC
    testl      %eax,%eax                # null this?
    je         common_errNullObject     # yes, fail
    movl       %eax, TMP_SPILL1(%ebp)
    movl       offObject_clazz(%eax),%eax# eax<- thisPtr->clazz
    movl       %eax,OUT_ARG0(%esp)                 # arg0<- class
    movl       offThread_methodClassDex(%ecx),%eax   # eax<- methodClassDex
    movl       offThread_method(%ecx),%ecx           # ecx<- method
    movl       %eax,OUT_ARG3(%esp)                 # arg3<- dex
    movzwl     2(rPC),%eax                         # eax<- BBBB
    movl       %ecx,OUT_ARG2(%esp)                 # arg2<- method
    movl       %eax,OUT_ARG1(%esp)                 # arg1<- BBBB
    call       dvmFindInterfaceMethodInCache # eax<- call(class, ref, method, dex)
    testl      %eax,%eax
    je         common_exceptionThrown
    movl       TMP_SPILL1(%ebp), %ecx
    jmp        common_invokeMethod${routine}
