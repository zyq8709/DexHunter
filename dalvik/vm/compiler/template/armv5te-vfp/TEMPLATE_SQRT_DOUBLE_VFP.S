%verify "executed"
    /*
     * 64-bit floating point vfp sqrt operation.
     * If the result is a NaN, bail out to library code to do
     * the right thing.
     *
     * On entry:
     *     r2 src addr of op1
     * On exit:
     *     r0,r1 = res
     */
    fldd    d0, [r2]
    fsqrtd  d1, d0
    fcmpd   d1, d1
    fmstat
    fmrrd   r0, r1, d1
    bxeq    lr   @ Result OK - return
    ldr     r2, .Lsqrt
    fmrrd   r0, r1, d0   @ reload orig operand
    bx      r2   @ tail call to sqrt library routine

.Lsqrt:
    .word   sqrt
