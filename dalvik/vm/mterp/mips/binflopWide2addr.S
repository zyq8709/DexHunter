%default {"preinstr":"", "chkzero":"0"}
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0-a1 op a2-a3".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-double/2addr, sub-double/2addr, mul-double/2addr,
     *  div-double/2addr, rem-double/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a1)                            #  a1 <- B
    EAS2(a1, rFP, a1)                      #  a1 <- &fp[B]
    EAS2(rOBJ, rFP, rOBJ)                  #  rOBJ <- &fp[A]
#ifdef SOFT_FLOAT
    LOAD64(rARG2, rARG3, a1)               #  a2/a3 <- vBB/vBB+1
    LOAD64(rARG0, rARG1, rOBJ)             #  a0/a1 <- vAA/vAA+1
    .if $chkzero
    or        t0, rARG2, rARG3             #  second arg (a2-a3) is zero?
    beqz      t0, common_errDivideByZero
    .endif
#else
    LOAD64_F(fa0, fa0f, rOBJ)
    LOAD64_F(fa1, fa1f, a1)
    .if $chkzero
    li.d      ft0, 0
    c.eq.d    fcc0, fa1, ft0
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
1:
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
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
    /* 12-15 instructions */

