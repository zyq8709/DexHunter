%verify "executed"
    /*
     * 32-bit binary multiplication.
     */
    /* mul vAA, vBB, vCC */
    movzbl   2(rPC),%eax            # eax<- BB
    movzbl   3(rPC),%ecx            # ecx<- CC
    GET_VREG_R %eax %eax            # eax<- vBB
    SPILL(rIBASE)
    imull    (rFP,%ecx,4),%eax      # trashes rIBASE/edx
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx
