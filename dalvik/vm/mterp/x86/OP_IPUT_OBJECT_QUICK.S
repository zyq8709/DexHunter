%verify "executed"
%verify "null object"
    /* For: iput-object-quick */
    /* op vA, vB, offset@CCCC */
    movzbl    rINSTbl,%ecx              # ecx<- BA
    sarl      $$4,%ecx                  # ecx<- B
    GET_VREG_R  %ecx %ecx               # vB (object we're operating on)
    andb      $$0xf,rINSTbl             # rINST<- A
    GET_VREG_R  rINST rINST             # rINST<- v[A]
    movzwl    2(rPC),%eax               # eax<- field byte offset
    testl     %ecx,%ecx                 # is object null?
    je        common_errNullObject
    movl      rINST,(%ecx,%eax,1)
    movl      rSELF,%eax
    testl     rINST,rINST               # did we store null?
    movl      offThread_cardTable(%eax),%eax  # get card table base
    je        1f                            # skip card mark if null store
    shrl      $$GC_CARD_SHIFT,%ecx          # object head to card number
    movb      %al,(%eax,%ecx)               # mark card based on object head
1:
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
