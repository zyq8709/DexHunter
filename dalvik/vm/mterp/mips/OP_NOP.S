%verify "executed"
    FETCH_ADVANCE_INST(1)                  #  advance to next instr, load rINST
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)                        #  execute it

#ifdef ASSIST_DEBUGGER
    /* insert fake function header to help gdb find the stack frame */
    .type dalvik_inst, @function
dalvik_inst:
    .ent dalvik_inst
    .end dalvik_inst
#endif

