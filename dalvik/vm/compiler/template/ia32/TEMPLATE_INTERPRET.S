    /*
     * This handler is a bit odd - it may be called via chaining or
     * from static code and is expected to cause control to flow
     * to the interpreter.  The problem is where to find the Dalvik
     * PC of the next instruction.  When called via chaining, the dPC
     * will be located at *rp.  When called from static code, rPC is
     * valid and rp is a real return pointer (that should be ignored).
     * The Arm target deals with this by using the link register as
     * a flag.  If it is zero, we know we were called from static code.
     * If non-zero, it points to the chain cell containing dPC.
     * For x86, we'll infer the source by looking where rp points.
     * If it points to anywhere within the code cache, we'll assume
     * we got here via chaining.  Otherwise, we'll assume rPC is valid.
     *
     * On entry:
     *    (TOS)<- return pointer or pointer to dPC
     */

/*
 * FIXME - this won't work as-is.  The cache boundaries are not
 * set up until later.  Perhaps rething this whole thing.  Do we
 * really need an interpret teplate?
 */


     movl   rSELF,%ecx
     movl   $$.LinterpPunt,%edx
     pop    %eax
     /*cmpl   %eax,offThread_jitCacheEnd(%ecx)*/
     ja     1f
     /*cmpl   %eax,offThread_jitCacheStart(%ecx)*/
     jb     1f
     movl   %eax,rPC
1:
     jmp    *(%edx)

.LinterpPunt:
    .long   dvmJitToInterpPunt
