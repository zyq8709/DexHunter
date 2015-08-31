%verify "executed"
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* shl-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t2)                            #  t2 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1

    EAS2(t2, rFP, t2)                      #  t2 <- &fp[AA]
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

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
    STORE64(v0, v1, t2)                    #  vAA/vAA+1 <- a0/a1
    GOTO_OPCODE(t0)                        #  jump to next instruction

