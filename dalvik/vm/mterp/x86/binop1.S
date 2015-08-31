%default {"result":"%eax","tmp":"%ecx"}
    /*
     * Generic 32-bit binary operation in which both operands loaded to
     * registers (op0 in eax, op1 in ecx).
     */
    /* binop vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    GET_VREG_R %ecx %ecx            # eax<- vBB
    $instr                          # ex: addl    %ecx,%eax
    SET_VREG $result rINST
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
