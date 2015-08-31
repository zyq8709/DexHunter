    /*
     * This handler restores state following a selfVerification memory access.
     * On entry:
     *    a0 - offset from rSELF to the 1st element of the coreRegs save array.
     * Note: the following registers are not restored
     *       zero, AT, gp, sp, fp, ra
     */

    add     a0, a0, rSELF               # pointer to heapArgSpace.coreRegs[0]
#if 0
    lw      zero, r_ZERO*4(a0)          # restore zero
#endif
    .set noat
    lw      AT, r_AT*4(a0)              # restore at
    .set at
    lw      v0, r_V0*4(a0)              # restore v0
    lw      v1, r_V1*4(a0)              # restore v1

    lw      a1, r_A1*4(a0)              # restore a1
    lw      a2, r_A2*4(a0)              # restore a2
    lw      a3, r_A3*4(a0)              # restore a3

    lw      t0, r_T0*4(a0)              # restore t0
    lw      t1, r_T1*4(a0)              # restore t1
    lw      t2, r_T2*4(a0)              # restore t2
    lw      t3, r_T3*4(a0)              # restore t3
    lw      t4, r_T4*4(a0)              # restore t4
    lw      t5, r_T5*4(a0)              # restore t5
    lw      t6, r_T6*4(a0)              # restore t6
    lw      t7, r_T7*4(a0)              # restore t7

    lw      s0, r_S0*4(a0)              # restore s0
    lw      s1, r_S1*4(a0)              # restore s1
    lw      s2, r_S2*4(a0)              # restore s2
    lw      s3, r_S3*4(a0)              # restore s3
    lw      s4, r_S4*4(a0)              # restore s4
    lw      s5, r_S5*4(a0)              # restore s5
    lw      s6, r_S6*4(a0)              # restore s6
    lw      s7, r_S7*4(a0)              # restore s7

    lw      t8, r_T8*4(a0)              # restore t8
    lw      t9, r_T9*4(a0)              # restore t9

    lw      k0, r_K0*4(a0)              # restore k0
    lw      k1, r_K1*4(a0)              # restore k1

#if 0
    lw      gp, r_GP*4(a0)              # restore gp
    lw      sp, r_SP*4(a0)              # restore sp
    lw      fp, r_FP*4(a0)              # restore fp
    lw      ra, r_RA*4(a0)              # restore ra
#endif

/* #ifdef HARD_FLOAT */
#if 0
    lw      f0, fr0*4(a0)               # restore f0
    lw      f1, fr1*4(a0)               # restore f1
    lw      f2, fr2*4(a0)               # restore f2
    lw      f3, fr3*4(a0)               # restore f3
    lw      f4, fr4*4(a0)               # restore f4
    lw      f5, fr5*4(a0)               # restore f5
    lw      f6, fr6*4(a0)               # restore f6
    lw      f7, fr7*4(a0)               # restore f7
    lw      f8, fr8*4(a0)               # restore f8
    lw      f9, fr9*4(a0)               # restore f9
    lw      f10, fr10*4(a0)             # restore f10
    lw      f11, fr11*4(a0)             # restore f11
    lw      f12, fr12*4(a0)             # restore f12
    lw      f13, fr13*4(a0)             # restore f13
    lw      f14, fr14*4(a0)             # restore f14
    lw      f15, fr15*4(a0)             # restore f15
    lw      f16, fr16*4(a0)             # restore f16
    lw      f17, fr17*4(a0)             # restore f17
    lw      f18, fr18*4(a0)             # restore f18
    lw      f19, fr19*4(a0)             # restore f19
    lw      f20, fr20*4(a0)             # restore f20
    lw      f21, fr21*4(a0)             # restore f21
    lw      f22, fr22*4(a0)             # restore f22
    lw      f23, fr23*4(a0)             # restore f23
    lw      f24, fr24*4(a0)             # restore f24
    lw      f25, fr25*4(a0)             # restore f25
    lw      f26, fr26*4(a0)             # restore f26
    lw      f27, fr27*4(a0)             # restore f27
    lw      f28, fr28*4(a0)             # restore f28
    lw      f29, fr29*4(a0)             # restore f29
    lw      f30, fr30*4(a0)             # restore f30
    lw      f31, fr31*4(a0)             # restore f31
#endif

    lw      a0, r_A1*4(a0)              # restore a0
    RETURN
