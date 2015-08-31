%verify "executed"
%verify "null object"
    # iget-wide-quick vA, vB, offset       /* CCCC */
    GET_OPB(a2)                            #  a2 <- B
    GET_VREG(a3, a2)                       #  a3 <- object we're operating on
    FETCH(a1, 1)                           #  a1 <- field byte offset
    GET_OPA4(a2)                           #  a2 <- A(+)
    # check object for null
    beqz      a3, common_errNullObject     #  object was null
    addu      t0, a3, a1                   #  t0 <- a3 + a1
    LOAD64(a0, a1, t0)                     #  a0 <- obj.field (64 bits, aligned)
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a2)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  fp[A] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

