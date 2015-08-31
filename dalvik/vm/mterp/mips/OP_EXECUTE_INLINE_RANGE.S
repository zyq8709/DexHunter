%verify "executed"
%verify "exception handled"
    /*
     * Execute a "native inline" instruction, using "/range" semantics.
     * Same idea as execute-inline, but we get the args differently.
     *
     * We need to call an InlineOp4Func:
     *  bool (func)(u4 arg0, u4 arg1, u4 arg2, u4 arg3, JValue* pResult)
     *
     * The first four args are in a0-a3, pointer to return value storage
     * is on the stack.  The function's return value is a flag that tells
     * us if an exception was thrown.
     */
    /* [opt] execute-inline/range {vCCCC..v(CCCC+AA-1)}, inline@BBBB */
    lhu       a2, offThread_subMode(rSELF)
    FETCH(rBIX, 1)                       # rBIX<- BBBB
    EXPORT_PC()                          # can throw
    and       a2, kSubModeDebugProfile   # Any going on?
    bnez      a2, .L${opcode}_debugmode  # yes - take slow path
.L${opcode}_resume:
    addu      a1, rSELF, offThread_retval # a1<- &self->retval
    GET_OPA(a0)
    sw        a1, STACK_OFFSET_ARG04(sp)  # push &self->retval
    BAL(.L${opcode}_continue)             # make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)     #  restore gp
    beqz      v0, common_exceptionThrown  # returned false, handle exception
    FETCH_ADVANCE_INST(3)                 # advance rPC, load rINST
    GET_INST_OPCODE(t0)                   # extract opcode from rINST
    GOTO_OPCODE(t0)                       # jump to next instruction

%break

    /*
     * Extract args, call function.
     *  a0 = #of args (0-4)
     *  rBIX = call index
     *  ra = return addr, above  [DO NOT JAL out of here w/o preserving ra]
     */
.L${opcode}_continue:
    FETCH(rOBJ, 2)                       # rOBJ <- CCCC
    beq       a0, 0, 0f
    beq       a0, 1, 1f
    beq       a0, 2, 2f
    beq       a0, 3, 3f
    beq       a0, 4, 4f
    JAL(common_abort)                      #  too many arguments

4:
    add       t0, rOBJ, 3
    GET_VREG(a3, t0)
3:
    add       t0, rOBJ, 2
    GET_VREG(a2, t0)
2:
    add       t0, rOBJ, 1
    GET_VREG(a1, t0)
1:
    GET_VREG(a0, rOBJ)
0:
    la        rOBJ, gDvmInlineOpsTable      # table of InlineOperation
    EAS4(t1, rOBJ, rBIX)                    # t1 <- rINST + rBIX<<4
    lw        t9, 0(t1)
    jr        t9                            # sizeof=16, "func" is first entry
    # not reached

    /*
     * We're debugging or profiling.
     * rBIX: opIndex
     */
.L${opcode}_debugmode:
    move      a0, rBIX
    JAL(dvmResolveInlineNative)
    beqz      v0, .L${opcode}_resume       #  did it resolve? no, just move on
    move      rOBJ, v0                     #  remember method
    move      a0, v0
    move      a1, rSELF
    JAL(dvmFastMethodTraceEnter)           #  (method, self)
    addu      a1, rSELF, offThread_retval  #  a1<- &self->retval
    GET_OPA(a0)                            #  a0 <- A
    # Stack should have 16/20 available
    sw        a1, STACK_OFFSET_ARG04(sp)   #  push &self->retval
    move      rINST, rOBJ                  #  rINST<- method
    BAL(.L${opcode}_continue)              #  make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)      #  restore gp
    move      rOBJ, v0                     #  save result of inline
    move      a0, rINST                    #  a0<- method
    move      a1, rSELF                    #  a1<- self
    JAL(dvmFastNativeMethodTraceExit)      #  (method, self)
    beqz      rOBJ, common_exceptionThrown #  returned false, handle exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
