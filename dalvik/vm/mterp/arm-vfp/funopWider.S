    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "d0 = op s0".
     *
     * For: int-to-double, float-to-double
     */
    /* unop vA, vB */
    mov     r3, rINST, lsr #12          @ r3<- B
    mov     r9, rINST, lsr #8           @ r9<- A+
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vB
    flds    s0, [r3]                    @ s0<- vB
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    and     r9, r9, #15                 @ r9<- A
    $instr                              @ d0<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vA
    fstd    d0, [r9]                    @ vA<- d0
    GOTO_OPCODE(ip)                     @ jump to next instruction
