/*
 * Inter-instruction transfer stub.  Call out to dvmCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to dvmCheckBefore is done as a tail call.
 * rIBASE updates won't be seen until a refresh, and we can tell we have a
 * stale rIBASE if breakFlags==0.  Always refresh rIBASE here, and then
 * bail to the real handler if breakFlags==0.
 */
    ldrb   r3, [rSELF, #offThread_breakFlags]
    adrl   lr, dvmAsmInstructionStart + (${opnum} * 64)
    ldr    rIBASE, [rSELF, #offThread_curHandlerTable]
    cmp    r3, #0
    bxeq   lr                   @ nothing to do - jump to real handler
    EXPORT_PC()
    mov    r0, rPC              @ arg0
    mov    r1, rFP              @ arg1
    mov    r2, rSELF            @ arg2
    b      dvmCheckBefore       @ (dPC,dFP,self) tail call
