%verify "executed"
    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to mask off the low
     * 6 bits of the shift distance.  x86 shifts automatically mask off
     * the low 5 bits of %cl, so have to handle the 64 > shiftcount > 31
     * case specially.
     */
    /* shr-long vAA, vBB, vCC */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl    2(rPC),%eax               # eax<- BB
    movzbl    3(rPC),%ecx               # ecx<- CC
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE %eax 1         # rIBASE<- v[BB+1]
    GET_VREG_R  %ecx %ecx               # ecx<- vCC
    GET_VREG_WORD %eax %eax 0           # eax<- v[BB+0]
    shrdl     rIBASE,%eax
    shrl      %cl,rIBASE
    testb     $$32,%cl
    je        2f
    movl      rIBASE,%eax
    xorl      rIBASE,rIBASE
2:
    SET_VREG_WORD rIBASE rINST 1          # v[AA+1]<- rIBASE
    FETCH_INST_OPCODE 2 %ecx
    UNSPILL(rIBASE)
    SET_VREG_WORD %eax rINST 0         # v[BB+0]<- eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
