%default { "cccc":"2" }
%verify "executed"
%verify "finalizable class"
    /*
     * Invoke Object.<init> on an object.  In practice we know that
     * Object's nullary constructor doesn't do anything, so we just
     * skip it unless a debugger is active.
     */
    FETCH(a1, ${cccc})                  # a1<- CCCC
    GET_VREG(a0, a1)                    # a0<- "this" ptr
    # check for NULL
    beqz    a0, common_errNullObject    # export PC and throw NPE
    LOAD_base_offObject_clazz(a1, a0)   # a1<- obj->clazz
    LOAD_base_offClassObject_accessFlags(a2, a1) # a2<- clazz->accessFlags
    and     a2, CLASS_ISFINALIZABLE     # is this class finalizable?
    beqz    a2, .L${opcode}_finish      # no, go

.L${opcode}_setFinal:
    EXPORT_PC()                         # can throw
    JAL(dvmSetFinalizable)              # call dvmSetFinalizable(obj)
    LOAD_offThread_exception(a0, rSELF)	# a0<- self->exception
    # exception pending?
    bnez    a0, common_exceptionThrown  # yes, handle it

.L${opcode}_finish:
    lhu     a1, offThread_subMode(rSELF)
    and     a1, kSubModeDebuggerActive  # debugger active?
    bnez    a1, .L${opcode}_debugger    # Yes - skip optimization
    FETCH_ADVANCE_INST(${cccc}+1)       # advance to next instr, load rINST
    GET_INST_OPCODE(t0)                 # t0<- opcode from rINST
    GOTO_OPCODE(t0)                     # execute it

%break
    /*
     * A debugger is attached, so we need to go ahead and do
     * this.  For simplicity, we'll just jump directly to the
     * corresponding handler.  Note that we can't use
     * rIBASE here because it may be in single-step mode.
     * Load the primary table base directly.
     */
.L${opcode}_debugger:
    lw      a1, offThread_mainHandlerTable(rSELF)
    li      t0, OP_INVOKE_DIRECT_RANGE
    GOTO_OPCODE_BASE(a1, t0)            # execute it
