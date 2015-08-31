    /*
     * Generic 32-bit floating point operation.  Provide an "instr" line that
     * specifies an instruction that performs s2 = s0 op s1.
     *
     * On entry:
     *     r0 = target dalvik register address
     *     r1 = op1 address
     *     r2 = op2 address
     */
     flds    s0,[r1]
     flds    s1,[r2]
     $instr
     fsts    s2,[r0]
     bx      lr
