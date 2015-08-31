%verify "executed"
%verify "negative array length"
%verify "allocation fails"
    /*
     * Allocate an array of objects, specified with the array class
     * and a count.
     *
     * The verifier guarantees that this is an array class, so we don't
     * check for it here.
     */
    /* new-array vA, vB, class@CCCC */
    movl    rSELF,%ecx
    EXPORT_PC
    movl    offThread_methodClassDex(%ecx),%ecx # ecx<- pDvmDex
    movzwl  2(rPC),%eax                       # eax<- CCCC
    movl    offDvmDex_pResClasses(%ecx),%ecx  # ecx<- pDvmDex->pResClasses
    SPILL(rIBASE)
    movl    (%ecx,%eax,4),%ecx                # ecx<- resolved class
    movzbl  rINSTbl,%eax
    sarl    $$4,%eax                          # eax<- B
    GET_VREG_R %eax %eax                      # eax<- vB (array length)
    andb    $$0xf,rINSTbl                     # rINST<- A
    testl   %eax,%eax
    js      common_errNegativeArraySize       # bail, passing len in eax
    testl   %ecx,%ecx                         # already resolved?
    jne     .L${opcode}_finish                # yes, fast path
    /*
     * Resolve class.  (This is an uncommon case.)
     *  ecx holds class (null here)
     *  eax holds array length (vB)
     */
    movl    rSELF,%ecx
    SPILL_TMP1(%eax)                   # save array length
    movl    offThread_method(%ecx),%ecx  # ecx<- self->method
    movzwl  2(rPC),%eax                # eax<- CCCC
    movl    offMethod_clazz(%ecx),%ecx # ecx<- method->clazz
    movl    %eax,OUT_ARG1(%esp)
    movl    $$0,OUT_ARG2(%esp)
    movl    %ecx,OUT_ARG0(%esp)
    call    dvmResolveClass            # eax<- call(clazz,ref,flag)
    movl    %eax,%ecx
    UNSPILL_TMP1(%eax)
    testl   %ecx,%ecx                  # successful resolution?
    je      common_exceptionThrown     # no, bail.
# fall through to ${opcode}_finish

    /*
     * Finish allocation
     *
     * ecx holds class
     * eax holds array length (vB)
     */
.L${opcode}_finish:
    movl    %ecx,OUT_ARG0(%esp)
    movl    %eax,OUT_ARG1(%esp)
    movl    $$ALLOC_DONT_TRACK,OUT_ARG2(%esp)
    call    dvmAllocArrayByClass    # eax<- call(clazz,length,flags)
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    testl   %eax,%eax               # failed?
    je      common_exceptionThrown  # yup - go handle
    SET_VREG %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
