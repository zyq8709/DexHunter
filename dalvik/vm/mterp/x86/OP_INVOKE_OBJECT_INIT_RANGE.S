%default { "cccc":"2" }
%verify "executed"
%verify "finalizable class"
    /*
     * Invoke Object.<init> on an object.  In practice we know that
     * Object's nullary constructor doesn't do anything, so we just
     * skip it unless a debugger is active.
     */
    movzwl     4(rPC),%eax              # eax<- CCCC, offset = 2 * cccc
    GET_VREG_R %ecx, %eax               # ecx<- "this" ptr
    testl      %ecx,%ecx                # null this?
    je         common_errNullObject     # yes, fail
    movl       offObject_clazz(%ecx), %eax # eax<- obj->clazz
    SPILL_TMP1(rIBASE)                  # save %edx
    movl       offClassObject_accessFlags(%eax), %edx # edx<- clazz->accessFlags
    andl       $$CLASS_ISFINALIZABLE, %edx # is this class finalizable?
    jnz        .L${opcode}_setFinal     # yes, go
.L${opcode}_finish:
    movl       rSELF, %ecx
    movl       offThread_subMode(%ecx), %eax
    andl       $$kSubModeDebuggerActive, %eax # debugger active?
    jnz        .L${opcode}_debugger     # Yes - skip optimization
    UNSPILL_TMP1(rIBASE)
    FETCH_INST_OPCODE 3 %ecx            # 3 = cccc + 1
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx
%break

.L${opcode}_setFinal:
    EXPORT_PC                           # can throw
    movl       %ecx, OUT_ARG0(%esp)     # arg1<- obj
    call       dvmSetFinalizable        # call dvmSetFinalizable(obj)
    movl       rSELF, %ecx
    movl       offThread_exception(%ecx), %eax # eax<- self->exception
    cmpl       $$0, %eax                # exception pending?
    jne        common_exceptionThrown   # yes, handle it
    jmp        .L${opcode}_finish

    /*
     * A debugger is attached, so we need to go ahead and do
     * this.  For simplicity, we'll just jump directly to the
     * corresponding handler.  Note that we can't use
     * rIBASE here because it may be in single-step mode.
     * Load the primary table base directly.
     */
.L${opcode}_debugger:
    movl    offThread_mainHandlerTable(%ecx), %ecx # load main handler table
    movl       $$OP_INVOKE_DIRECT_RANGE, %eax
    /*
     * We can't use GOTO_NEXT here since we want to jump directly to
     * handler without touching rIBASE.
     */
    jmp        *(%ecx,%eax,4)
