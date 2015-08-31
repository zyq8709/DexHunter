%default {"preinstr":"", "chkzero":"0"}
    /*
     * Generic 64-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * for: add-long, sub-long, div-long, rem-long, and-long, or-long,
     *      xor-long, add-double, sub-double, mul-double, div-double,
     *      rem-double
     *
     * On entry:
     *     a0 = target dalvik register address
     *     a1 = op1 address
     *     a2 = op2 address
     *
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    move rOBJ, a0                       # save a0
#ifdef  SOFT_FLOAT
    move t0, a1                         # save a1
    move t1, a2                         # save a2
    LOAD64(rARG0, rARG1, t0)            # a0/a1<- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)            # a2/a3<- vCC/vCC+1
    .if $chkzero
    or          t0, rARG2, rARG3        # second arg (a2-a3) is zero?
    beqz        t0, common_errDivideByZero
    .endif
    $preinstr                           # optional op
    $instr                              # result<- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    LOAD64_F(fa0, fa0f, a1)
    LOAD64_F(fa1, fa1f, a2)
    .if $chkzero
    li.d        ft0, 0
    c.eq.d      fcc0, fa1, ft0
    bc1t        fcc0, common_errDivideByZero
    .endif
    $preinstr                           # optional op
    $instr_f
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    RETURN
