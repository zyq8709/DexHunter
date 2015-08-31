%verify "executed"
%verify "exception for null object (impossible in javac)"
%verify "dvmUnlockObject fails"
    /*
     * Unlock an object.
     *
     * Exceptions that occur when unlocking a monitor need to appear as
     * if they happened at the following instruction.  See the Dalvik
     * instruction spec.
     */
    /* monitor-exit vAA */
    GET_OPA(a2)                            #  a2 <- AA
    EXPORT_PC()                            #  before fetch: export the PC
    GET_VREG(a1, a2)                       #  a1 <- vAA (object)
    # null object?
    beqz      a1, 1f
    move      a0, rSELF                    #  a0 <- self
    JAL(dvmUnlockObject)                   #  v0 <- success for unlock(self, obj)
    # failed?
    FETCH_ADVANCE_INST(1)                  #  before throw: advance rPC, load rINST
    beqz      v0, common_exceptionThrown   #  yes, exception is pending
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
1:
    FETCH_ADVANCE_INST(1)                  #  before throw: advance rPC, load rINST
    b         common_errNullObject
