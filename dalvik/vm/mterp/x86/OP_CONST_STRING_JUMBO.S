
%verify "String already resolved"
%verify "String not yet resolved"
%verify "String cannot be resolved"
    /* const/string vAA, String@BBBBBBBB */
    movl      rSELF,%ecx
    movl      2(rPC),%eax              # eax<- BBBBBBBB
    movl      offThread_methodClassDex(%ecx),%ecx# ecx<- self->methodClassDex
    movl      offDvmDex_pResStrings(%ecx),%ecx # ecx<- dvmDex->pResStrings
    movl      (%ecx,%eax,4),%eax       # eax<- rResString[BBBB]
    FETCH_INST_OPCODE 3 %ecx
    testl     %eax,%eax                # resolved yet?
    je        .L${opcode}_resolve
    SET_VREG  %eax rINST               # vAA<- rResString[BBBB]
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx

/* This is the less common path, so we'll redo some work
   here rather than force spills on the common path */
.L${opcode}_resolve:
    movl     rSELF,%eax
    EXPORT_PC
    movl     offThread_method(%eax),%eax # eax<- self->method
    movl     2(rPC),%ecx               # ecx<- BBBBBBBB
    movl     offMethod_clazz(%eax),%eax
    movl     %ecx,OUT_ARG1(%esp)
    movl     %eax,OUT_ARG0(%esp)
    SPILL(rIBASE)
    call     dvmResolveString          # go resolve
    UNSPILL(rIBASE)
    testl    %eax,%eax                 # failed?
    je       common_exceptionThrown
    FETCH_INST_OPCODE 3 %ecx
    SET_VREG %eax rINST
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx
