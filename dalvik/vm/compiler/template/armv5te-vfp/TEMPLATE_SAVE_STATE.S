    /*
     * This handler performs a register save for selfVerification mode.
     * On entry:
     *    Top of stack + 4: r7 value to save
     *    Top of stack + 0: r0 value to save
     *    r0 - offset from rSELF to the beginning of the heapArgSpace record
     *    r7 - the value of regMap
     *
     * The handler must save regMap, r0-r12 and then return with r0-r12
     * with their original values (note that this means r0 and r7 must take
     * the values on the stack - not the ones in those registers on entry.
     * Finally, the two registers previously pushed must be popped.
     */
    add     r0, r0, rSELF               @ pointer to heapArgSpace
    stmia   r0!, {r7}                   @ save regMap
    ldr     r7, [r13, #0]               @ recover r0 value
    stmia   r0!, {r7}                   @ save r0
    ldr     r7, [r13, #4]               @ recover r7 value
    stmia   r0!, {r1-r12}
    add     r0, #12                     @ move to start of FP save regio
    vstmia  r0, {d0-d15}
    pop     {r0, r7}                    @ recover r0, r7
    bx      lr
