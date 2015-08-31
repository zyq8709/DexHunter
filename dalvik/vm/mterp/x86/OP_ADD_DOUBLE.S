   /*
    * File: OP_ADD_DOUBLE.S
    */

    movzbl   2(rPC),%eax                # eax<- BB
    movzbl   3(rPC),%ecx                # ecx<- CC
    movq     (rFP, %eax, 4), %xmm0      # %xmm0<- vBB
    movq     (rFP, %ecx, 4), %xmm1      # %xmm1<- vCC
    FETCH_INST_OPCODE 2 %ecx
    addsd    %xmm1, %xmm0
    ADVANCE_PC 2
    movq     %xmm0, (rFP, rINST, 4)     # vAA<- vBB * vCC
    GOTO_NEXT_R %ecx

