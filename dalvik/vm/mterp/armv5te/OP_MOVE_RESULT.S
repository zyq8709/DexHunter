%verify "executed"
    /* for: move-result, move-result-object */
    /* op vAA */
    mov     r2, rINST, lsr #8           @ r2<- AA
    FETCH_ADVANCE_INST(1)               @ advance rPC, load rINST
    ldr     r0, [rSELF, #offThread_retval]    @ r0<- self->retval.i
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    SET_VREG(r0, r2)                    @ fp[AA]<- r0
    GOTO_OPCODE(ip)                     @ jump to next instruction
