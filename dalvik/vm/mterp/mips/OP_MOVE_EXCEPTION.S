%verify "executed"
    /* move-exception vAA */
    GET_OPA(a2)                            #  a2 <- AA
    LOAD_offThread_exception(a3, rSELF)    #  a3 <- dvmGetException bypass
    li        a1, 0                        #  a1 <- 0
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    SET_VREG(a3, a2)                       #  fp[AA] <- exception obj
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    STORE_offThread_exception(a1, rSELF)   #  dvmClearException bypass
    GOTO_OPCODE(t0)                        #  jump to next instruction

