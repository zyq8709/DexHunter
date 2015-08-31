    /*
     * Generic 64-bit binary operation.
     */
    /* binop/2addr vA, vB */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $$4,%ecx                  # ecx<- B
    GET_VREG_WORD %eax %ecx 0           # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1           # eax<- v[B+1]
    andb      $$0xF,rINSTbl             # rINST<- A
    $instr1         # example: addl   %eax,(rFP,rINST,4)
    $instr2         # example: adcl   %ecx,4(rFP,rINST,4)
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
