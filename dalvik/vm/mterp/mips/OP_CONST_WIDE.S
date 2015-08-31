%verify "executed"
    # const-wide vAA,                      /* +HHHHhhhhBBBBbbbb */
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (low middle)
    FETCH(a2, 3)                           #  a2 <- hhhh (high middle)
    sll       a1, 16 #
    or        a0, a1                       #  a0 <- BBBBbbbb (low word)
    FETCH(a3, 4)                           #  a3 <- HHHH (high)
    GET_OPA(t1)                            #  t1 <- AA
    sll       a3, 16
    or        a1, a3, a2                   #  a1 <- HHHHhhhh (high word)
    FETCH_ADVANCE_INST(5)                  #  advance rPC, load rINST
    EAS2(t1, rFP, t1)                      #  t1 <- &fp[AA]
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(a0, a1, t1)                    #  vAA <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

