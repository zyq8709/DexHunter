    /*
     * Call out to the runtime to lock an object.  Because this thread
     * may have been suspended in THREAD_MONITOR state and the Jit's
     * translation cache subsequently cleared, we cannot return directly.
     * Instead, unconditionally transition to the interpreter to resume.
     *
     * On entry:
     *    a0 - self pointer
     *    a1 - the object (which has already been null-checked by the caller
     *    rPC - the Dalvik PC of the following instruction.
     */
    la     a2, .LdvmLockObject
    lw     t9, (a2)
    sw     zero, offThread_inJitCodeCache(a0)   # record that we're not returning
    JALR(t9)                                    # dvmLockObject(self, obj)
    lw     gp, STACK_OFFSET_GP(sp)

    la     a2, .LdvmJitToInterpNoChain
    lw     a2, (a2)

    # Bail to interpreter - no chain [note - rPC still contains dPC]
#if defined(WITH_JIT_TUNING)
    li      a0, kHeavyweightMonitor
#endif
    jr      a2
