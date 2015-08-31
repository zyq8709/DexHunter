#if defined(WITH_SELF_VERIFICATION)
    /*
     * This handler encapsulates heap memory ops for selfVerification mode.
     *
     * The call to the handler is inserted prior to a heap memory operation.
     * This handler then calls a function to decode the memory op, and process
     * it accordingly. Afterwards, the handler changes the return address to
     * skip the memory op so it never gets executed.
     */
#ifdef HARD_FLOAT
    /* push f0-f31 onto stack */
    sw      f0, fr0*-4(sp)              # push f0
    sw      f1, fr1*-4(sp)              # push f1
    sw      f2, fr2*-4(sp)              # push f2
    sw      f3, fr3*-4(sp)              # push f3
    sw      f4, fr4*-4(sp)              # push f4
    sw      f5, fr5*-4(sp)              # push f5
    sw      f6, fr6*-4(sp)              # push f6
    sw      f7, fr7*-4(sp)              # push f7
    sw      f8, fr8*-4(sp)              # push f8
    sw      f9, fr9*-4(sp)              # push f9
    sw      f10, fr10*-4(sp)            # push f10
    sw      f11, fr11*-4(sp)            # push f11
    sw      f12, fr12*-4(sp)            # push f12
    sw      f13, fr13*-4(sp)            # push f13
    sw      f14, fr14*-4(sp)            # push f14
    sw      f15, fr15*-4(sp)            # push f15
    sw      f16, fr16*-4(sp)            # push f16
    sw      f17, fr17*-4(sp)            # push f17
    sw      f18, fr18*-4(sp)            # push f18
    sw      f19, fr19*-4(sp)            # push f19
    sw      f20, fr20*-4(sp)            # push f20
    sw      f21, fr21*-4(sp)            # push f21
    sw      f22, fr22*-4(sp)            # push f22
    sw      f23, fr23*-4(sp)            # push f23
    sw      f24, fr24*-4(sp)            # push f24
    sw      f25, fr25*-4(sp)            # push f25
    sw      f26, fr26*-4(sp)            # push f26
    sw      f27, fr27*-4(sp)            # push f27
    sw      f28, fr28*-4(sp)            # push f28
    sw      f29, fr29*-4(sp)            # push f29
    sw      f30, fr30*-4(sp)            # push f30
    sw      f31, fr31*-4(sp)            # push f31

    sub     sp, (32-0)*4                # adjust stack pointer
#endif

    /* push gp registers (except zero, gp, sp, and fp) */
    .set noat
    sw      AT, r_AT*-4(sp)             # push at
    .set at
    sw      v0, r_V0*-4(sp)             # push v0
    sw      v1, r_V1*-4(sp)             # push v1
    sw      a0, r_A0*-4(sp)             # push a0
    sw      a1, r_A1*-4(sp)             # push a1
    sw      a2, r_A2*-4(sp)             # push a2
    sw      a3, r_A3*-4(sp)             # push a3
    sw      t0, r_T0*-4(sp)             # push t0
    sw      t1, r_T1*-4(sp)             # push t1
    sw      t2, r_T2*-4(sp)             # push t2
    sw      t3, r_T3*-4(sp)             # push t3
    sw      t4, r_T4*-4(sp)             # push t4
    sw      t5, r_T5*-4(sp)             # push t5
    sw      t6, r_T6*-4(sp)             # push t6
    sw      t7, r_T7*-4(sp)             # push t7
    sw      s0, r_S0*-4(sp)             # push s0
    sw      s1, r_S1*-4(sp)             # push s1
    sw      s2, r_S2*-4(sp)             # push s2
    sw      s3, r_S3*-4(sp)             # push s3
    sw      s4, r_S4*-4(sp)             # push s4
    sw      s5, r_S5*-4(sp)             # push s5
    sw      s6, r_S6*-4(sp)             # push s6
    sw      s7, r_S7*-4(sp)             # push s7
    sw      t8, r_T8*-4(sp)             # push t8
    sw      t9, r_T9*-4(sp)             # push t9
    sw      k0, r_K0*-4(sp)             # push k0
    sw      k1, r_K1*-4(sp)             # push k1
    sw      ra, r_RA*-4(sp)             # push RA

    # Note: even if we don't save all 32 registers, we still need to
    #       adjust SP by 32 registers due to the way we are storing
    #       the registers on the stack.
    sub     sp, (32-0)*4                # adjust stack pointer

    la     a2, .LdvmSelfVerificationMemOpDecode  # defined in footer.S
    lw     a2, (a2)
    move   a0, ra                       # a0<- link register
    move   a1, sp                       # a1<- stack pointer
    JALR(a2)

    /* pop gp registers (except zero, gp, sp, and fp) */
    # Note: even if we don't save all 32 registers, we still need to
    #       adjust SP by 32 registers due to the way we are storing
    #       the registers on the stack.
    add     sp, (32-0)*4                # adjust stack pointer
    .set noat
    lw      AT, r_AT*-4(sp)             # pop at
    .set at
    lw      v0, r_V0*-4(sp)             # pop v0
    lw      v1, r_V1*-4(sp)             # pop v1
    lw      a0, r_A0*-4(sp)             # pop a0
    lw      a1, r_A1*-4(sp)             # pop a1
    lw      a2, r_A2*-4(sp)             # pop a2
    lw      a3, r_A3*-4(sp)             # pop a3
    lw      t0, r_T0*-4(sp)             # pop t0
    lw      t1, r_T1*-4(sp)             # pop t1
    lw      t2, r_T2*-4(sp)             # pop t2
    lw      t3, r_T3*-4(sp)             # pop t3
    lw      t4, r_T4*-4(sp)             # pop t4
    lw      t5, r_T5*-4(sp)             # pop t5
    lw      t6, r_T6*-4(sp)             # pop t6
    lw      t7, r_T7*-4(sp)             # pop t7
    lw      s0, r_S0*-4(sp)             # pop s0
    lw      s1, r_S1*-4(sp)             # pop s1
    lw      s2, r_S2*-4(sp)             # pop s2
    lw      s3, r_S3*-4(sp)             # pop s3
    lw      s4, r_S4*-4(sp)             # pop s4
    lw      s5, r_S5*-4(sp)             # pop s5
    lw      s6, r_S6*-4(sp)             # pop s6
    lw      s7, r_S7*-4(sp)             # pop s7
    lw      t8, r_T8*-4(sp)             # pop t8
    lw      t9, r_T9*-4(sp)             # pop t9
    lw      k0, r_K0*-4(sp)             # pop k0
    lw      k1, r_K1*-4(sp)             # pop k1
    lw      ra, r_RA*-4(sp)             # pop RA

#ifdef HARD_FLOAT
    /* pop f0-f31 from stack */
    add     sp, (32-0)*4                # adjust stack pointer
    lw      f0, fr0*-4(sp)              # pop f0
    lw      f1, fr1*-4(sp)              # pop f1
    lw      f2, fr2*-4(sp)              # pop f2
    lw      f3, fr3*-4(sp)              # pop f3
    lw      f4, fr4*-4(sp)              # pop f4
    lw      f5, fr5*-4(sp)              # pop f5
    lw      f6, fr6*-4(sp)              # pop f6
    lw      f7, fr7*-4(sp)              # pop f7
    lw      f8, fr8*-4(sp)              # pop f8
    lw      f9, fr9*-4(sp)              # pop f9
    lw      f10, fr10*-4(sp)            # pop f10
    lw      f11, fr11*-4(sp)            # pop f11
    lw      f12, fr12*-4(sp)            # pop f12
    lw      f13, fr13*-4(sp)            # pop f13
    lw      f14, fr14*-4(sp)            # pop f14
    lw      f15, fr15*-4(sp)            # pop f15
    lw      f16, fr16*-4(sp)            # pop f16
    lw      f17, fr17*-4(sp)            # pop f17
    lw      f18, fr18*-4(sp)            # pop f18
    lw      f19, fr19*-4(sp)            # pop f19
    lw      f20, fr20*-4(sp)            # pop f20
    lw      f21, fr21*-4(sp)            # pop f21
    lw      f22, fr22*-4(sp)            # pop f22
    lw      f23, fr23*-4(sp)            # pop f23
    lw      f24, fr24*-4(sp)            # pop f24
    lw      f25, fr25*-4(sp)            # pop f25
    lw      f26, fr26*-4(sp)            # pop f26
    lw      f27, fr27*-4(sp)            # pop f27
    lw      f28, fr28*-4(sp)            # pop f28
    lw      f29, fr29*-4(sp)            # pop f29
    lw      f30, fr30*-4(sp)            # pop f30
    lw      f31, fr31*-4(sp)            # pop f31
#endif

    RETURN
#endif
