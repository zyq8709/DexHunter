/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    lbu    a3, offThread_breakFlags(rSELF)
    la     rBIX, dvmAsmInstructionStart + (${opnum} * 128)
    lw     rIBASE, offThread_curHandlerTable(rSELF)
    bnez   a3, 1f
    jr     rBIX            # nothing to do - jump to real handler
1:
    EXPORT_PC()
    move   a0, rPC         # arg0
    move   a1, rFP         # arg1
    move   a2, rSELF       # arg2
    JAL(dvmCheckBefore)
    jr     rBIX
