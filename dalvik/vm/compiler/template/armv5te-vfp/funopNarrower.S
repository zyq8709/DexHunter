    /*
     * Generic 64bit-to-32bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "s0 = op d0".
     *
     * For: double-to-int, double-to-float
     *
     * On entry:
     *     r0 = target dalvik register address
     *     r1 = src dalvik register address
     */
    /* unop vA, vB */
    fldd    d0, [r1]                    @ d0<- vB
    $instr                              @ s0<- op d0
    fsts    s0, [r0]                    @ vA<- s0
    bx      lr
