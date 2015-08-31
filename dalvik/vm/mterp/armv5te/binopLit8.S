%default {"preinstr":"", "result":"r0", "chkzero":"0"}
    /*
     * Generic 32-bit "lit8" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit8, rsub-int/lit8, mul-int/lit8, div-int/lit8,
     *      rem-int/lit8, and-int/lit8, or-int/lit8, xor-int/lit8,
     *      shl-int/lit8, shr-int/lit8, ushr-int/lit8
     */
    /* binop/lit8 vAA, vBB, #+CC */
    FETCH_S(r3, 1)                      @ r3<- ssssCCBB (sign-extended for CC)
    mov     r9, rINST, lsr #8           @ r9<- AA
    and     r2, r3, #255                @ r2<- BB
    GET_VREG(r0, r2)                    @ r0<- vBB
    movs    r1, r3, asr #8              @ r1<- ssssssCC (sign extended)
    .if $chkzero
    @cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    $preinstr                           @ optional op; may set condition codes
    $instr                              @ $result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG($result, r9)               @ vAA<- $result
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-12 instructions */
