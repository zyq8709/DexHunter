%default {"preinstr":""}
    /*
     * Generic 64bit-to-32bit unary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = op r0/r1", where
     * "result" is a 32-bit quantity in r0.
     *
     * For: long-to-float, double-to-int, double-to-float
     *
     * (This would work for long-to-int, but that instruction is actually
     * an exact match for OP_MOVE.)
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    ubfx    r9, rINST, #8, #4           @ r9<- A
    add     r3, rFP, r3, lsl #2         @ r3<- &fp[B]
    ldmia   r3, {r0-r1}                 @ r0/r1<- vB/vB+1
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    $preinstr                           @ optional op; may set condition codes
    $instr                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */
