    /*
     * Increment profile counter for this trace, and decrement
     * sample counter.  If sample counter goes below zero, turn
     * off profiling.
     *
     * On entry
     * (ra-16) is address of pointer to counter.  Note: the counter
     *    actually exists 16 bytes before the return target for mips.
     *     - 4 bytes for prof count addr.
     *     - 4 bytes for chain cell offset (2bytes 32 bit aligned).
     *     - 4 bytes for call TEMPLATE_PERIODIC_PROFILING.
     *     - 4 bytes for call delay slot.
     */
     lw     a0, -16(ra)
     lw     a1, offThread_pProfileCountdown(rSELF)
     lw     a2, 0(a0)                   # get counter
     lw     a3, 0(a1)                   # get countdown timer
     addu   a2, 1
     sub    a3, 1                       # FIXME - bug in ARM code???
     bltz   a3, .L${opcode}_disable_profiling
     sw     a2, 0(a0)
     sw     a3, 0(a1)
     RETURN
.L${opcode}_disable_profiling:
     la     a0, dvmJitTraceProfilingOff
     JALR(a0)
     # The ra register is preserved by the JALR macro.
     jr     ra
