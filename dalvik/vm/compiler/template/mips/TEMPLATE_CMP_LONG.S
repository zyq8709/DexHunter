%verify "endianess"
    /*
     * Compare two 64-bit values
     *    x = y     return  0
     *    x < y     return -1
     *    x > y     return  1
     *
     * I think I can improve on the ARM code by the following observation
     *    slt   t0,  x.hi, y.hi;        # (x.hi < y.hi) ? 1:0
     *    sgt   t1,  x.hi, y.hi;        # (y.hi > x.hi) ? 1:0
     *    subu  v0, t0, t1              # v0= -1:1:0 for [ < > = ]
     *
     * This code assumes the register pair ordering will depend on endianess (a1:a0 or a0:a1).
     *    a1:a0 => vBB
     *    a3:a2 => vCC
     */
    /* cmp-long vAA, vBB, vCC */
    slt    t0, rARG1, rARG3             # compare hi
    sgt    t1, rARG1, rARG3
    subu   v0, t1, t0                   # v0<- (-1,1,0)
    bnez   v0, .L${opcode}_finish
                                        # at this point x.hi==y.hi
    sltu   t0, rARG0, rARG2             # compare lo
    sgtu   t1, rARG0, rARG2
    subu   v0, t1, t0                   # v0<- (-1,1,0) for [< > =]
.L${opcode}_finish:
    RETURN
