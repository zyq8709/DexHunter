%verify "executed"
%verify "null object"
    /* For: iput-wide-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $$4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    movzwl    2(rPC),%eax               # eax<- field byte offset
    testl      %ecx,%ecx                # is object null?
    je        common_errNullObject
    leal      (%ecx,%eax,1),%ecx        # ecx<- Address of 64-bit target
    andb      $$0xf,rINSTbl             # rINST<- A
    GET_VREG_WORD %eax rINST 0          # eax<- lsw
    GET_VREG_WORD rINST rINST 1         # rINST<- msw
    movl      %eax,(%ecx)
    movl      rINST,4(%ecx)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
