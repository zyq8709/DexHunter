%default {"srcdouble":"1","tgtlong":"1"}
/* On fp to int conversions, Java requires that
 * if the result > maxint, it should be clamped to maxint.  If it is less
 * than minint, it should be clamped to minint.  If it is a nan, the result
 * should be zero.  Further, the rounding mode is to truncate.  This model
 * differs from what is delivered normally via the x86 fpu, so we have
 * to play some games.
 */
    /* float/double to int/long vA, vB */
    movzbl    rINSTbl,%ecx       # ecx<- A+
    sarl      $$4,rINST         # rINST<- B
    .if $srcdouble
    fldl     (rFP,rINST,4)       # %st0<- vB
    .else
    flds     (rFP,rINST,4)       # %st0<- vB
    .endif
    ftst
    fnstcw   LOCAL0_OFFSET(%ebp)      # remember original rounding mode
    movzwl   LOCAL0_OFFSET(%ebp),%eax
    movb     $$0xc,%ah
    movw     %ax,LOCAL0_OFFSET+2(%ebp)
    fldcw    LOCAL0_OFFSET+2(%ebp)    # set "to zero" rounding mode
    andb     $$0xf,%cl                # ecx<- A
    .if $tgtlong
    fistpll  (rFP,%ecx,4)             # convert and store
    .else
    fistpl   (rFP,%ecx,4)             # convert and store
    .endif
    fldcw    LOCAL0_OFFSET(%ebp)      # restore previous rounding mode
    .if $tgtlong
    movl     $$0x80000000,%eax
    xorl     4(rFP,%ecx,4),%eax
    orl      (rFP,%ecx,4),%eax
    .else
    cmpl     $$0x80000000,(rFP,%ecx,4)
    .endif
    je       .L${opcode}_special_case # fix up result

.L${opcode}_finish:
    FETCH_INST_OPCODE 1 %ecx
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx

.L${opcode}_special_case:
    fnstsw   %ax
    sahf
    jp       .L${opcode}_isNaN
    adcl     $$-1,(rFP,%ecx,4)
    .if $tgtlong
    adcl     $$-1,4(rFP,%ecx,4)
    .endif
   jmp       .L${opcode}_finish
.L${opcode}_isNaN:
    movl      $$0,(rFP,%ecx,4)
    .if $tgtlong
    movl      $$0,4(rFP,%ecx,4)
    .endif
    jmp       .L${opcode}_finish
