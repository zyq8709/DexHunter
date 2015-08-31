%default {"preinstr":"", "result":"a0", "chkzero":"0"}
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    # binop/lit8 vAA, vBB,                 /* +CC */
    FETCH_S(a3, 1)                         #  a3 <- ssssCCBB (sign-extended for CC)
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    and       a2, a3, 255                  #  a2 <- BB
    GET_VREG(a0, a2)                       #  a0 <- vBB
    sra       a1, a3, 8                    #  a1 <- ssssssCC (sign extended)
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    $preinstr                              #  optional op
    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vAA <- $result
    /* 10-12 instructions */

