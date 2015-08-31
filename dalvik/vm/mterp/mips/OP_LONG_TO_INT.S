%verify "executed"
    GET_OPB(a1)                            #  a1 <- B from 15:12
    GET_OPA4(a0)                           #  a0 <- A from 11:8
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
#ifdef HAVE_BIG_ENDIAN
    addu      a1, a1, 1
#endif
    GET_VREG(a2, a1)                       #  a2 <- fp[B]
    GET_INST_OPCODE(t0)                    #  t0 <- opcode from rINST
    SET_VREG_GOTO(a2, a0, t0)              #  fp[A] <- a2
