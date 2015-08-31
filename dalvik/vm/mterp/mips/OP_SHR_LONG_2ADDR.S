%verify "executed"
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shr-long/2addr vA, vB */
    GET_OPA4(t2)                           #  t2 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(t2, rFP, t2)                      #  t2 <- &fp[A]
    LOAD64(a0, a1, t2)                     #  a0/a1 <- vAA/vAA+1
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    sra     v1, a1, a2                     #  rhi<- ahi >> (shift&31)
    srl     v0, a0, a2                     #  rlo<- alo >> (shift&31)
    sra     a3, a1, 31                     #  a3<- sign(ah)
    not     a0, a2                         #  alo<- 31-shift (shift is 5b)
    sll     a1, 1
    sll     a1, a0                         #  ahi<- ahi << (32-(shift&31))
    or      v0, a1                         #  rlo<- rlo | ahi
    andi    a2, 0x20                       #  shift & 0x20
    movn    v0, v1, a2                     #  rlo<- rhi (if shift&0x20)
    movn    v1, a3, a2                     #  rhi<- sign(ahi) (if shift&0x20)

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, t2)                    #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

