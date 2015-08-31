%verify "executed"
%verify "null object"
    /* For: iget-quick, iget-object-quick */
    # op vA, vB, offset                    /* CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1 #
    lw        a0, 0(t0)                    #  a0 <- obj.field (always 32 bits)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG(a0, a2)                       #  fp[A] <- a0
    GOTO_OPCODE(t0)                        #  jump to next instruction

