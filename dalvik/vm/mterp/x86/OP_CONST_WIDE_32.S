%verify "executed"
    /* const-wide/32 vAA, #+BBBBbbbb */
    movl     2(rPC),%eax                # eax<- BBBBbbbb
    SPILL(rIBASE)                       # save rIBASE (cltd trashes it)
    cltd                                # rIBASE:eax<- ssssssssssssBBBB
    SET_VREG_WORD rIBASE rINST,1        # store msw
    FETCH_INST_OPCODE 3 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 0          # store lsw
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx
