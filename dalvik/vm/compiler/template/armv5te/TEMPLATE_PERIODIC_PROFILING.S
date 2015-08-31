    /*
     * Increment profile counter for this trace, and decrement
     * sample counter.  If sample counter goes below zero, turn
     * off profiling.
     *
     * On entry
     * (lr-11) is address of pointer to counter.  Note: the counter
     *    actually exists 10 bytes before the return target, but because
     *    we are arriving from thumb mode, lr will have its low bit set.
     */
     ldr    r0, [lr,#-11]
     ldr    r1, [rSELF, #offThread_pProfileCountdown]
     ldr    r2, [r0]                    @ get counter
     ldr    r3, [r1]                    @ get countdown timer
     add    r2, #1
     subs   r2, #1
     blt    .L${opcode}_disable_profiling
     str    r2, [r0]
     str    r3, [r1]
     bx     lr

.L${opcode}_disable_profiling:
     mov    r4, lr                     @ preserve lr
     ldr    r0, .LdvmJitTraceProfilingOff
     blx    r0
     bx     r4
