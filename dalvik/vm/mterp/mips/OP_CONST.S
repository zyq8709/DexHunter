%verify "executed"
    # const vAA,                           /* +BBBBbbbb */
    GET_OPA(a3)                            #  a3 <- AA
    FETCH(a0, 1)                           #  a0 <- bbbb (low)
    FETCH(a1, 2)                           #  a1 <- BBBB (high)
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    sll       a1, a1, 16
    or        a0, a1, a0                   #  a0 <- BBBBbbbb
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

