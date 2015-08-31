%verify "executed"
    /*
     * Long integer shift, 2addr version.  vA is 64-bit value/result, vB is
     * 32-bit shift distance.
     */
    /* shl-long/2addr vA, vB */
    /* ecx gets shift count */
    /* Need to spill rIBASE */
    /* rINSTw gets AA */
    movzbl    rINSTbl,%ecx             # ecx<- BA
    andb      $$0xf,rINSTbl            # rINST<- A
    GET_VREG_WORD %eax rINST 0         # eax<- v[AA+0]
    sarl      $$4,%ecx                 # ecx<- B
    SPILL(rIBASE)
    GET_VREG_WORD rIBASE rINST 1       # rIBASE<- v[AA+1]
    GET_VREG_R  %ecx %ecx              # ecx<- vBB
    shldl     %eax,rIBASE
    sall      %cl,%eax
    testb     $$32,%cl
    je        2f
    movl      %eax,rIBASE
    xorl      %eax,%eax
2:
    SET_VREG_WORD rIBASE rINST 1       # v[AA+1]<- rIBASE
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 1 %ecx
    SET_VREG_WORD %eax rINST 0         # v[AA+0]<- eax
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
