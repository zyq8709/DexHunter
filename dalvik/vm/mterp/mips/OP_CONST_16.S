%verify "executed"
    # const/16 vAA,                        /* +BBBB */
    FETCH_S(a0, 1)                         #  a0 <- ssssBBBB (sign-extended)
    GET_OPA(a3)                            #  a3 <- AA
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

