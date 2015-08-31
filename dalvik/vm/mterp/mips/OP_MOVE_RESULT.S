%verify "executed"
    /* for: move-result, move-result-object */
    /* op vAA */
    GET_OPA(a2)                            #  a2 <- AA
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    LOAD_rSELF_retval(a0)                  #  a0 <- self->retval.i
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    SET_VREG_GOTO(a0, a2, t0)              #  fp[AA] <- a0

