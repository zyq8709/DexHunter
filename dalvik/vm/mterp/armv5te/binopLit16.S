%default {"result":"r0", "chkzero":"0"}
    /*
     * Generic 32-bit "lit16" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-int/lit16, rsub-int, mul-int/lit16, div-int/lit16,
     *      rem-int/lit16, and-int/lit16, or-int/lit16, xor-int/lit16
     */
    /* binop/lit16 vA, vB, #+CCCC */
    FETCH_S(r1, 1)                      @ r1<- ssssCCCC (sign-extended)
    mov     r2, rINST, lsr #12          @ r2<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r2)                    @ r0<- vB
    and     r9, r9, #15
    .if $chkzero
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST

    $instr                              @ $result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG($result, r9)               @ vAA<- $result
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-13 instructions */
