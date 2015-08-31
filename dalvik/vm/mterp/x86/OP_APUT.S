%default { "reg":"rINST", "store":"movl", "shift":"4" }
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
    testl     %eax,%eax                 # null array object?
    je        common_errNullObject      # bail if so
    cmpl      offArrayObject_length(%eax),%ecx
    jae       common_errArrayIndex      # index >= length, bail.  Expects:
                                        #   arrayObj in eax
                                        #   index in ecx
    leal      offArrayObject_contents(%eax,%ecx,$shift),%eax
.L${opcode}_finish:
    GET_VREG_R  rINST rINST
    FETCH_INST_OPCODE 2 %ecx
    $store     $reg,(%eax)
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
