%verify "executed"
    /* const/4 vA, #+B */
    movsx   rINSTbl,%eax              # eax<-ssssssBx
    movl    $$0xf,rINST
    andl    %eax,rINST                # rINST<- A
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    sarl    $$4,%eax
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx
