%verify "executed"
    /* move-result-wide vAA */
    movl    rSELF,%ecx
    movl    offThread_retval(%ecx),%eax
    movl    4+offThread_retval(%ecx),%ecx
    SET_VREG_WORD %eax rINST 0     # v[AA+0] <- eax
    SET_VREG_WORD %ecx rINST 1     # v[AA+1] <- ecx
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
