%verify "executed"
    /* move-wide/from16 vAA, vBBBB */
    /* NOTE: regs can overlap, e.g. "move v6,v7" or "move v7,v6" */
    movzwl    2(rPC),%ecx              # ecx<- BBBB
    movzbl    rINSTbl,%eax             # eax<- AAAA
    GET_VREG_WORD rINST %ecx 0         # rINST<- v[BBBB+0]
    GET_VREG_WORD %ecx %ecx 1          # ecx<- v[BBBB+1]
    SET_VREG_WORD rINST %eax 0         # v[AAAA+0]<- rINST
    SET_VREG_WORD %ecx %eax 1          # v[AAAA+1]<- eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
