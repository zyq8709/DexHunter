    /* (stub) */
    SAVE_PC_TO_SELF()            # only need to export PC and FP
    SAVE_FP_TO_SELF()
    move        a0, rSELF        # self is first arg to function
    JAL(dvmMterp_${opcode})      # call
    LOAD_PC_FROM_SELF()          # retrieve updated values
    LOAD_FP_FROM_SELF()
    FETCH_INST()                 # load next instruction from rPC
    GET_INST_OPCODE(t0)          # ...trim down to just the opcode
    GOTO_OPCODE(t0)              # ...and jump to the handler
