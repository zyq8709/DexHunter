%verify "executed"
    /* move-exception vAA */
    movl    rSELF,%ecx
    movl    offThread_exception(%ecx),%eax # eax<- dvmGetException bypass
    SET_VREG %eax rINST                # fp[AA]<- exception object
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    movl    $$0,offThread_exception(%ecx) # dvmClearException bypass
    GOTO_NEXT_R %eax
