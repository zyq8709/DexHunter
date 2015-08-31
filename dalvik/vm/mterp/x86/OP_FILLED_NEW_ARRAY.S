%default { "isrange":"0" }
%verify "executed"
%verify "unimplemented array type"
    /*
     * Create a new array with elements filled from registers.
     *
     * for: filled-new-array, filled-new-array/range
     */
    /* op vB, {vD, vE, vF, vG, vA}, class@CCCC */
    /* op {vCCCC..v(CCCC+AA-1)}, type@BBBB */
    movl    rSELF,%eax
    movl    offThread_methodClassDex(%eax),%eax # eax<- pDvmDex
    movzwl  2(rPC),%ecx                       # ecx<- BBBB
    movl    offDvmDex_pResClasses(%eax),%eax  # eax<- pDvmDex->pResClasses
    SPILL(rIBASE)                             # preserve rIBASE
    movl    (%eax,%ecx,4),%eax                # eax<- resolved class
    EXPORT_PC
    testl   %eax,%eax                         # already resolved?
    jne     .L${opcode}_continue              # yes, continue
    # less frequent path, so we'll redo some work
    movl    rSELF,%eax
    movl    $$0,OUT_ARG2(%esp)                # arg2<- false
    movl    %ecx,OUT_ARG1(%esp)               # arg1<- BBBB
    movl    offThread_method(%eax),%eax         # eax<- self->method
    movl    offMethod_clazz(%eax),%eax        # eax<- method->clazz
    movl    %eax,OUT_ARG0(%esp)               # arg0<- clazz
    call    dvmResolveClass                   # eax<- call(clazz,ref,flag)
    testl   %eax,%eax                         # null?
    je      common_exceptionThrown            # yes, handle it

       # note: fall through to .L${opcode}_continue

    /*
     * On entry:
     *    eax holds array class [r0]
     *    rINST holds AA or BB [r10]
     *    ecx is scratch
     */
.L${opcode}_continue:
    movl    offClassObject_descriptor(%eax),%ecx  # ecx<- arrayClass->descriptor
    movl    $$ALLOC_DONT_TRACK,OUT_ARG2(%esp)     # arg2<- flags
    movzbl  1(%ecx),%ecx                          # ecx<- descriptor[1]
    movl    %eax,OUT_ARG0(%esp)                   # arg0<- arrayClass
    movl    rSELF,%eax
    cmpb    $$'I',%cl                             # supported?
    je      1f
    cmpb    $$'L',%cl
    je      1f
    cmpb    $$'[',%cl
    jne      .L${opcode}_notimpl                  # no, not handled yet
1:
    movl    %ecx,offThread_retval+4(%eax)           # save type
    .if      (!$isrange)
    SPILL_TMP1(rINST)                              # save copy, need "B" later
    sarl    $$4,rINST
    .endif
    movl    rINST,OUT_ARG1(%esp)                  # arg1<- A or AA (length)
    call    dvmAllocArrayByClass     # eax<- call(arrayClass, length, flags)
    movl    rSELF,%ecx
    testl   %eax,%eax                             # alloc successful?
    je      common_exceptionThrown                # no, handle exception
    movl    %eax,offThread_retval(%ecx)             # retval.l<- new array
    movzwl  4(rPC),%ecx                           # ecx<- FEDC or CCCC
    leal    offArrayObject_contents(%eax),%eax    # eax<- newArray->contents

/* at this point:
 *     eax is pointer to tgt
 *     rINST is length
 *     ecx is FEDC or CCCC
 *     TMP_SPILL1 is BA
 *  We now need to copy values from registers into the array
 */

    .if $isrange
    # set up src pointer
    SPILL_TMP2(%esi)
    SPILL_TMP3(%edi)
    leal    (rFP,%ecx,4),%esi # set up src ptr
    movl    %eax,%edi         # set up dst ptr
    movl    rINST,%ecx        # load count register
    rep
    movsd
    UNSPILL_TMP2(%esi)
    UNSPILL_TMP3(%edi)
    movl    rSELF,%ecx
    movl    offThread_retval+4(%ecx),%eax      # eax<- type
    .else
    testl  rINST,rINST
    je     4f
    UNSPILL_TMP1(rIBASE)      # restore "BA"
    andl   $$0x0f,rIBASE      # rIBASE<- 0000000A
    sall   $$16,rIBASE        # rIBASE<- 000A0000
    orl    %ecx,rIBASE        # rIBASE<- 000AFEDC
3:
    movl   $$0xf,%ecx
    andl   rIBASE,%ecx        # ecx<- next reg to load
    GET_VREG_R %ecx %ecx
    shrl   $$4,rIBASE
    leal   4(%eax),%eax
    movl   %ecx,-4(%eax)
    sub    $$1,rINST
    jne    3b
4:
    movl   rSELF,%ecx
    movl    offThread_retval+4(%ecx),%eax      # eax<- type
    .endif

    cmpb    $$'I',%al                        # Int array?
    je      5f                               # skip card mark if so
    movl    offThread_retval(%ecx),%eax        # eax<- object head
    movl    offThread_cardTable(%ecx),%ecx     # card table base
    shrl    $$GC_CARD_SHIFT,%eax             # convert to card num
    movb    %cl,(%ecx,%eax)                  # mark card based on object head
5:
    UNSPILL(rIBASE)                          # restore rIBASE
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx


    /*
     * Throw an exception indicating that we have not implemented this
     * mode of filled-new-array.
     */
.L${opcode}_notimpl:
    movl    $$.LstrFilledNewArrayNotImplA,%eax
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowInternalError
    jmp     common_exceptionThrown
