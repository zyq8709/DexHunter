    /*
     * To support deadlock prediction, this version of MONITOR_ENTER
     * will always call the heavyweight dvmLockObject, check for an
     * exception and then bail out to the interpreter.
     *
     * On entry:
     *    a0 - self pointer
     *    a1 - the object (which has already been null-checked by the caller
     *    rPC - the Dalvik PC of the following instruction.
     *
     */
    la     a2, .LdvmLockObject
    lw     t9, (a2)
    sw     zero, offThread_inJitCodeCache(a0)   # record that we're not returning
    JALR(t9)                                    # dvmLockObject(self, obj)
    lw     gp, STACK_OFFSET_GP(sp)

    # test for exception
    lw     a1, offThread_exception(rSELF)
    beqz   a1, 1f
    sub    a0, rPC, 2                           # roll dPC back to this monitor instruction
    j      .LhandleException
1:
    # Bail to interpreter - no chain [note - rPC still contains dPC]
#if defined(WITH_JIT_TUNING)
    li     a0, kHeavyweightMonitor
#endif
    la     a2, .LdvmJitToInterpNoChain
    lw     a2, (a2)
    jr     a2
