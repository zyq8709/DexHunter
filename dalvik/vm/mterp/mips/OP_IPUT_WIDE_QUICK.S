%verify "executed"
%verify "null object"
    # iput-wide-quick vA, vB, offset       /* CCCC */
    GET_OPA4(a0)                           #  a0 <- A(+)
    GET_OPB(a1)                            #  a1 <- B
    GET_VREG(a2, a1)                       #  a2 <- fp[B], the object pointer
    EAS2(a3, rFP, a0)                      #  a3 <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[A]
    # check object for null
    beqz      a2, common_errNullObject     #  object was null
    FETCH(a3, 1)                           #  a3 <- field byte offset
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    addu      a2, a2, a3                   #  obj.field (64 bits, aligned) <- a0/a1
    STORE64(a0, a1, a2)                    #  obj.field (64 bits, aligned) <- a0/a1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

