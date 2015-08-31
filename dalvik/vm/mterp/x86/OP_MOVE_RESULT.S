%verify "executed"
    /* for: move-result, move-result-object */
    /* op vAA */
    movl     rSELF,%eax                    # eax<- rSELF
    movl     offThread_retval(%eax),%eax   # eax<- self->retval.l
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    SET_VREG  %eax rINST                   # fp[AA]<- retval.l
    GOTO_NEXT_R %ecx
