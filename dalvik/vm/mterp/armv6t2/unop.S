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
    ubfx    r9, rINST, #8, #4           @ r9<- A
    GET_VREG(r0, r3)                    @ r0<- vB
    $preinstr                           @ optional op; may set condition codes
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    $instr                              @ r0<- op, r0-r3 changed
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r9)                    @ vAA<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
    /* 8-9 instructions */
