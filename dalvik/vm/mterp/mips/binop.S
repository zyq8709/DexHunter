%default {"preinstr":"", "result":"a0", "chkzero":"0"}
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = a0 op a1".
     * This could be a MIPS instruction or a function call.  (If the result
     * comes back in a register other than a0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (a1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int
     */
    /* binop vAA, vBB, vCC */
    FETCH(a0, 1)                           #  a0 <- CCBB
    GET_OPA(rOBJ)                          #  rOBJ <- AA
    srl       a3, a0, 8                    #  a3 <- CC
    and       a2, a0, 255                  #  a2 <- BB
    GET_VREG(a1, a3)                       #  a1 <- vCC
    GET_VREG(a0, a2)                       #  a0 <- vBB
    .if $chkzero
    # is second operand zero?
    beqz      a1, common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)                  #  advance rPC, load rINST
    $preinstr                              #  optional op
    $instr                                 #  $result <- op, a0-a3 changed
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO($result, rOBJ, t0)       #  vAA <- $result
    /* 11-14 instructions */

