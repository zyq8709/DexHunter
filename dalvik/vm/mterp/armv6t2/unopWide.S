%default {"preinstr":""}
    /*
     * Generic 64-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0/r1".
     * This could be an ARM instruction or a function call.
     *
     * For: neg-long, not-long, neg-double, long-to-double, double-to-long
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    ldmia   r3, {r0-r1}                 @ r0/r1<- vAA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ r0/r1<- op, r2-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vAA<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-11 instructions */
