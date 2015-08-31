%verify "executed"
    /* fill-array-data vAA, +BBBBBBBB */
    movl    2(rPC),%ecx                # ecx<- BBBBbbbb
    leal    (rPC,%ecx,2),%ecx          # ecx<- PC + BBBBbbbb*2
    GET_VREG_R %eax rINST
    EXPORT_PC
    movl    %eax,OUT_ARG0(%esp)
    movl    %ecx,OUT_ARG1(%esp)
    SPILL(rIBASE)
    call    dvmInterpHandleFillArrayData
    UNSPILL(rIBASE)
    FETCH_INST_OPCODE 3 %ecx
    testl   %eax,%eax                   # exception thrown?
    je      common_exceptionThrown
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx
