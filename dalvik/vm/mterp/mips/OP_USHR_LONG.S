%verify "executed"
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.
     */
    /* ushr-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(t0)                            #  t3 <- AA
    and       a3, a0, 255                  #  a3 <- BB
    srl       a0, a0, 8                    #  a0 <- CC
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[BB]
    GET_VREG(a2, a0)                       #  a2 <- vCC
    LOAD64(a0, a1, a3)                     #  a0/a1 <- vBB/vBB+1
    EAS2(rOBJ, rFP, t0)                    #  rOBJ <- &fp[AA]

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    srl       v1, a1, a2                   #  rhi<- ahi >> (shift&31)
    srl       v0, a0, a2                   #  rlo<- alo >> (shift&31)
    not       a0, a2                       #  alo<- 31-n  (shift is 5b)
    sll       a1, 1
    sll       a1, a0                       #  ahi<- ahi << (32-(shift&31))
    or        v0, a1                       #  rlo<- rlo | ahi
    andi      a2, 0x20                     #  shift & 0x20
    movn      v0, v1, a2                   #  rlo<- rhi (if shift&0x20)
    movn      v1, zero, a2                 #  rhi<- 0 (if shift&0x20)

    STORE64(v0, v1, rOBJ)                  #  vAA/vAA+1 <- v0/v1
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

