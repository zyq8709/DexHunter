%verify "executed"
    /* const vAA, #+BBBBbbbb */
    movl      2(rPC),%eax             # grab all 32 bits at once
    movl      rINST,rINST             # rINST<- AA
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    SET_VREG %eax rINST               # vAA<- eax
    GOTO_NEXT_R %ecx
