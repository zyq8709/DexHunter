%default {"result":"a0", "chkzero":"0"}
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = a0 op a1".
     * This could be an MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    # binop/lit16 vA, vB,                  /* +CCCC */
    FETCH_S(a1, 1)                         #  a1 <- ssssCCCC (sign-extended)
    GET_OPB(a2)                            #  a2 <- B
    GET_OPA(rOBJ)                          #  rOBJ <- A+
    GET_VREG(a0, a2)                       #  a0 <- vB
    and       rOBJ, rOBJ, 15
    .if $chkzero
    # cmp a1, 0; is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST

    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vAA <- $result
    /* 10-13 instructions */

