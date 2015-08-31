   /*
    * File: OP_SUB_DOUBLE_2ADDR.S
    */

    movzx       rINSTbl,%ecx            # ecx<- A+
    andb        $$0xf,%cl               # ecx<- A
    sarl        $$4,rINST               # rINST<- B
    # TODO: movsd?
    movq        (rFP, rINST, 4), %xmm1  # %xmm1<- vB
    movq        (rFP, %ecx, 4), %xmm0   # %xmm0<- vA
    FETCH_INST_OPCODE 1 %eax
    subsd       %xmm1, %xmm0            # %xmm0<- vA op vB
    ADVANCE_PC 1
    movq        %xmm0, (rFP, %ecx, 4)   # vA<- %xmm0; result
    GOTO_NEXT_R %eax
