%verify "executed"
%verify "exception handled"
    /*
     * Execute a "native inline" instruction.
     *
     * We need to call an InlineOp4Func:
     *  bool (func)(u4 arg0, u4 arg1, u4 arg2, u4 arg3, JValue* pResult)
     *
     * The first four args are in a0-a3, pointer to return value storage
     * is on the stack.  The function's return value is a flag that tells
     * us if an exception was thrown.
     *
     * TUNING: could maintain two tables, pointer in Thread and
     * swap if profiler/debuggger active.
     */
    /* [opt] execute-inline vAA, {vC, vD, vE, vF}, inline@BBBB */
    lhu       a2, offThread_subMode(rSELF)
    FETCH(rBIX, 1)                         #  rBIX <- BBBB
    EXPORT_PC()                            #  can throw
    and       a2, kSubModeDebugProfile     #  Any going on?
    bnez      a2, .L${opcode}_debugmode    #  yes - take slow path
.L${opcode}_resume:
    addu      a1, rSELF, offThread_retval  #  a1 <- &self->retval
    GET_OPB(a0)                            #  a0 <- B
    # Stack should have 16/20 available
    sw        a1, STACK_OFFSET_ARG04(sp)   #  push &self->retval
    BAL(.L${opcode}_continue)              #  make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)      #  restore gp
    # test boolean result of inline
    beqz      v0, common_exceptionThrown   #  returned false, handle exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
%break

    /*
     * Extract args, call function.
     *  a0 = #of args (0-4)
     *  rBIX = call index
     *
     * Other ideas:
     * - Use a jump table from the main piece to jump directly into the
     *   AND/LW pairs.  Costs a data load, saves a branch.
     * - Have five separate pieces that do the loading, so we can work the
     *   interleave a little better.  Increases code size.
     */
.L${opcode}_continue:
    FETCH(rINST, 2)                        #  rINST <- FEDC
    beq       a0, 0, 0f
    beq       a0, 1, 1f
    beq       a0, 2, 2f
    beq       a0, 3, 3f
    beq       a0, 4, 4f
    JAL(common_abort)                      #  too many arguments

4:
    and       t0, rINST, 0xf000            #  isolate F
    ESRN(t1, rFP, t0, 10)
    lw        a3, 0(t1)                    #  a3 <- vF (shift right 12, left 2)
3:
    and       t0, rINST, 0x0f00            #  isolate E
    ESRN(t1, rFP, t0, 6)
    lw        a2, 0(t1)                    #  a2 <- vE
2:
    and       t0, rINST, 0x00f0            #  isolate D
    ESRN(t1, rFP, t0, 2)
    lw        a1, 0(t1)                    #  a1 <- vD
1:
    and       t0, rINST, 0x000f            #  isolate C
    EASN(t1, rFP, t0, 2)
    lw        a0, 0(t1)                    #  a0 <- vC
0:
    la        rINST, gDvmInlineOpsTable    #  table of InlineOperation
    EAS4(t1, rINST, rBIX)                  #  t1 <- rINST + rBIX<<4
    lw        t9, 0(t1)
    jr        t9                           #  sizeof=16, "func" is first entry
    # (not reached)

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
    GET_OPB(a0)                            #  a0 <- B
    # Stack should have 16/20 available
    sw        a1, STACK_OFFSET_ARG04(sp)   #  push &self->retval
    BAL(.L${opcode}_continue)              #  make call; will return after
    lw        gp, STACK_OFFSET_GP(sp)      #  restore gp
    move      rINST, v0                    #  save result of inline
    move      a0, rOBJ                     #  a0<- method
    move      a1, rSELF                    #  a1<- self
    JAL(dvmFastNativeMethodTraceExit)      #  (method, self)
    beqz      rINST, common_exceptionThrown   #  returned false, handle exception
    FETCH_ADVANCE_INST(3)                  #  advance rPC, load rINST
    GET_INST_OPCODE(t0)                    #  extract opcode from rINST
    GOTO_OPCODE(t0)                        #  jump to next instruction
