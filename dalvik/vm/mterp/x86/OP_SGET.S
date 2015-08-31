%verify "executed"
%verify "field already resolved"
%verify "field not yet resolved"
%verify "field cannot be resolved"
    /*
     * General 32-bit SGET handler.
     *
     * for: sget, sget-object, sget-boolean, sget-byte, sget-char, sget-short
     */
    /* op vAA, field@BBBB */
    movl      rSELF,%ecx
    movzwl    2(rPC),%eax                        # eax<- field ref BBBB
    movl      offThread_methodClassDex(%ecx),%ecx  # ecx<- DvmDex
    movl      offDvmDex_pResFields(%ecx),%ecx    # ecx<- dvmDex->pResFields
#if defined(WITH_JIT)
    movl      %ecx, TMP_SPILL1(%ebp)
    lea       (%ecx,%eax,4),%ecx
    movl      %ecx, TMP_SPILL2(%ebp)
    movl      TMP_SPILL1(%ebp), %ecx
#endif
    movl      (%ecx,%eax,4),%eax                 # eax<- resolved StaticField ptr
    testl     %eax,%eax                          # resolved entry null?
    je        .L${opcode}_resolve                # if not, make it so
.L${opcode}_finish:     # field ptr in eax
    movl      offStaticField_value(%eax),%eax
    FETCH_INST_OPCODE 2 %ecx
    ADVANCE_PC 2
    SET_VREG %eax rINST
    GOTO_NEXT_R %ecx

    /*
     * Go resolve the field
     */
.L${opcode}_resolve:
    movl     rSELF,%ecx
    movzwl   2(rPC),%eax                        # eax<- field ref BBBB
    movl     offThread_method(%ecx),%ecx          # ecx<- current method
    EXPORT_PC                                   # could throw, need to export
    movl     offMethod_clazz(%ecx),%ecx         # ecx<- method->clazz
    movl     %eax,OUT_ARG1(%esp)
    movl     %ecx,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveStaticField              # eax<- resolved StaticField ptr
    UNSPILL(rIBASE)
    testl    %eax,%eax
    je      common_exceptionThrown             # no, handle exception
#if defined(WITH_JIT)
    movl      TMP_SPILL2(%ebp), %ecx
    SPILL(rIBASE)
    call     common_verifyField
    UNSPILL(rIBASE)
#endif
    jmp      .L${opcode}_finish                 # success, continue
