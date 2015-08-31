%verify "executed"
%verify "null object"
    /* For: iget-wide-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $$4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    movzwl    2(rPC),%eax               # eax<- field byte offset
    cmpl      $$0,%ecx                  # is object null?
    je        common_errNullObject
    leal      (%ecx,%eax,1),%eax        # eax<- address of 64-bit source
    movl      (%eax),%ecx               # ecx<- lsw
    movl      4(%eax),%eax              # eax<- msw
    andb      $$0xf,rINSTbl             # rINST<- A
    SET_VREG_WORD %ecx rINST 0          # v[A+0]<- lsw
    FETCH_INST_OPCODE 2 %ecx
    SET_VREG_WORD %eax rINST 1          # v[A+1]<- msw
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
