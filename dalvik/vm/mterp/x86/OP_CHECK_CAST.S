%verify "executed"
%verify "null object"
%verify "class cast exception thrown, with correct class name"
%verify "class cast exception not thrown on same class"
%verify "class cast exception not thrown on subclass"
%verify "class not resolved"
%verify "class already resolved"
    /*
     * Check to see if a cast from one class to another is allowed.
     */
    /* check-cast vAA, class@BBBB */
    movl      rSELF,%ecx
    GET_VREG_R  rINST,rINST             # rINST<- vAA (object)
    movzwl    2(rPC),%eax               # eax<- BBBB
    movl      offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    testl     rINST,rINST               # is oject null?
    movl      offDvmDex_pResClasses(%ecx),%ecx # ecx<- pDvmDex->pResClasses
    je        .L${opcode}_okay          # null obj, cast always succeeds
    movl      (%ecx,%eax,4),%eax        # eax<- resolved class
    movl      offObject_clazz(rINST),%ecx # ecx<- obj->clazz
    testl     %eax,%eax                 # have we resolved this before?
    je        .L${opcode}_resolve       # no, go do it now
.L${opcode}_resolved:
    cmpl      %eax,%ecx                 # same class (trivial success)?
    jne       .L${opcode}_fullcheck     # no, do full check
.L${opcode}_okay:
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

    /*
     * Trivial test failed, need to perform full check.  This is common.
     *  ecx holds obj->clazz
     *  eax holds class resolved from BBBB
     *  rINST holds object
     */
.L${opcode}_fullcheck:
    movl    %eax,sReg0                 # we'll need the desired class on failure
    movl    %eax,OUT_ARG1(%esp)
    movl    %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call    dvmInstanceofNonTrivial    # eax<- boolean result
    UNSPILL(rIBASE)
    testl   %eax,%eax                  # failed?
    jne     .L${opcode}_okay           # no, success

    # A cast has failed.  We need to throw a ClassCastException.
    EXPORT_PC
    movl    offObject_clazz(rINST),%eax
    movl    %eax,OUT_ARG0(%esp)                 # arg0<- obj->clazz
    movl    sReg0,%ecx
    movl    %ecx,OUT_ARG1(%esp)                 # arg1<- desired class
    call    dvmThrowClassCastException
    jmp     common_exceptionThrown

    /*
     * Resolution required.  This is the least-likely path, and we're
     * going to have to recreate some data.
     *
     *  rINST holds object
     */
.L${opcode}_resolve:
    movl    rSELF,%ecx
    EXPORT_PC
    movzwl  2(rPC),%eax                # eax<- BBBB
    movl    offThread_method(%ecx),%ecx  # ecx<- self->method
    movl    %eax,OUT_ARG1(%esp)        # arg1<- BBBB
    movl    offMethod_clazz(%ecx),%ecx # ecx<- metho->clazz
    movl    $$0,OUT_ARG2(%esp)         # arg2<- false
    movl    %ecx,OUT_ARG0(%esp)        # arg0<- method->clazz
    SPILL(rIBASE)
    call    dvmResolveClass            # eax<- resolved ClassObject ptr
    UNSPILL(rIBASE)
    testl   %eax,%eax                  # got null?
    je      common_exceptionThrown     # yes, handle exception
    movl    offObject_clazz(rINST),%ecx  # ecx<- obj->clazz
    jmp     .L${opcode}_resolved       # pick up where we left off
