%verify "executed"
    /* move-wide vA, vB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    GET_OPA4(a2)                           #  a2 <- A(+)
    GET_OPB(a3)                            #  a3 <- B
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[B]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[A]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[B]
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a2)                    #  fp[A] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

