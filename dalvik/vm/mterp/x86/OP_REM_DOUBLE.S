%verify "executed"
    /* rem_float vAA, vBB, vCC */
    movzbl   3(rPC),%ecx            # ecx<- BB
    movzbl   2(rPC),%eax            # eax<- CC
    fldl     (rFP,%ecx,4)           # vCC to fp stack
    fldl     (rFP,%eax,4)           # vCC to fp stack
    FETCH_INST_OPCODE 2 %ecx
1:
    fprem
    fstsw     %ax
    sahf
    jp        1b
    fstp      %st(1)
    ADVANCE_PC 2
    fstpl    (rFP,rINST,4)           # %st to vAA
    GOTO_NEXT_R %ecx
