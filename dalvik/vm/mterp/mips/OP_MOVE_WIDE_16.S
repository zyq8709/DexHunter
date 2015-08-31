%verify "executed"
    /* move-wide/16 vAAAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6, v7" or "move v7, v6" */
    FETCH(a3, 2)                           #  a3 <- BBBB
    FETCH(a2, 1)                           #  a2 <- AAAA
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BBBB]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[AAAA]
    LOAD64(a0, a1, a3)                     #  a0/a1 <- fp[BBBB]
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a2)                    #  fp[AAAA] <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

