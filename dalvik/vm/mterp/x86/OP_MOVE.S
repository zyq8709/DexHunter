%verify "executed"
    /* for move, move-object, long-to-int */
    /* op vA, vB */
    movzbl rINSTbl,%eax          # eax<- BA
    andb   $$0xf,%al             # eax<- A
    shrl   $$4,rINST            # rINST<- B
    GET_VREG_R rINST rINST
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    SET_VREG rINST %eax           # fp[A]<-fp[B]
    GOTO_NEXT_R %ecx
