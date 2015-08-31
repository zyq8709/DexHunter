%verify "executed"
%verify "null object"
    /* For: iget-quick, iget-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $$4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    movzwl    2(rPC),%eax               # eax<- field byte offset
    cmpl      $$0,%ecx                  # is object null?
    je        common_errNullObject
    movl      (%ecx,%eax,1),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    andb      $$0xf,rINSTbl             # rINST<- A
    SET_VREG  %eax rINST                # fp[A]<- result
    GOTO_NEXT_R %ecx
