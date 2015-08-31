%verify "executed"

    /*
     * 64-bit floating point sqrt operation.
     * If the result is a NaN, bail out to library code to do
     * the right thing.
     *
     * On entry:
     *     a2 src addr of op1
     * On exit:
     *     v0,v1/fv0 = res
     */
#ifdef  SOFT_FLOAT
    LOAD64(rARG0, rARG1, a2)        # a0/a1<- vBB/vBB+1
#else
    LOAD64_F(fa0, fa0f, a2)         # fa0/fa0f<- vBB/vBB+1
    sqrt.d	fv0, fa0
    c.eq.d	fv0, fv0
    bc1t	1f
#endif
    JAL(sqrt)
1:
    RETURN
