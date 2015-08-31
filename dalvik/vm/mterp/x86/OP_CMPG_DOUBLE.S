%default {"is_double":"1","nanval":"1"}
%verify "executed"
%verify "basic lt, gt, eq"
%verify "left arg NaN"
%verify "right arg NaN"
    /* float/double_cmp[gl] vAA, vBB, vCC */
    movzbl    3(rPC),%eax             # eax<- CC
    movzbl    2(rPC),%ecx             # ecx<- BB
    .if $is_double
    fldl     (rFP,%eax,4)
    fldl     (rFP,%ecx,4)
    .else
    flds     (rFP,%eax,4)
    flds     (rFP,%ecx,4)
    .endif
    xorl     %ecx,%ecx
    fucompp     # z if equal, p set if NaN, c set if st0 < st1
    fnstsw   %ax
    sahf
    FETCH_INST_OPCODE 2 %eax
    jp       .L${opcode}_isNaN
    je       .L${opcode}_finish
    sbbl     %ecx,%ecx
    jb       .L${opcode}_finish
    incl     %ecx
.L${opcode}_finish:
    SET_VREG %ecx rINST
    ADVANCE_PC 2
    GOTO_NEXT_R %eax

.L${opcode}_isNaN:
    movl      $$$nanval,%ecx
    jmp       .L${opcode}_finish
