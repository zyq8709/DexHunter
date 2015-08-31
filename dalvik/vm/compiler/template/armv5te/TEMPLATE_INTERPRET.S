    /*
     * This handler transfers control to the interpeter without performing
     * any lookups.  It may be called either as part of a normal chaining
     * operation, or from the transition code in header.S.  We distinquish
     * the two cases by looking at the link register.  If called from a
     * translation chain, it will point to the chaining Dalvik PC -3.
     * On entry:
     *    lr - if NULL:
     *        r1 - the Dalvik PC to begin interpretation.
     *    else
     *        [lr, #3] contains Dalvik PC to begin interpretation
     *    rSELF - pointer to thread
     *    rFP - Dalvik frame pointer
     */
    cmp     lr, #0
#if defined(WORKAROUND_CORTEX_A9_745320)
    /* Don't use conditional loads if the HW defect exists */
    beq     101f
    ldr     r1,[lr, #3]
101:
#else
    ldrne   r1,[lr, #3]
#endif
    ldr     r2, .LinterpPunt
    mov     r0, r1                       @ set Dalvik PC
    bx      r2
    @ doesn't return

.LinterpPunt:
    .word   dvmJitToInterpPunt
