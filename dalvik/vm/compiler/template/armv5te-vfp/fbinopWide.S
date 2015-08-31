    /*
     * Generic 64-bit floating point operation.  Provide an "instr" line that
     * specifies an instruction that performs s2 = s0 op s1.
     *
     * On entry:
     *     r0 = target dalvik register address
     *     r1 = op1 address
     *     r2 = op2 address
     */
     fldd    d0,[r1]
     fldd    d1,[r2]
     $instr
     fstd    d2,[r0]
     bx      lr
