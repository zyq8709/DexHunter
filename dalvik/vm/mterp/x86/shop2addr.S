%default {"result":"%eax"}
    /*
     * Generic 32-bit "shift/2addr" operation.
     */
    /* shift/2addr vA, vB */
    movzx    rINSTbl,%ecx           # eax<- BA
    sarl     $$4,%ecx               # ecx<- B
    GET_VREG_R %ecx %ecx            # eax<- vBB
    andb     $$0xf,rINSTbl          # rINST<- A
    GET_VREG_R %eax rINST           # eax<- vAA
    $instr                          # ex: sarl %cl,%eax
    FETCH_INST_OPCODE 1 %ecx
    SET_VREG $result rINST
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
