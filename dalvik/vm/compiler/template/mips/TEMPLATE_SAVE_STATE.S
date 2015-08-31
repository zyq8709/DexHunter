    /*
     * This handler performs a register save for selfVerification mode.
     * On entry:
     *    Top of stack + 4: a1 value to save
     *    Top of stack + 0: a0 value to save
     *    a0 - offset from rSELF to the beginning of the heapArgSpace record
     *    a1 - the value of regMap
     *
     * The handler must save regMap, r0-r31, f0-f31 if FPU, and then return with
     * r0-r31 with their original values (note that this means a0 and a1 must take
     * the values on the stack - not the ones in those registers on entry.
     * Finally, the two registers previously pushed must be popped.
     * Note: the following registers are not saved
     *       zero, AT, gp, sp, fp, ra
     */
    add     a0, a0, rSELF               # pointer to heapArgSpace
    sw      a1, 0(a0)                   # save regMap
    add     a0, a0, 4                   # pointer to coreRegs
#if 0
    sw      zero, r_ZERO*4(a0)          # save zero
#endif
    .set noat
    sw      AT, r_AT*4(a0)              # save at
    .set at
    sw      v0, r_V0*4(a0)              # save v0
    sw      v1, r_V1*4(a0)              # save v1

    lw      a1, 0(sp)                   # recover a0 value
    sw      a1, r_A0*4(a0)              # save a0
    lw      a1, 4(sp)                   # recover a1 value
    sw      a1, r_A1*4(a0)              # save a1
    sw      a2, r_A2*4(a0)              # save a2
    sw      a3, r_A3*4(a0)              # save a3

    sw      t0, r_T0*4(a0)              # save t0
    sw      t1, r_T1*4(a0)              # save t1
    sw      t2, r_T2*4(a0)              # save t2
    sw      t3, r_T3*4(a0)              # save t3
    sw      t4, r_T4*4(a0)              # save t4
    sw      t5, r_T5*4(a0)              # save t5
    sw      t6, r_T6*4(a0)              # save t6
    sw      t7, r_T7*4(a0)              # save t7

    sw      s0, r_S0*4(a0)              # save s0
    sw      s1, r_S1*4(a0)              # save s1
    sw      s2, r_S2*4(a0)              # save s2
    sw      s3, r_S3*4(a0)              # save s3
    sw      s4, r_S4*4(a0)              # save s4
    sw      s5, r_S5*4(a0)              # save s5
    sw      s6, r_S6*4(a0)              # save s6
    sw      s7, r_S7*4(a0)              # save s7

    sw      t8, r_T8*4(a0)              # save t8
    sw      t9, r_T9*4(a0)              # save t9

    sw      k0, r_K0*4(a0)              # save k0
    sw      k1, r_K1*4(a0)              # save k1

#if 0
    sw      gp, r_GP*4(a0)              # save gp
    sw      sp, r_SP*4(a0)              # save sp (need to adjust??? )
    sw      fp, r_FP*4(a0)              # save fp
    sw      ra, r_RA*4(a0)              # save ra
#endif

/* #ifdef HARD_FLOAT */
#if 0
    sw      f0, fr0*4(a0)               # save f0
    sw      f1, fr1*4(a0)               # save f1
    sw      f2, fr2*4(a0)               # save f2
    sw      f3, fr3*4(a0)               # save f3
    sw      f4, fr4*4(a0)               # save f4
    sw      f5, fr5*4(a0)               # save f5
    sw      f6, fr6*4(a0)               # save f6
    sw      f7, fr7*4(a0)               # save f7
    sw      f8, fr8*4(a0)               # save f8
    sw      f9, fr9*4(a0)               # save f9
    sw      f10, fr10*4(a0)             # save f10
    sw      f11, fr11*4(a0)             # save f11
    sw      f12, fr12*4(a0)             # save f12
    sw      f13, fr13*4(a0)             # save f13
    sw      f14, fr14*4(a0)             # save f14
    sw      f15, fr15*4(a0)             # save f15
    sw      f16, fr16*4(a0)             # save f16
    sw      f17, fr17*4(a0)             # save f17
    sw      f18, fr18*4(a0)             # save f18
    sw      f19, fr19*4(a0)             # save f19
    sw      f20, fr20*4(a0)             # save f20
    sw      f21, fr21*4(a0)             # save f21
    sw      f22, fr22*4(a0)             # save f22
    sw      f23, fr23*4(a0)             # save f23
    sw      f24, fr24*4(a0)             # save f24
    sw      f25, fr25*4(a0)             # save f25
    sw      f26, fr26*4(a0)             # save f26
    sw      f27, fr27*4(a0)             # save f27
    sw      f28, fr28*4(a0)             # save f28
    sw      f29, fr29*4(a0)             # save f29
    sw      f30, fr30*4(a0)             # save f30
    sw      f31, fr31*4(a0)             # save f31
#endif

    lw      a1, 0(sp)                   # recover a0 value
    lw      a1, 4(sp)                   # recover a1 value
    sub     sp, sp, 8                   # adjust stack ptr
    RETURN
