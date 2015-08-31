%verify "executed"
    /* const/16 vAA, #+BBBB */
    movswl  2(rPC),%ecx                # ecx<- ssssBBBB
    FETCH_INST_OPCODE 2 %eax
    ADVANCE_PC 2
    SET_VREG %ecx rINST                # vAA<- ssssBBBB
    GOTO_NEXT_R %eax
