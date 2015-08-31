    /*
     * This handler restores state following a selfVerification memory access.
     * On entry:
     *    r0 - offset from rSELF to the 1st element of the coreRegs save array.
     */
    add     r0, r0, rSELF               @ pointer to heapArgSpace.coreRegs[0]
    add     r0, #64                     @ pointer to heapArgSpace.fpRegs[0]
    vldmia  r0, {d0-d15}
    sub     r0, #64                     @ pointer to heapArgSpace.coreRegs[0]
    ldmia   r0, {r0-r12}
    bx      lr
