%verify "executed"
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA
     *
     * for: aput, aput-object, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    GET_VREG_R  rINST rINST             # rINST<- vAA
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    /* On entry:
     *   eax<- array object
     *   ecx<- index
     *   rINST<- vAA
     */
    leal      offArrayObject_contents(%eax,%ecx,4),%ecx
    testl     rINST,rINST                    # storing null reference?
    je        .L${opcode}_skip_check
    SPILL_TMP1(%ecx)                         # save target address
    SPILL_TMP2(%eax)                         # save object head
    movl      offObject_clazz(%eax),%eax     # eax<- arrayObj->clazz
    movl      offObject_clazz(rINST),%ecx    # ecx<- obj->clazz
    movl      %eax,OUT_ARG1(%esp)
    movl      %ecx,OUT_ARG0(%esp)
    movl      %ecx,sReg0                     # store the two classes for later
    movl      %eax,sReg1
    SPILL(rIBASE)
    call      dvmCanPutArrayElement          # test object type vs. array type
    UNSPILL(rIBASE)
    UNSPILL_TMP1(%ecx)                       # recover target address
    testl     %eax,%eax
    movl      rSELF,%eax
    jne       .L${opcode}_types_okay

    # The types don't match.  We need to throw an ArrayStoreException.
    EXPORT_PC
    movl      sReg0,%eax                     # restore the two classes...
    movl      %eax,OUT_ARG0(%esp)
    movl      sReg1,%ecx
    movl      %ecx,OUT_ARG1(%esp)
    call      dvmThrowArrayStoreExceptionIncompatibleElement # ...and throw
    jmp       common_exceptionThrown

.L${opcode}_types_okay:
    movl      offThread_cardTable(%eax),%eax   # get card table base
    movl      rINST,(%ecx)                   # store into array
    UNSPILL_TMP2(rINST)                      # recover object head
    FETCH_INST_OPCODE 2 %ecx
    shrl      $$GC_CARD_SHIFT,rINST          # object head to card number
    movb      %al,(%eax,rINST)               # mark card using object head
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.L${opcode}_skip_check:
    movl      rINST,(%ecx)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
