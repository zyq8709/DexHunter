%verify "executed"
    /* const-wide/16 vAA, #+BBBB */
    movswl    2(rPC),%eax               # eax<- ssssBBBB
    SPILL(rIBASE)                       # preserve rIBASE (cltd trashes it)
    cltd                                # rIBASE:eax<- ssssssssssssBBBB
    SET_VREG_WORD rIBASE rINST 1        # store msw
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)                     # restore rIBASE
    SET_VREG_WORD %eax rINST 0          # store lsw
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
