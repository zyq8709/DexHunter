%verify "executed"
    # const/4 vA,                          /* +B */
    sll       a1, rINST, 16                #  a1 <- Bxxx0000
    GET_OPA(a0)                            #  a0 <- A+
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    sra       a1, a1, 28                   #  a1 <- sssssssB (sign-extended)
    and       a0, a0, 15
    GET_INST_OPCODE(t0)                    #  ip <- opcode from rINST
    SET_VREG_GOTO(a1, a0, t0)              #  fp[A] <- a1

