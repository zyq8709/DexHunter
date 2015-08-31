%default {"preinstr":"", "result0":"r0", "result1":"r1", "chkzero":"0"}
    /*
     * Generic 64-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0-r1 op r2-r3".
     * This could be an ARM instruction or a function call.  (If the result
     * comes back in a register other than r0, you can override "result".)
     *
     * If "chkzero" is set to 1, we perform a divide-by-zero check on
     * vCC (r1).  Useful for integer division and modulus.
     *
     * For: add-long/2addr, sub-long/2addr, div-long/2addr, rem-long/2addr,
     *      and-long/2addr, or-long/2addr, xor-long/2addr, add-double/2addr,
     *      sub-double/2addr, mul-double/2addr, div-double/2addr,
     *      rem-double/2addr
     */
    /* binop/2addr vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r1, rINST, lsr #12          @ r1<- B
    and     r9, r9, #15
    add     r1, rFP, r1, lsl #2         @ r1<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r1, {r2-r3}                 @ r2/r3<- vBB/vBB+1
    ldmia   r9, {r0-r1}                 @ r0/r1<- vAA/vAA+1
    .if $chkzero
    orrs    ip, r2, r3                  @ second arg (r2-r3) is zero?
    beq     common_errDivideByZero
    .endif
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST

    $preinstr                           @ optional op; may set condition codes
    $instr                              @ result<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {$result0,$result1}     @ vAA/vAA+1<- $result0/$result1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 12-15 instructions */
