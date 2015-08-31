%default {"preinstr":"", "chkzero":"0"}
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" and
     * "instr_f" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-float/2addr, sub-float/2addr, mul-float/2addr,
     * div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  t1 <- A+
    GET_OPB(a3)                            #  a3 <- B
#ifdef SOFT_FLOAT
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
#else
    GET_VREG_F(fa0, rOBJ)
    GET_VREG_F(fa1, a3)
    .if $chkzero
    # is second operand zero?
    li.s      ft0, 0
    c.eq.s    fcc0, ft0, fa1
    bc1t      fcc0, common_errDivideByZero
    .endif
#endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
#ifdef SOFT_FLOAT
    $instr                                 #  result <- op, a0-a3 changed
    SET_VREG(v0, rOBJ)                     #  vAA <- result
#else
    $instr_f
    SET_VREG_F(fv0, rOBJ)                  #  vAA <- result
#endif
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
    /* 10-13 instructions */

