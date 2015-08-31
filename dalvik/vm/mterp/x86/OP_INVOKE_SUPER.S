%default { "isrange":"0", "routine":"NoRange" }
%verify "executed"
%verify "unknown method"
    /*
     * Handle a "super" method call.
     *
     * for: invoke-super, invoke-super/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op vAA, {vCCCC..v(CCCC+AA-1)}, meth@BBBB */
    movl      rSELF,rINST
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(rINST),%ecx # ecx<- pDvmDex
    EXPORT_PC
    movl      offDvmDex_pResMethods(%ecx),%ecx # ecx<- pDvmDex->pResMethods
    movl      (%ecx,%eax,4),%ecx        # ecx<- resolved baseMethod
    movl      offThread_method(rINST),%eax # eax<- method
    movzwl    4(rPC),rINST              # rINST<- GFED or CCCC
    .if       (!$isrange)
    andl      $$0xf,rINST               # rINST<- D (or stays CCCC)
    .endif
    GET_VREG_R  %edx rINST             # %edx<- "this" ptr
    testl     %edx,%edx                # null "this"?
    SPILL_TMP1(%edx)
    je        common_errNullObject      # yes, throw
    movl      offMethod_clazz(%eax),%eax # eax<- method->clazz
    testl     %ecx,%ecx                 # already resolved?
    je       .L${opcode}_resolve
    /*
     * At this point:
     *  ecx = resolved base method [r0]
     *  eax = method->clazz [r9]
     */
.L${opcode}_continue:
    movl    offClassObject_super(%eax),%eax   # eax<- method->clazz->super
    movzwl  offMethod_methodIndex(%ecx),%edx  # edx<- baseMthod->methodIndex
    cmpl    offClassObject_vtableCount(%eax),%edx # compare(methodIndex,vtableCount)
    jae     .L${opcode}_nsm           # method not present in superclass
    movl    offClassObject_vtable(%eax),%eax   # eax<- ...clazz->super->vtable
    movl    (%eax,%edx,4),%eax        # eax<- vtable[methodIndex]
    UNSPILL_TMP1(%edx)
    movl    %edx, %ecx
    jmp     common_invokeMethod${routine}


    /* At this point:
     * ecx = null (needs to be resolved base method)
     * eax = method->clazz
    */
.L${opcode}_resolve:
    SPILL_TMP2(%eax)                    # method->clazz
    movl    %eax,OUT_ARG0(%esp)         # arg0<- method->clazz
    movzwl  2(rPC),%ecx                 # ecx<- BBBB
    movl    $$METHOD_VIRTUAL,OUT_ARG2(%esp)  # arg2<- resolver method type
    movl    %ecx,OUT_ARG1(%esp)         # arg1<- ref
    call    dvmResolveMethod            # eax<- call(clazz, ref, flags)
    testl   %eax,%eax                   # got null?
    movl    %eax,%ecx                   # ecx<- resolved base method
    UNSPILL_TMP2(%eax)                  # restore method->clazz
    jne     .L${opcode}_continue        # good to go - continue
    jmp     common_exceptionThrown      # handle exception

    /*
     * Throw a NoSuchMethodError with the method name as the message.
     *  ecx = resolved base method
     */
.L${opcode}_nsm:
    movl    offMethod_name(%ecx),%eax
    jmp     common_errNoSuchMethod
