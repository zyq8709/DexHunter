    /*
     * Generic 64bit-to-32bit unary floating point operation.  Provide an
     * "instr" line that specifies an instruction that performs "s0 = op d0".
     *
     * For: double-to-int, double-to-float
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    fldd    d0, [r3]                    @ d0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    $instr                              @ s0<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fsts    s0, [r9]                    @ vA<- s0
    GOTO_OPCODE(ip)                     @ jump to next instruction
