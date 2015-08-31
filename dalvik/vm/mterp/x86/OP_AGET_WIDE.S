%verify "executed"
    /*
     * Array get, 64 bits.  vAA <- vBB[vCC].
     *
     */
    /* op vAA, vBB, vCC */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    GET_VREG_R  %eax %eax               # eax<- vBB (array object)
    GET_VREG_R  %ecx %ecx               # ecs<- vCC (requested index)
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects
                                        #    arrayObj in eax
                                        #    index in ecx
    leal      offArrayObject_contents(%eax,%ecx,8),%eax
    movl      (%eax),%ecx
    movl      4(%eax),%eax
    SET_VREG_WORD %ecx rINST 0
    SET_VREG_WORD %eax rINST 1
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
