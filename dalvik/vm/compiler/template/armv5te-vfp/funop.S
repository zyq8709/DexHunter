    /*
     * Generic 32bit-to-32bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "s1 = op s0".
     *
     * For: float-to-int, int-to-float
     *
     * On entry:
     *     r0 = target dalvik register address
     *     r1 = src dalvik register address
     */
    /* unop vA, vB */
    flds    s0, [r1]                    @ s0<- vB
    $instr                              @ s1<- op s0
    fsts    s1, [r0]                    @ vA<- s1
    bx      lr
