%verify "executed"
    # const/high16 vAA,                    /* +BBBB0000 */
    FETCH(a0, 1)                           #  a0 <- 0000BBBB (zero-extended)
    GET_OPA(a3)                            #  a3 <- AA
    sll       a0, a0, 16                   #  a0 <- BBBB0000
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a3, t0)              #  vAA <- a0

