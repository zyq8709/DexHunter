%verify "executed"
    # const-wide/high16 vAA,               /* +BBBB000000000000 */
    FETCH(a1, 1)                           #  a1 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    li        a0, 0                        #  a0 <- 00000000
    sll       a1, 16                       #  a1 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, a3)                    #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

