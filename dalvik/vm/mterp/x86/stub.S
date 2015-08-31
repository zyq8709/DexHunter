    /* (stub) */
    SAVE_PC_FP_TO_SELF %ecx          # leaves rSELF in %ecx
    movl %ecx,OUT_ARG0(%esp)         # self is first arg to function
    call      dvmMterp_${opcode}     # do the real work
    movl      rSELF,%ecx
    LOAD_PC_FP_FROM_SELF             # retrieve updated values
    movl      offThread_curHandlerTable(%ecx),rIBASE  # set up rIBASE
    FETCH_INST
    GOTO_NEXT
