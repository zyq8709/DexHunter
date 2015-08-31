%verify "executed"
    /* mul vA, vB */
    movzx   rINSTbl,%ecx              # ecx<- A+
    sarl    $$4,rINST                 # rINST<- B
    GET_VREG_R %eax rINST             # eax<- vB
    andb    $$0xf,%cl                 # ecx<- A
    SPILL(rIBASE)
    imull   (rFP,%ecx,4),%eax         # trashes rIBASE/edx
    UNSPILL(rIBASE)
    SET_VREG %eax %ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
