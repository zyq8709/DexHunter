%default {"result":"","special":""}
    /*
     * 32-bit binary div/rem operation.  Handles special case of op0=minint and
     * op1=-1.
     */
    /* div/rem/lit16 vA, vB, #+CCCC */
    /* Need A in rINST, ssssCCCC in ecx, vB in eax */
    movzbl   rINSTbl,%eax         # eax<- 000000BA
    SPILL(rIBASE)
    sarl     $$4,%eax             # eax<- B
    GET_VREG_R %eax %eax          # eax<- vB
    movswl   2(rPC),%ecx          # ecx<- ssssCCCC
    andb     $$0xf,rINSTbl        # rINST<- A
    cmpl     $$0,%ecx
    je       common_errDivideByZero
    cmpl     $$-1,%ecx
    jne      .L${opcode}_continue_div
    cmpl     $$0x80000000,%eax
    jne      .L${opcode}_continue_div
    movl     $special,$result
    SET_VREG $result rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx

.L${opcode}_continue_div:
    cltd
    idivl   %ecx
    SET_VREG $result rINST
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
