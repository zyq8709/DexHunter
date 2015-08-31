%default { "load":"movl", "shift":"4" }
%verify "executed"
    /*
     * Array get, 32 bits or less.  vAA <- vBB[vCC].
     *
     * for: aget, aget-object, aget-boolean, aget-byte, aget-char, aget-short
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
    $load     offArrayObject_contents(%eax,%ecx,$shift),%eax
.L${opcode}_finish:
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG  %eax rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
