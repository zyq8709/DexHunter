%verify "executed"
%verify "basic lt, gt, eq"
%verify "hi equal, lo <=>"
%verify "lo equal, hi <=>"
    /*
     * Compare two 64-bit values
     *    x = y     return  0
     *    x < y     return -1
     *    x > y     return  1
     *
     * I think I can improve on the ARM code by the following observation
     *    slt   t0,  x.hi, y.hi;	# (x.hi < y.hi) ? 1:0
     *    sgt   t1,  x.hi, y.hi;	# (y.hi > x.hi) ? 1:0
     *    subu  v0, t0, t1              # v0= -1:1:0 for [ < > = ]
     */
    /* cmp-long vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(a3, rFP, a3)                      #  a3 <- &fp[CC]
    LOAD64(a0, a1, a2)                     #  a0/a1 <- vBB/vBB+1
    LOAD64(a2, a3, a3)                     #  a2/a3 <- vCC/vCC+1

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    slt       t0, a1, a3                   #  compare hi
    sgt       t1, a1, a3
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0)
    bnez      v0, .L${opcode}_finish
    # at this point x.hi==y.hi
    sltu      t0, a0, a2                   #  compare lo
    sgtu      t1, a0, a2
    subu      v0, t1, t0                   #  v0 <- (-1, 1, 0) for [< > =]

.L${opcode}_finish:
    SET_VREG(v0, rOBJ)                     #  vAA <- v0
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

