%default {"preinstr":""}
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op r0".
     * This could be an ARM instruction or a function call.
     *
     * for: neg-int, not-int, neg-float, int-to-float, float-to-int,
     *      int-to-byte, int-to-char, int-to-short
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    GET_VREG(r0, r3)                    @ r0<- vB
    and     r9, r9, #15
    $preinstr                           @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    $instr                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 9-10 instructions */
