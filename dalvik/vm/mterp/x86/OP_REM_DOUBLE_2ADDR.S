%verify "executed"
    /* rem_float/2addr vA, vB */
    movzx   rINSTbl,%ecx                # ecx<- A+
    sarl    $$4,rINST                  # rINST<- B
    fldl     (rFP,rINST,4)              # vBB to fp stack
    andb    $$0xf,%cl                   # ecx<- A
    fldl     (rFP,%ecx,4)               # vAA to fp stack
1:
    fprem
    fstsw     %ax
    sahf
    jp        1b
    fstp      %st(1)
    FETCH_INST_OPCODE 1 %eax
    ADVANCE_PC 1
    fstpl    (rFP,%ecx,4)               # %st to vA
    GOTO_NEXT_R %eax
