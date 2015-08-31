%verify "executed"
%verify "exception for null object"
    /*
     * Synchronize on an object.
     */
    /* monitor-enter vAA */
    GET_OPA(a2)                            #  a2 <- AA
    GET_VREG(a1, a2)                       #  a1 <- vAA (object)
    move      a0, rSELF                    #  a0 <- self
    EXPORT_PC()                            #  export PC so we can grab stack trace
    # null object?
    beqz      a1, common_errNullObject     #  null object, throw an exception
    FETCH_ADVANCE_INST(1)                  #  advance rPC, load rINST
    JAL(dvmLockObject)                     #  call(self, obj)
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction

