%verify "executed"
    /*
     * See comments in OP_MUL_LONG.S
     */
    /* mul-long/2addr vA, vB */
    GET_OPA4(t0)                           #  t0 <- A+

    EAS2(t0, rFP, t0)                      #  t0 <- &fp[A]
    LOAD64(a0, a1, t0)                     #  vAA.low / high

    GET_OPB(t1)                            #  t1 <- B
    EAS2(t1, rFP, t1)                      #  t1 <- &fp[B]
    LOAD64(a2, a3, t1)                     #  vBB.low / high

    mul       v1, a3, a0                   #  v1= a3a0
    multu     a2, a0
    mfhi      t1
    mflo      v0                           #  v0= a2a0
    mul       t2, a2, a1                   #  t2= a2a1
    addu      v1, v1, t1                   #  v1= a3a0 + hi(a2a0)
    addu      v1, v1, t2                   #  v1= v1 + a2a1;

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t1)                    #  extract opcode from rINST
    # vAA <- v0 (low)
    STORE64(v0, v1, t0)                    #  vAA+1 <- v1 (high)
    GOTO_OPCODE(t1)                        #  jump to next instruction

