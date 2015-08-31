%verify "executed"
%verify "null object"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * 64-bit instance field get.
     *
     */
    /* op vA, vB, field@CCCC */
    movl    rSELF,%ecx
    SPILL(rIBASE)                               # preserve rIBASE
    movzwl  2(rPC),rIBASE                       # rIBASE<- 0000CCCC
    movl    offThread_methodClassDex(%ecx),%eax # eax<- DvmDex
    movzbl  rINSTbl,%ecx                        # ecx<- BA
    sarl    $$4,%ecx                            # ecx<- B
    movl    offDvmDex_pResFields(%eax),%eax     # eax<- pDvmDex->pResFields
    andb    $$0xf,rINSTbl                       # rINST<- A
    GET_VREG_R %ecx %ecx                        # ecx<- fp[B], the object ptr
    movl    (%eax,rIBASE,4),%eax                # resolved entry
    testl   %eax,%eax                           # is resolved entry null?
    jne     .L${opcode}_finish                  # no, already resolved
    movl    rIBASE,OUT_ARG1(%esp)               # for dvmResolveInstField
    movl    rSELF,rIBASE
    EXPORT_PC
    movl    offThread_method(rIBASE),rIBASE     # rIBASE<- current method
    movl    offMethod_clazz(rIBASE),rIBASE      # rIBASE<- method->clazz
    SPILL_TMP1(%ecx)                            # save objpointer across call
    movl    rPC,OUT_ARG0(%esp)                  # pass in method->clazz
    call    dvmResolveInstField                 #  ... to dvmResolveInstField
    UNSPILL_TMP1(%ecx)
    testl   %eax,%eax                           # returns InstrField ptr
    jne     .L${opcode}_finish
    jmp     common_exceptionThrown

.L${opcode}_finish:
    /*
     * Currently:
     *   eax holds resolved field
     *   ecx holds object
     *   rINST holds A
     */
    movl    offInstField_byteOffset(%eax),%eax  # eax<- byte offset of field
    testl   %ecx,%ecx                           # object null?
    je      common_errNullObject                # object was null
    leal    (%ecx,%eax,1),%eax                  # eax<- address of field
    movl    (%eax),%ecx                         # ecx<- lsw
    movl    4(%eax),%eax                        # eax<- msw
    SET_VREG_WORD %ecx rINST 0
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                             # restore rIBASE
    SET_VREG_WORD %eax rINST 1
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
