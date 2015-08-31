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
    mov     r2, rINST, lsr #8           @ r2<- AA
    EXPORT_PC()                         @ before fetch: export the PC
    GET_VREG(r1, r2)                    @ r1<- vAA (object)
    cmp     r1, #0                      @ null object?
    beq     1f                          @ yes
    mov     r0, rSELF                   @ r0<- self
    bl      dvmUnlockObject             @ r0<- success for unlock(self, obj)
    cmp     r0, #0                      @ failed?
    FETCH_ADVANCE_INST(1)               @ before throw: advance rPC, load rINST
    beq     common_exceptionThrown      @ yes, exception is pending
    GET_INST_OPCODE(ip)                 @ extract opcode from rINST
    GOTO_OPCODE(ip)                     @ jump to next instruction
1:
    FETCH_ADVANCE_INST(1)               @ advance before throw
    b      common_errNullObject
