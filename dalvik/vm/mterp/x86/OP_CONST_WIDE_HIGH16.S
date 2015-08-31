%verify "executed"
    /* const-wide/high16 vAA, #+BBBB000000000000 */
    movzwl     2(rPC),%eax                # eax<- 0000BBBB
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    sall       $$16,%eax                  # eax<- BBBB0000
    SET_VREG_WORD %eax rINST 1            # v[AA+1]<- eax
    xorl       %eax,%eax
    SET_VREG_WORD %eax rINST 0            # v[AA+0]<- eax
    GOTO_NEXT_R %ecx
