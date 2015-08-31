/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.  Unlike the Arm handler, we can't do this as a tail call
 * because rIBASE is caller save and we need to reload it.
 *
 * Note that unlike in the Arm implementation, we should never arrive
 * here with a zero breakFlag because we always refresh rIBASE on
 * return.
 */
    EXPORT_PC
    movl   rSELF, %eax
    movl   rPC, OUT_ARG0(%esp)
    cmpb   $$0,offThread_breakFlags(%eax)    # anything to do?
    movl   rFP, OUT_ARG1(%esp)
    je     1f                                # reload rIBASE & resume if not
    movl   %eax, OUT_ARG2(%esp)
    call   dvmCheckBefore                    # (dPC, dFP, self)
    movl   rSELF, %eax
1:
    movl   offThread_curHandlerTable(%eax), rIBASE # reload rIBASE
    jmp    *dvmAsmInstructionStart+(${opnum}*4)
