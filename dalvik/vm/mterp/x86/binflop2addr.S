    /*
     * Generic 32-bit binary float operation.
     *
     * For: add-fp, sub-fp, mul-fp, div-fp
     */

    /* binop/2addr vA, vB */
    movzx   rINSTbl,%ecx           # ecx<- A+
    andb    $$0xf,%cl              # ecx<- A
    $load    (rFP,%ecx,4)          # vAA to fp stack
    sarl    $$4,rINST             # rINST<- B
    $instr   (rFP,rINST,4)         # ex: faddp
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    $store    (rFP,%ecx,4)         # %st to vA
    GOTO_NEXT_R %eax
