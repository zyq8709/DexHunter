%default {"pre0":"","pre1":""}
    /*
     * Generic 32-bit unary operation.  Provide an "instr" line that
     * specifies an instruction that performs "result = op eax".
     */
    /* unop vA, vB */
    movzbl   rINSTbl,%ecx           # ecx<- A+
    sarl     $$4,rINST             # rINST<- B
    GET_VREG_R %eax rINST           # eax<- vB
    andb     $$0xf,%cl              # ecx<- A
    $pre0
    $pre1
    $instr
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
