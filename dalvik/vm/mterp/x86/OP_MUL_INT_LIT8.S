%verify "executed"
    /* mul/lit8 vAA, vBB, #+CC */
    movzbl    2(rPC),%eax              # eax<- BB
    movsbl    3(rPC),%ecx              # ecx<- ssssssCC
    GET_VREG_R   %eax %eax             # eax<- rBB
    SPILL(rIBASE)
    imull     %ecx,%eax                # trashes rIBASE/edx
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG  %eax rINST
    GOTO_NEXT_R %ecx
