    /*
     * This handler restores state following a selfVerification memory access.
     * On entry:
     *    r0 - offset from rSELF to the 1st element of the coreRegs save array.
     */
    add     r0, r0, rSELF               @ pointer to heapArgSpace.coreRegs[0]
    ldmia   r0, {r0-r12}
    bx      lr
