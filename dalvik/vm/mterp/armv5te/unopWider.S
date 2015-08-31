%default {"preinstr":""}
    /*
     * Generic 32bit-to-64bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op r0", where
     * "result" is a 64-bit quantity in r0/r1.
     *
     * For: int-to-long, int-to-double, float-to-long, float-to-double
     */
    /* unop vA, vB */
    mov     r9, rINST, lsr #8           @ r9<- A+
    mov     r3, rINST, lsr #12          @ r3<- B
    and     r9, r9, #15
    GET_VREG(r0, r3)                    @ r0<- vB
    add     r9, rFP, r9, lsl #2         @ r9<- &fp[A]
    $preinstr                           @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    $instr                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    stmia   r9, {r0-r1}                 @ vA/vA+1<- r0/r1
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 10-11 instructions */
