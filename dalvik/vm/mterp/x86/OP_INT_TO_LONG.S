%verify "executed"
    /* int to long vA, vB */
    movzbl  rINSTbl,%eax                # eax<- +A
    sarl    $$4,%eax                    # eax<- B
    GET_VREG_R %eax %eax                # eax<- vB
    andb    $$0xf,rINSTbl               # rINST<- A
    SPILL(rIBASE)                       # cltd trashes rIBASE/edx
    cltd                                # rINST:eax<- sssssssBBBBBBBB
    SET_VREG_WORD rIBASE rINST 1        # v[A+1]<- rIBASE/rPC
    FETCH_INST_OPCODE 1 %ecx
    UNSPILL(rIBASE)
    SET_VREG_WORD %eax rINST 0          # v[A+0]<- %eax
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
