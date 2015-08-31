%default {"result":"%eax"}
    /*
     * Generic 32-bit "/2addr" binary operation.  Provide an "instr" line
     * that specifies an instruction that performs "result = r0 op r1".
     * This could be an instruction or a function call.
     *
     * For: add-int/2addr, sub-int/2addr, mul-int/2addr, div-int/2addr,
     *      rem-int/2addr, and-int/2addr, or-int/2addr, xor-int/2addr,
     *      shl-int/2addr, shr-int/2addr, ushr-int/2addr, add-float/2addr,
     *      sub-float/2addr, mul-float/2addr, div-float/2addr, rem-float/2addr
     */
    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx               # ecx<- A+
    sarl    $$4,rINST                  # rINST<- B
    GET_VREG_R %eax rINST              # eax<- vB
    andb    $$0xf,%cl                  # ecx<- A
    $instr                             # for ex: addl   %eax,(rFP,%ecx,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
