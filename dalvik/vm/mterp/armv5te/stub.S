    /* (stub) */
    SAVE_PC_FP_TO_SELF()            @ only need to export these two
    mov     r0, rSELF               @ self is first arg to function
    bl      dvmMterp_${opcode}      @ call
    LOAD_PC_FP_FROM_SELF()          @ retrieve updated values
    FETCH_INST()                    @ load next instruction from rPC
    GET_INST_OPCODE(ip)             @ ...trim down to just the opcode
    GOTO_OPCODE(ip)                 @ ...and jump to the handler
