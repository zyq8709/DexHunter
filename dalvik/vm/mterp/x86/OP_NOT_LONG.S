%verify "executed"
    /* unop vA, vB */
    movzbl    rINSTbl,%ecx       # ecx<- BA
    sarl      $$4,%ecx           # ecx<- B
    andb      $$0xf,rINSTbl      # rINST<- A
    GET_VREG_WORD %eax %ecx 0    # eax<- v[B+0]
    GET_VREG_WORD %ecx %ecx 1    # ecx<- v[B+1]
    notl      %eax
    notl      %ecx
    SET_VREG_WORD %eax rINST 0   # v[A+0]<- eax
    FETCH_INST_OPCODE 1 %eax
    SET_VREG_WORD %ecx rINST 1   # v[A+1]<- ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %eax
