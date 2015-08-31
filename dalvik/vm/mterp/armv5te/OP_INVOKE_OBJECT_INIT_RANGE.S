%default { "cccc":"2" }
%verify "executed"
%verify "finalizable class"
    /*
     * Invoke Object.<init> on an object.  In practice we know that
     * Object's nullary constructor doesn't do anything, so we just
     * skip it unless a debugger is active.
     */
    FETCH(r1, ${cccc})                  @ r1<- CCCC
    GET_VREG(r0, r1)                    @ r0<- "this" ptr
    cmp     r0, #0                      @ check for NULL
    beq     common_errNullObject        @ export PC and throw NPE
    ldr     r1, [r0, #offObject_clazz]  @ r1<- obj->clazz
    ldr     r2, [r1, #offClassObject_accessFlags] @ r2<- clazz->accessFlags
    tst     r2, #CLASS_ISFINALIZABLE    @ is this class finalizable?
    bne     .L${opcode}_setFinal        @ yes, go
.L${opcode}_finish:
    ldrh    r1, [rSELF, #offThread_subMode]
    ands    r1, #kSubModeDebuggerActive @ debugger active?
    bne     .L${opcode}_debugger        @ Yes - skip optimization
    FETCH_ADVANCE_INST(${cccc}+1)       @ advance to next instr, load rINST
    GET_INST_OPCODE(ip)                 @ ip<- opcode from rINST
    GOTO_OPCODE(ip)                     @ execute it
%break

.L${opcode}_setFinal:
    EXPORT_PC()                         @ can throw
    bl      dvmSetFinalizable           @ call dvmSetFinalizable(obj)
    ldr     r0, [rSELF, #offThread_exception] @ r0<- self->exception
    cmp     r0, #0                      @ exception pending?
    bne     common_exceptionThrown      @ yes, handle it
    b       .L${opcode}_finish

    /*
     * A debugger is attached, so we need to go ahead and do
     * this.  For simplicity, we'll just jump directly to the
     * corresponding handler.  Note that we can't use
     * rIBASE here because it may be in single-step mode.
     * Load the primary table base directly.
     */
.L${opcode}_debugger:
    ldr     r1, [rSELF, #offThread_mainHandlerTable]
    mov     ip, #OP_INVOKE_DIRECT_RANGE
    GOTO_OPCODE_BASE(r1,ip)             @ execute it
