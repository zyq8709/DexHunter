%verify "executed"
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    GET_OPA4(t2)                           #  t2 <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a2, a3)                       #  a2 <- vB
    EAS2(rOBJ, rFP, t2)                    #  rOBJ <- &fp[A]
    LOAD64(a0, a1, rOBJ)                   #  a0/a1 <- vAA/vAA+1

    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    sll     v0, a0, a2                     #  rlo<- alo << (shift&31)
    not     v1, a2                         #  rhi<- 31-shift  (shift is 5b)
    srl     a0, 1
    srl     a0, v1                         #  alo<- alo >> (32-(shift&31))
    sll     v1, a1, a2                     #  rhi<- ahi << (shift&31)
    or      v1, a0                         #  rhi<- rhi | alo
    andi    a2, 0x20                       #  shift< shift & 0x20
    movn    v1, v0, a2                     #  rhi<- rlo (if shift&0x20)
    movn    v0, zero, a2                   #  rlo<- 0  (if shift&0x20)

    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE64(v0, v1, rOBJ)                  #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

