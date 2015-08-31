%verify "executed"
    # const-wide/32 vAA,                   /* +BBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- 0000bbbb (low)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_S(a2, 2)                         #  a2 <- ssssBBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    sll       a2, a2, 16
    or        a0, a0, a2                   #  a0 <- BBBBbbbb
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[AA]
    sra       a1, a0, 31                   #  a1 <- ssssssss
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

