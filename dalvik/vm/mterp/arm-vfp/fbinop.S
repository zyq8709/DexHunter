    /*
     * Generic 32-bit floating-point operation.  Provide an "instr" line that
     * specifies an instruction that performs "s2 = s0 op s1".  Because we
     * use the "softfp" ABI, this must be an instruction, not a function call.
     *
     * For: add-float, sub-float, mul-float, div-float
     */
    /* floatop vAA, vBB, vCC */
    FETCH(r0, 1)                        @ r0<- CCBB
    mov     r9, rINST, lsr #8           @ r9<- AA
    mov     r3, r0, lsr #8              @ r3<- CC
    and     r2, r0, #255                @ r2<- BB
    VREG_INDEX_TO_ADDR(r3, r3)          @ r3<- &vCC
    VREG_INDEX_TO_ADDR(r2, r2)          @ r2<- &vBB
    flds    s1, [r3]                    @ s1<- vCC
    flds    s0, [r2]                    @ s0<- vBB

    FETCH_ADVANCE_INST(2)               @ advance rPC, load rINST
    $instr                              @ s2<- op
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    VREG_INDEX_TO_ADDR(r9, r9)          @ r9<- &vAA
    fsts    s2, [r9]                    @ vAA<- s2
    GOTO_OPCODE(ip)                     @ jump to next instruction
