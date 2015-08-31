    /*
     * Generic 32bit-to-64bit floating point unary operation.  Provide an
     * "instr" line that specifies an instruction that performs "d0 = op s0".
     *
     * For: int-to-double, float-to-double
     *
     * On entry:
     *     r0 = target dalvik register address
     *     r1 = src dalvik register address
     */
    /* unop vA, vB */
    flds    s0, [r1]                    @ s0<- vB
    $instr                              @ d0<- op s0
    fstd    d0, [r0]                    @ vA<- d0
    bx      lr
