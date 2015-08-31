%verify "executed"
    /* const/high16 vAA, #+BBBB0000 */
    movzwl     2(rPC),%eax                # eax<- 0000BBBB
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    sall       $$16,%eax                  # eax<- BBBB0000
    SET_VREG %eax rINST                   # vAA<- eax
    GOTO_NEXT_R %ecx
