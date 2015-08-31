%default {"preinstr":"", "result":"r0", "chkzero":"0"}
    /*
     * Generic 32-bit binary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = r0 op r1".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.  Note that we
     * *don't* check for (INT_MIN / -1) here, because the ARM math lib
     * handles it correctly.
     *
     * For: add-int, sub-int, mul-int, div-int, rem-int, and-int, or-int,
     *      xor-int, shl-int, shr-int, ushr-int, add-float, sub-float,
     *      mul-float, div-float, rem-float
     */
    /* binop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    GET_VREG(r1, r3)                    @ r1<- vCC
    GET_VREG(r0, r2)                    @ r0<- vBB
    .if $chkzero
    cmp     r1, #0                      @ is second operand zero?
    beq     common_errDivideByZero
    .endif

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ $result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG($result, r9)               @ vAA<- $result
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 11-14 instructions */
