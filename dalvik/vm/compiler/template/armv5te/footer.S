/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .section .data.rel.ro
    .align  2
.LinvokeNative:
    @ Prep for the native call
    @ r1 = newFP, r0 = methodToCall
    mov     r2, #0
    ldr     r9, [rSELF, #offThread_jniLocal_topCookie]@r9<-thread->localRef->...
    str     r2, [rSELF, #offThread_inJitCodeCache] @ not in jit code cache
    str     r1, [rSELF, #offThread_curFrame]   @ curFrame = newFp
    str     r9, [r1, #(offStackSaveArea_localRefCookie - sizeofStackSaveArea)]
                                        @ newFp->localRefCookie=top
    ldrh    lr, [rSELF, #offThread_subMode]
    SAVEAREA_FROM_FP(r10, r1)           @ r10<- new stack save area

    mov     r2, r0                      @ r2<- methodToCall
    mov     r0, r1                      @ r0<- newFP
    add     r1, rSELF, #offThread_retval  @ r1<- &retval
    mov     r3, rSELF                   @ arg3<- self
    ands    lr, #kSubModeMethodTrace
    beq     121f                        @ hop if not profiling
    @ r2: methodToCall, r6: rSELF
    stmfd   sp!, {r2,r6}
    stmfd   sp!, {r0-r3}
    mov     r0, r2
    mov     r1, r6
    ldr     ip, .LdvmFastMethodTraceEnter
    blx     ip
    ldmfd   sp!, {r0-r3}

    ldr     ip, [r2, #offMethod_nativeFunc]
    blx     ip

    ldmfd   sp!, {r0-r1}
    ldr     ip, .LdvmFastNativeMethodTraceExit
    blx     ip
    b       212f
121:
    ldr     ip, [r2, #offMethod_nativeFunc]
    blx     ip
212:

    @ native return; r10=newSaveArea
    @ equivalent to dvmPopJniLocals
    ldr     r2, [r10, #offStackSaveArea_returnAddr] @ r2 = chaining cell ret
    ldr     r0, [r10, #offStackSaveArea_localRefCookie] @ r0<- saved->top
    ldr     r1, [rSELF, #offThread_exception] @ check for exception
    str     rFP, [rSELF, #offThread_curFrame]  @ curFrame = fp
    cmp     r1, #0                      @ null?
    str     r0, [rSELF, #offThread_jniLocal_topCookie] @ new top <- old top
    ldr     r0, [r10, #offStackSaveArea_savedPc] @ reload rPC

    @ r0 = dalvikCallsitePC
    bne     .LhandleException           @ no, handle exception

    str     r2, [rSELF, #offThread_inJitCodeCache] @ set the new mode
    cmp     r2, #0                      @ return chaining cell still exists?
    bxne    r2                          @ yes - go ahead

    @ continue executing the next instruction through the interpreter
    ldr     r1, .LdvmJitToInterpTraceSelectNoChain @ defined in footer.S
    add     rPC, r0, #6                 @ reconstruct new rPC (advance 6 bytes)
#if defined(WITH_JIT_TUNING)
    mov     r0, #kCallsiteInterpreted
#endif
    bx      r1

/*
 * On entry:
 * r0  Faulting Dalvik PC
 */
.LhandleException:
#if defined(WITH_SELF_VERIFICATION)
    ldr     pc, .LdeadFood @ should not see this under self-verification mode
.LdeadFood:
    .word   0xdeadf00d
#endif
    mov     r2, #0
    str     r2, [rSELF, #offThread_inJitCodeCache] @ in interpreter land
    ldr     r1, .LdvmMterpCommonExceptionThrown @ PIC way of getting &func
    ldr     rIBASE, .LdvmAsmInstructionStart    @ same as above
    mov     rPC, r0                 @ reload the faulting Dalvik address
    bx      r1                  @ branch to dvmMterpCommonExceptionThrown

    .align  2
.LdvmAsmInstructionStart:
    .word   dvmAsmInstructionStart
.LdvmJitToInterpNoChainNoProfile:
    .word   dvmJitToInterpNoChainNoProfile
.LdvmJitToInterpTraceSelectNoChain:
    .word   dvmJitToInterpTraceSelectNoChain
.LdvmJitToInterpNoChain:
    .word   dvmJitToInterpNoChain
.LdvmMterpStdBail:
    .word   dvmMterpStdBail
.LdvmMterpCommonExceptionThrown:
    .word   dvmMterpCommonExceptionThrown
.LdvmLockObject:
    .word   dvmLockObject
.LdvmJitTraceProfilingOff:
    .word   dvmJitTraceProfilingOff
#if defined(WITH_JIT_TUNING)
.LdvmICHitCount:
    .word   gDvmICHitCount
#endif
#if defined(WITH_SELF_VERIFICATION)
.LdvmSelfVerificationMemOpDecode:
    .word   dvmSelfVerificationMemOpDecode
#endif
.LdvmFastMethodTraceEnter:
    .word   dvmFastMethodTraceEnter
.LdvmFastNativeMethodTraceExit:
    .word   dvmFastNativeMethodTraceExit
.LdvmFastMethodTraceExit:
    .word   dvmFastMethodTraceExit
.L__aeabi_cdcmple:
    .word   __aeabi_cdcmple
.L__aeabi_cfcmple:
    .word   __aeabi_cfcmple

    .global dvmCompilerTemplateEnd
dvmCompilerTemplateEnd:

#endif /* WITH_JIT */
