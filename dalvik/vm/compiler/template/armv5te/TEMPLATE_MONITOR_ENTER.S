    /*
     * Call out to the runtime to lock an object.  Because this thread
     * may have been suspended in THREAD_MONITOR state and the Jit's
     * translation cache subsequently cleared, we cannot return directly.
     * Instead, unconditionally transition to the interpreter to resume.
     *
     * On entry:
     *    r0 - self pointer
     *    r1 - the object (which has already been null-checked by the caller
     *    r4 - the Dalvik PC of the following instruction.
     */
    ldr     r2, .LdvmLockObject
    mov     r3, #0                       @ Record that we're not returning
    str     r3, [r0, #offThread_inJitCodeCache]
    blx     r2                           @ dvmLockObject(self, obj)
    ldr     r2, .LdvmJitToInterpNoChain
    @ Bail to interpreter - no chain [note - r4 still contains rPC]
#if defined(WITH_JIT_TUNING)
    mov     r0, #kHeavyweightMonitor
#endif
    bx      r2
