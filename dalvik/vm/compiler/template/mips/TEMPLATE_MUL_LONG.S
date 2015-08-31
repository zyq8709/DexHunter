%verify "executed"
    /*
     * Signed 64-bit integer multiply.
     *
     * For JIT: op1 in a0/a1, op2 in a2/a3, return in v0/v1
     *
     * Consider WXxYZ (a1a0 x a3a2) with a long multiply:
     *
     *         a1   a0
     *   x     a3   a2
     *   -------------
     *       a2a1 a2a0
     *       a3a0
     *  a3a1 (<= unused)
     *  ---------------
     *         v1   v0
     *
     */
    /* mul-long vAA, vBB, vCC */
    mul     rRESULT1,rARG3,rARG0              #  v1= a3a0
    multu   rARG2,rARG0
    mfhi    t1
    mflo    rRESULT0                          #  v0= a2a0
    mul     t0,rARG2,rARG1                    #  t0= a2a1
    addu    rRESULT1,rRESULT1,t1              #  v1= a3a0 + hi(a2a0)
    addu    rRESULT1,rRESULT1,t0              #  v1= a3a0 + hi(a2a0) + a2a1;
    RETURN
