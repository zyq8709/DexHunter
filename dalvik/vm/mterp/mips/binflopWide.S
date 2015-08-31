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
     * IMPORTANT: you may specify "chkzero" or "preinstr" but not both.
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  s5 <- AA
    and       a2, a0, 255                  #  a2 <- BB
    srl       a3, a0, 8                    #  a3 <- CC
    EAS2(rOBJ, rFP, rOBJ)                  #  s5 <- &fp[AA]
    EAS2(a2, rFP, a2)                      #  a2 <- &fp[BB]
    EAS2(t1, rFP, a3)                      #  a3 <- &fp[CC]
#ifdef SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)               #  a0/a1 <- vBB/vBB+1
    LOAD64(rARG2, rARG3, t1)               #  a2/a3 <- vCC/vCC+1
    .if $chkzero
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, a2)
    LOAD64_F(fa1, fa1f, t1)
    .if $chkzero
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
#ifdef SOFT_FLOAT
    $instr                                 #  result <- op, a0-a3 changed
    STORE64(rRESULT0, rRESULT1, rOBJ)
#else
    $instr_f
    STORE64_F(fv0, fv0f, rOBJ)
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 14-17 instructions */

