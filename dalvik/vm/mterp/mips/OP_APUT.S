%default { "store":"sw", "shift":"2" }
%verify "executed"
    /*
     * Array put, 32 bits or less.  vBB[vCC] <- vAA.
     * for: aput, aput-boolean, aput-byte, aput-char, aput-short
     */
    /* op vAA, vBB, vCC */
    FETCH_B(a2, 1)                         #  a2 <- BB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    FETCH_C(a3, 1)                         #  a3 <- CC
    GET_VREG(a0, a2)                       #  a0 <- vBB (array object)
    GET_VREG(a1, a3)                       #  a1 <- vCC (requested index)
    # null array object?
    beqz      a0, common_errNullObject     #  yes, bail
    LOAD_base_offArrayObject_length(a3, a0) #  a3 <- arrayObj->length
    .if $shift
    EASN(a0, a0, a1, $shift)               #  a0 <- arrayObj + index*width
    .else
    addu      a0, a0, a1
    .endif
    bgeu      a1, a3, common_errArrayIndex #  index >= length, bail
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_VREG(a2, rOBJ)                     #  a2 <- vAA
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    $store a2, offArrayObject_contents(a0) #  vBB[vCC] <- a2
    GOTO_OPCODE(t0)                        #  jump to next instruction

