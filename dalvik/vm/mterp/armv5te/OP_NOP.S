%verify "executed"
    FETCH_ADVANCE_INST(1)               @ advance to next instr, load rINST
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    GOTO_OPCODE(ip)                     @ execute it

#ifdef ASSIST_DEBUGGER
    /* insert fake function header to help gdb find the stack frame */
    .type   dalvik_inst, %function
dalvik_inst:
    .fnstart
    MTERP_ENTRY1
    MTERP_ENTRY2
    .fnend
#endif
