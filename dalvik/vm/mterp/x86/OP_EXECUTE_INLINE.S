%verify "executed"
%verify "exception handled"
    /*
     * Execute a "native inline" instruction.
     *
     * We will be calling through a function table:
     *
     * (*gDvmInlineOpsTable[opIndex].func)(arg0, arg1, arg2, arg3, pResult)
     *
     * Ignores argument count - always loads 4.
     *
     */
    /* [opt] execute-inline vAA, {vC, vD, vE, vF}, inline@BBBB */
    movl      rSELF,%ecx
    EXPORT_PC
    movzwl    2(rPC),%eax               # eax<- BBBB
    SPILL(rIBASE)                       # preserve rIBASE
    movl      offThread_subMode(%ecx), %edx # edx<- submode flags
    andl      $$kSubModeDebugProfile, %edx # debug or profile mode active?
    jnz       .L${opcode}_debugprofile   # yes, take slow path
.L${opcode}_resume:
    leal      offThread_retval(%ecx),%ecx # ecx<- &self->retval
    movl      %ecx,OUT_ARG4(%esp)
    call      .L${opcode}_continue      # make call; will return after
    UNSPILL(rIBASE)                     # restore rIBASE
    testl     %eax,%eax                 # successful?
    jz        common_exceptionThrown    # no, handle exception
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

.L${opcode}_continue:
    /*
     * Extract args, call function.
     *  ecx = #of args (0-4)
     *  eax = call index
     *  @esp = return addr
     *  esp is -4 from normal
     *
     *  Go ahead and load all 4 args, even if not used.
     */
    movzwl    4(rPC),rIBASE

    movl      $$0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $$4,rIBASE
    movl      %ecx,4+OUT_ARG0(%esp)

    movl      $$0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $$4,rIBASE
    movl      %ecx,4+OUT_ARG1(%esp)

    movl      $$0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $$4,rIBASE
    movl      %ecx,4+OUT_ARG2(%esp)

    movl      $$0xf,%ecx
    andl      rIBASE,%ecx
    GET_VREG_R  %ecx %ecx
    sarl      $$4,rIBASE
    movl      %ecx,4+OUT_ARG3(%esp)

    sall      $$4,%eax      # index *= sizeof(table entry)
    jmp       *gDvmInlineOpsTable(%eax)
    # will return to caller of .L${opcode}_continue

    /*
     * We're debugging or profiling.
     * eax: opIndex
     */
.L${opcode}_debugprofile:
    movl      %eax,OUT_ARG0(%esp)       # arg0<- BBBB
    SPILL_TMP1(%eax)                    # save opIndex
    call      dvmResolveInlineNative    # dvmResolveInlineNative(opIndex)
    movl      rSELF,%ecx                # restore self
    testl     %eax,%eax                 # method resolved?
    movl      %eax,%edx                 # save possibly resolved method in edx
    UNSPILL_TMP1(%eax)                  # in case not resolved, restore opIndex
    jz        .L${opcode}_resume        # not resolved, just move on
    SPILL_TMP2(%edx)                    # save method
    movl      %edx,OUT_ARG0(%esp)       # arg0<- method
    movl      %ecx,OUT_ARG1(%esp)       # arg1<- self
    call      dvmFastMethodTraceEnter   # dvmFastMethodTraceEnter(method,self)
    movl      rSELF,%ecx                # restore self
    UNSPILL_TMP1(%eax)                  # restore opIndex
    leal      offThread_retval(%ecx),%ecx # ecx<- &self->retval
    movl      %ecx,OUT_ARG4(%esp)       # needed for pResult of inline operation handler
    call      .L${opcode}_continue      # make call; will return after
    SPILL_TMP1(%eax)                    # save result of inline
    UNSPILL_TMP2(%eax)                  # restore method
    movl      rSELF,%ecx                # restore self
    movl      %eax,OUT_ARG0(%esp)       # arg0<- method
    movl      %ecx,OUT_ARG1(%esp)       # arg1<- self
    call      dvmFastNativeMethodTraceExit # dvmFastNativeMethodTraceExit(method,self)
    UNSPILL(rIBASE)                     # restore rIBASE
    UNSPILL_TMP1(%eax)                  # restore result of inline
    testl     %eax,%eax                 # successful?
    jz        common_exceptionThrown    # no, handle exception
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx
