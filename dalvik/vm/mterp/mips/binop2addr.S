%default {"preinstr":"", "result":"a0", "chkzero":"0"}
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr
     */
    /* binop/2addr vA, vB */
    GET_OPA4(rOBJ)                         #  rOBJ <- A+
    GET_OPB(a3)                            #  a3 <- B
    GET_VREG(a0, rOBJ)                     #  a0 <- vA
    GET_VREG(a1, a3)                       #  a1 <- vB
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vAA <- $result
    /* 10-13 instructions */

