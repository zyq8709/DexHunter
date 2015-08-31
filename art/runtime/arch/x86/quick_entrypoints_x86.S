/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "asm_support_x86.S"

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kSaveAll)
     */
MACRO0(SETUP_SAVE_ALL_CALLEE_SAVE_FRAME)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl  MACRO_LITERAL(16), %esp  // Grow stack by 4 words, bottom word will hold Method*
    .cfi_adjust_cfa_offset 16
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsOnly)
     */
MACRO0(SETUP_REF_ONLY_CALLEE_SAVE_FRAME)
    PUSH edi  // Save callee saves (ebx is saved/restored by the upcall)
    PUSH esi
    PUSH ebp
    subl  MACRO_LITERAL(16), %esp  // Grow stack by 4 words, bottom word will hold Method*
    .cfi_adjust_cfa_offset 16
END_MACRO

MACRO0(RESTORE_REF_ONLY_CALLEE_SAVE_FRAME)
    addl MACRO_LITERAL(28), %esp  // Unwind stack up to return address
    .cfi_adjust_cfa_offset -28
END_MACRO

    /*
     * Macro that sets up the callee save frame to conform with
     * Runtime::CreateCalleeSaveMethod(kRefsAndArgs)
     */
MACRO0(SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME)
    PUSH edi  // Save callee saves
    PUSH esi
    PUSH ebp
    PUSH ebx  // Save args
    PUSH edx
    PUSH ecx
    PUSH eax   // Align stack, eax will be clobbered by Method*
END_MACRO

MACRO0(RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME)
    addl MACRO_LITERAL(4), %esp  // Remove padding
    .cfi_adjust_cfa_offset -4
    POP ecx  // Restore args except eax
    POP edx
    POP ebx
    POP ebp  // Restore callee saves
    POP esi
    POP edi
END_MACRO

    /*
     * Macro that set calls through to artDeliverPendingExceptionFromCode, where the pending
     * exception is Thread::Current()->exception_.
     */
MACRO0(DELIVER_PENDING_EXCEPTION)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME         // save callee saves for throw
    mov %esp, %ecx
    // Outgoing argument set up
    subl  MACRO_LITERAL(8), %esp             // Alignment padding
    .cfi_adjust_cfa_offset 8
    PUSH ecx                                 // pass SP
    pushl %fs:THREAD_SELF_OFFSET             // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    call SYMBOL(artDeliverPendingExceptionFromCode)  // artDeliverPendingExceptionFromCode(Thread*, SP)
    int3                                     // unreached
END_MACRO

MACRO2(NO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %ecx
    // Outgoing argument set up
    subl  MACRO_LITERAL(8), %esp  // alignment padding
    .cfi_adjust_cfa_offset 8
    PUSH ecx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    call VAR(cxx_name, 1)         // cxx_name(Thread*, SP)
    int3                          // unreached
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO2(ONE_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %ecx
    // Outgoing argument set up
    PUSH eax                      // alignment padding
    PUSH ecx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, Thread*, SP)
    int3                          // unreached
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO2(TWO_ARG_RUNTIME_EXCEPTION, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME  // save all registers as basis for long jump context
    mov %esp, %edx
    // Outgoing argument set up
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, Thread*, SP)
    int3                          // unreached
    END_FUNCTION VAR(c_name, 0)
END_MACRO

    /*
     * Called by managed code to create and deliver a NullPointerException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_null_pointer_exception, artThrowNullPointerExceptionFromCode

    /*
     * Called by managed code to create and deliver an ArithmeticException.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_div_zero, artThrowDivZeroFromCode

    /*
     * Called by managed code to create and deliver a StackOverflowError.
     */
NO_ARG_RUNTIME_EXCEPTION art_quick_throw_stack_overflow, artThrowStackOverflowFromCode

    /*
     * Called by managed code, saves callee saves and then calls artThrowException
     * that will place a mock Method* at the bottom of the stack. Arg1 holds the exception.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_deliver_exception, artDeliverExceptionFromCode

    /*
     * Called by managed code to create and deliver a NoSuchMethodError.
     */
ONE_ARG_RUNTIME_EXCEPTION art_quick_throw_no_such_method, artThrowNoSuchMethodFromCode

    /*
     * Called by managed code to create and deliver an ArrayIndexOutOfBoundsException. Arg1 holds
     * index, arg2 holds limit.
     */
TWO_ARG_RUNTIME_EXCEPTION art_quick_throw_array_bounds, artThrowArrayBoundsFromCode

    /*
     * All generated callsites for interface invokes and invocation slow paths will load arguments
     * as usual - except instead of loading arg0/r0 with the target Method*, arg0/r0 will contain
     * the method_idx.  This wrapper will save arg1-arg3, load the caller's Method*, align the
     * stack and call the appropriate C helper.
     * NOTE: "this" is first visible argument of the target, and so can be found in arg1/r1.
     *
     * The helper will attempt to locate the target and return a 64-bit result in r0/r1 consisting
     * of the target Method* in r0 and method->code_ in r1.
     *
     * If unsuccessful, the helper will return NULL/NULL. There will bea pending exception in the
     * thread and we branch to another stub to deliver it.
     *
     * On success this wrapper will restore arguments and *jump* to the target, leaving the lr
     * pointing back to the original caller.
     */
MACRO2(INVOKE_TRAMPOLINE, c_name, cxx_name)
    DEFINE_FUNCTION VAR(c_name, 0)
    // Set up the callee save frame to conform with Runtime::CreateCalleeSaveMethod(kRefsAndArgs)
    // return address
    PUSH edi
    PUSH esi
    PUSH ebp
    PUSH ebx
    PUSH edx
    PUSH ecx
    PUSH eax   // <-- callee save Method* to go here
    movl %esp, %edx  // remember SP
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp  // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    pushl 32(%edx)                // pass caller Method*
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, arg3, Thread*, SP)
    movl %edx, %edi               // save code pointer in EDI
    addl MACRO_LITERAL(36), %esp  // Pop arguments skip eax
    .cfi_adjust_cfa_offset -36
    POP ecx                       // Restore args
    POP edx
    POP ebx
    POP ebp  // Restore callee saves.
    POP esi
    // Swap EDI callee save with code pointer.
    xchgl %edi, (%esp)
    testl %eax, %eax              // Branch forward if exception pending.
    jz    1f
    // Tail call to intended method.
    ret
1:
    addl MACRO_LITERAL(4), %esp   // Pop code pointer off stack
    .cfi_adjust_cfa_offset -4
    DELIVER_PENDING_EXCEPTION
    END_FUNCTION VAR(c_name, 0)
END_MACRO

INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline, artInvokeInterfaceTrampoline
INVOKE_TRAMPOLINE art_quick_invoke_interface_trampoline_with_access_check, artInvokeInterfaceTrampolineWithAccessCheck

INVOKE_TRAMPOLINE art_quick_invoke_static_trampoline_with_access_check, artInvokeStaticTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_direct_trampoline_with_access_check, artInvokeDirectTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_super_trampoline_with_access_check, artInvokeSuperTrampolineWithAccessCheck
INVOKE_TRAMPOLINE art_quick_invoke_virtual_trampoline_with_access_check, artInvokeVirtualTrampolineWithAccessCheck

    /*
     * Quick invocation stub.
     * On entry:
     *   [sp] = return address
     *   [sp + 4] = method pointer
     *   [sp + 8] = argument array or NULL for no argument methods
     *   [sp + 12] = size of argument array in bytes
     *   [sp + 16] = (managed) thread pointer
     *   [sp + 20] = JValue* result
     *   [sp + 24] = result type char
     */
DEFINE_FUNCTION art_quick_invoke_stub
    PUSH ebp                      // save ebp
    PUSH ebx                      // save ebx
    mov %esp, %ebp                // copy value of stack pointer into base pointer
    .cfi_def_cfa_register ebp
    mov 20(%ebp), %ebx            // get arg array size
    addl LITERAL(28), %ebx        // reserve space for return addr, method*, ebx, and ebp in frame
    andl LITERAL(0xFFFFFFF0), %ebx    // align frame size to 16 bytes
    subl LITERAL(12), %ebx        // remove space for return address, ebx, and ebp
    subl %ebx, %esp               // reserve stack space for argument array
    lea  4(%esp), %eax            // use stack pointer + method ptr as dest for memcpy
    pushl 20(%ebp)                // push size of region to memcpy
    pushl 16(%ebp)                // push arg array as source of memcpy
    pushl %eax                    // push stack pointer as destination of memcpy
    call SYMBOL(memcpy)           // (void*, const void*, size_t)
    addl LITERAL(12), %esp        // pop arguments to memcpy
    movl LITERAL(0), (%esp)       // store NULL for method*
    mov 12(%ebp), %eax            // move method pointer into eax
    mov 4(%esp), %ecx             // copy arg1 into ecx
    mov 8(%esp), %edx             // copy arg2 into edx
    mov 12(%esp), %ebx            // copy arg3 into ebx
    call *METHOD_CODE_OFFSET(%eax) // call the method
    mov %ebp, %esp                // restore stack pointer
    POP ebx                       // pop ebx
    POP ebp                       // pop ebp
    mov 20(%esp), %ecx            // get result pointer
    cmpl LITERAL(68), 24(%esp)    // test if result type char == 'D'
    je return_double_quick
    cmpl LITERAL(70), 24(%esp)    // test if result type char == 'F'
    je return_float_quick
    mov %eax, (%ecx)              // store the result
    mov %edx, 4(%ecx)             // store the other half of the result
    ret
return_double_quick:
return_float_quick:
    movsd %xmm0, (%ecx)           // store the floating point result
    ret
END_FUNCTION art_quick_invoke_stub

MACRO3(NO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    // Outgoing argument set up
    subl MACRO_LITERAL(8), %esp   // push padding
    .cfi_adjust_cfa_offset 8
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    call VAR(cxx_name, 1)         // cxx_name(Thread*, SP)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    .cfi_adjust_cfa_offset -16
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(ONE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    // Outgoing argument set up
    PUSH eax                      // push padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, Thread*, SP)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    .cfi_adjust_cfa_offset -16
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(TWO_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    // Outgoing argument set up
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, Thread*, SP)
    addl MACRO_LITERAL(16), %esp  // pop arguments
    .cfi_adjust_cfa_offset -16
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO3(THREE_ARG_DOWNCALL, c_name, cxx_name, return_macro)
    DEFINE_FUNCTION VAR(c_name, 0)
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    // Outgoing argument set up
    subl MACRO_LITERAL(12), %esp  // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call VAR(cxx_name, 1)         // cxx_name(arg1, arg2, arg3, Thread*, SP)
    addl MACRO_LITERAL(32), %esp  // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    CALL_MACRO(return_macro, 2)   // return or deliver exception
    END_FUNCTION VAR(c_name, 0)
END_MACRO

MACRO0(RETURN_IF_EAX_NOT_ZERO)
    testl %eax, %eax               // eax == 0 ?
    jz  1f                         // if eax == 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

MACRO0(RETURN_IF_EAX_ZERO)
    testl %eax, %eax               // eax == 0 ?
    jnz  1f                        // if eax != 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

MACRO0(RETURN_OR_DELIVER_PENDING_EXCEPTION)
    mov %fs:THREAD_EXCEPTION_OFFSET, %ebx // get exception field
    testl %ebx, %ebx               // ebx == 0 ?
    jnz 1f                         // if ebx != 0 goto 1
    ret                            // return
1:                                 // deliver exception on current thread
    DELIVER_PENDING_EXCEPTION
END_MACRO

TWO_ARG_DOWNCALL art_quick_alloc_object, artAllocObjectFromCode, RETURN_IF_EAX_NOT_ZERO
TWO_ARG_DOWNCALL art_quick_alloc_object_with_access_check, artAllocObjectFromCodeWithAccessCheck, RETURN_IF_EAX_NOT_ZERO
THREE_ARG_DOWNCALL art_quick_alloc_array, artAllocArrayFromCode, RETURN_IF_EAX_NOT_ZERO
THREE_ARG_DOWNCALL art_quick_alloc_array_with_access_check, artAllocArrayFromCodeWithAccessCheck, RETURN_IF_EAX_NOT_ZERO
THREE_ARG_DOWNCALL art_quick_check_and_alloc_array, artCheckAndAllocArrayFromCode, RETURN_IF_EAX_NOT_ZERO
THREE_ARG_DOWNCALL art_quick_check_and_alloc_array_with_access_check, artCheckAndAllocArrayFromCodeWithAccessCheck, RETURN_IF_EAX_NOT_ZERO

TWO_ARG_DOWNCALL art_quick_resolve_string, artResolveStringFromCode, RETURN_IF_EAX_NOT_ZERO
TWO_ARG_DOWNCALL art_quick_initialize_static_storage, artInitializeStaticStorageFromCode, RETURN_IF_EAX_NOT_ZERO
TWO_ARG_DOWNCALL art_quick_initialize_type, artInitializeTypeFromCode, RETURN_IF_EAX_NOT_ZERO
TWO_ARG_DOWNCALL art_quick_initialize_type_and_verify_access, artInitializeTypeAndVerifyAccessFromCode, RETURN_IF_EAX_NOT_ZERO

ONE_ARG_DOWNCALL art_quick_lock_object, artLockObjectFromCode, ret
ONE_ARG_DOWNCALL art_quick_unlock_object, artUnlockObjectFromCode, RETURN_IF_EAX_ZERO

TWO_ARG_DOWNCALL art_quick_handle_fill_data, artHandleFillArrayDataFromCode, RETURN_IF_EAX_ZERO

DEFINE_FUNCTION art_quick_is_assignable
    PUSH eax                     // alignment padding
    PUSH ecx                    // pass arg2
    PUSH eax                     // pass arg1
    call SYMBOL(artIsAssignableFromCode)  // (Class* a, Class* b, Thread*, SP)
    addl LITERAL(12), %esp        // pop arguments
    .cfi_adjust_cfa_offset -12
    ret
END_FUNCTION art_quick_is_assignable

DEFINE_FUNCTION art_quick_memcpy
    PUSH edx                      // pass arg3
    PUSH ecx                      // pass arg2
    PUSH eax                      // pass arg1
    call SYMBOL(memcpy)           // (void*, const void*, size_t)
    addl LITERAL(12), %esp        // pop arguments
    .cfi_adjust_cfa_offset -12
    ret
END_FUNCTION art_quick_memcpy

TWO_ARG_DOWNCALL art_quick_check_cast, artCheckCastFromCode, RETURN_IF_EAX_ZERO
TWO_ARG_DOWNCALL art_quick_can_put_array_element, artCanPutArrayElementFromCode, RETURN_IF_EAX_ZERO

NO_ARG_DOWNCALL art_quick_test_suspend, artTestSuspendFromCode, ret

DEFINE_FUNCTION art_quick_fmod
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                      // pass arg4 b.hi
    PUSH edx                      // pass arg3 b.lo
    PUSH ecx                      // pass arg2 a.hi
    PUSH eax                      // pass arg1 a.lo
    call SYMBOL(fmod)             // (jdouble a, jdouble b)
    fstpl (%esp)                  // pop return value off fp stack
    movsd (%esp), %xmm0           // place into %xmm0
    addl LITERAL(28), %esp        // pop arguments
    .cfi_adjust_cfa_offset -28
    ret
END_FUNCTION art_quick_fmod

DEFINE_FUNCTION art_quick_fmodf
    PUSH eax                      // alignment padding
    PUSH ecx                      // pass arg2 b
    PUSH eax                      // pass arg1 a
    call SYMBOL(fmodf)            // (jfloat a, jfloat b)
    fstps (%esp)                  // pop return value off fp stack
    movss (%esp), %xmm0           // place into %xmm0
    addl LITERAL(12), %esp        // pop arguments
    .cfi_adjust_cfa_offset -12
    ret
END_FUNCTION art_quick_fmodf

DEFINE_FUNCTION art_quick_l2d
    PUSH ecx                      // push arg2 a.hi
    PUSH eax                      // push arg1 a.lo
    fildll (%esp)                 // load as integer and push into st0
    fstpl (%esp)                  // pop value off fp stack as double
    movsd (%esp), %xmm0           // place into %xmm0
    addl LITERAL(8), %esp         // pop arguments
    .cfi_adjust_cfa_offset -8
    ret
END_FUNCTION art_quick_l2d

DEFINE_FUNCTION art_quick_l2f
    PUSH ecx                      // push arg2 a.hi
    PUSH eax                      // push arg1 a.lo
    fildll (%esp)                 // load as integer and push into st0
    fstps (%esp)                  // pop value off fp stack as a single
    movss (%esp), %xmm0           // place into %xmm0
    addl LITERAL(8), %esp         // pop argument
    .cfi_adjust_cfa_offset -8
    ret
END_FUNCTION art_quick_l2f

DEFINE_FUNCTION art_quick_d2l
    PUSH eax                      // alignment padding
    PUSH ecx                      // pass arg2 a.hi
    PUSH eax                      // pass arg1 a.lo
    call SYMBOL(art_d2l)          // (jdouble a)
    addl LITERAL(12), %esp        // pop arguments
    .cfi_adjust_cfa_offset -12
    ret
END_FUNCTION art_quick_d2l

DEFINE_FUNCTION art_quick_f2l
    subl LITERAL(8), %esp         // alignment padding
    .cfi_adjust_cfa_offset 8
    PUSH eax                      // pass arg1 a
    call SYMBOL(art_f2l)          // (jfloat a)
    addl LITERAL(12), %esp        // pop arguments
    .cfi_adjust_cfa_offset -12
    ret
END_FUNCTION art_quick_f2l

DEFINE_FUNCTION art_quick_idivmod
    cmpl LITERAL(0x80000000), %eax
    je check_arg2  // special case
args_ok:
    cdq         // edx:eax = sign extend eax
    idiv %ecx   // (edx,eax) = (edx:eax % ecx, edx:eax / ecx)
    ret
check_arg2:
    cmpl LITERAL(-1), %ecx
    jne args_ok
    xorl %edx, %edx
    ret         // eax already holds min int
END_FUNCTION art_quick_idivmod

DEFINE_FUNCTION art_quick_ldiv
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                     // pass arg4 b.hi
    PUSH edx                     // pass arg3 b.lo
    PUSH ecx                     // pass arg2 a.hi
    PUSH eax                      // pass arg1 a.lo
    call SYMBOL(artLdivFromCode)  // (jlong a, jlong b)
    addl LITERAL(28), %esp        // pop arguments
    .cfi_adjust_cfa_offset -28
    ret
END_FUNCTION art_quick_ldiv

DEFINE_FUNCTION art_quick_ldivmod
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                     // pass arg4 b.hi
    PUSH edx                     // pass arg3 b.lo
    PUSH ecx                     // pass arg2 a.hi
    PUSH eax                      // pass arg1 a.lo
    call SYMBOL(artLdivmodFromCode) // (jlong a, jlong b)
    addl LITERAL(28), %esp        // pop arguments
    .cfi_adjust_cfa_offset -28
    ret
END_FUNCTION art_quick_ldivmod

DEFINE_FUNCTION art_quick_lmul
    imul %eax, %ebx              // ebx = a.lo(eax) * b.hi(ebx)
    imul %edx, %ecx              // ecx = b.lo(edx) * a.hi(ecx)
    mul  %edx                    // edx:eax = a.lo(eax) * b.lo(edx)
    add  %ebx, %ecx
    add  %ecx, %edx              // edx += (a.lo * b.hi) + (b.lo * a.hi)
    ret
END_FUNCTION art_quick_lmul

DEFINE_FUNCTION art_quick_lshl
    // ecx:eax << edx
    xchg %edx, %ecx
    shld %cl,%eax,%edx
    shl  %cl,%eax
    test LITERAL(32), %cl
    jz  1f
    mov %eax, %edx
    xor %eax, %eax
1:
    ret
END_FUNCTION art_quick_lshl

DEFINE_FUNCTION art_quick_lshr
    // ecx:eax >> edx
    xchg %edx, %ecx
    shrd %cl,%edx,%eax
    sar  %cl,%edx
    test LITERAL(32),%cl
    jz  1f
    mov %edx, %eax
    sar LITERAL(31), %edx
1:
    ret
END_FUNCTION art_quick_lshr

DEFINE_FUNCTION art_quick_lushr
    // ecx:eax >>> edx
    xchg %edx, %ecx
    shrd %cl,%edx,%eax
    shr  %cl,%edx
    test LITERAL(32),%cl
    jz  1f
    mov %edx, %eax
    xor %edx, %edx
1:
    ret
END_FUNCTION art_quick_lushr

DEFINE_FUNCTION art_quick_set32_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    subl LITERAL(8), %esp         // alignment padding
    .cfi_adjust_cfa_offset 8
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    mov 32(%ebx), %ebx            // get referrer
    PUSH ebx                      // pass referrer
    PUSH edx                      // pass new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artSet32InstanceFromCode)  // (field_idx, Object*, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set32_instance

DEFINE_FUNCTION art_quick_set64_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    subl LITERAL(8), %esp         // alignment padding
    .cfi_adjust_cfa_offset 8
    PUSH esp                      // pass SP-8
    addl LITERAL(8), (%esp)       // fix SP on stack by adding 8
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ebx                      // pass high half of new_val
    PUSH edx                      // pass low half of new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artSet64InstanceFromCode)  // (field_idx, Object*, new_val, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set64_instance

DEFINE_FUNCTION art_quick_set_obj_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    subl LITERAL(8), %esp         // alignment padding
    .cfi_adjust_cfa_offset 8
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    mov 32(%ebx), %ebx            // get referrer
    PUSH ebx                      // pass referrer
    PUSH edx                      // pass new_val
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artSetObjInstanceFromCode) // (field_idx, Object*, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set_obj_instance

DEFINE_FUNCTION art_quick_get32_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artGet32InstanceFromCode)  // (field_idx, Object*, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get32_instance

DEFINE_FUNCTION art_quick_get64_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artGet64InstanceFromCode)  // (field_idx, Object*, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get64_instance

DEFINE_FUNCTION art_quick_get_obj_instance
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass object
    PUSH eax                      // pass field_idx
    call SYMBOL(artGetObjInstanceFromCode) // (field_idx, Object*, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get_obj_instance

DEFINE_FUNCTION art_quick_set32_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass new_val
    PUSH eax                      // pass field_idx
    call SYMBOL(artSet32StaticFromCode)    // (field_idx, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set32_static

DEFINE_FUNCTION art_quick_set64_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    subl LITERAL(8), %esp         // alignment padding
    .cfi_adjust_cfa_offset 8
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    mov 32(%ebx), %ebx            // get referrer
    PUSH edx                      // pass high half of new_val
    PUSH ecx                      // pass low half of new_val
    PUSH ebx                      // pass referrer
    PUSH eax                      // pass field_idx
    call SYMBOL(artSet64StaticFromCode)  // (field_idx, referrer, new_val, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    .cfi_adjust_cfa_offset -32
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set64_static

DEFINE_FUNCTION art_quick_set_obj_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %ebx                // remember SP
    mov 32(%esp), %edx            // get referrer
    subl LITERAL(12), %esp        // alignment padding
    .cfi_adjust_cfa_offset 12
    PUSH ebx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH edx                      // pass referrer
    PUSH ecx                      // pass new_val
    PUSH eax                      // pass field_idx
    call SYMBOL(artSetObjStaticFromCode)  // (field_idx, new_val, referrer, Thread*, SP)
    addl LITERAL(32), %esp        // pop arguments
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME  // restore frame up to return address
    RETURN_IF_EAX_ZERO            // return or deliver exception
END_FUNCTION art_quick_set_obj_static

DEFINE_FUNCTION art_quick_get32_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    mov 32(%esp), %ecx            // get referrer
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass referrer
    PUSH eax                      // pass field_idx
    call SYMBOL(artGet32StaticFromCode)    // (field_idx, referrer, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    .cfi_adjust_cfa_offset -16
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get32_static

DEFINE_FUNCTION art_quick_get64_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME  // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    mov 32(%esp), %ecx            // get referrer
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass referrer
    PUSH eax                      // pass field_idx
    call SYMBOL(artGet64StaticFromCode)    // (field_idx, referrer, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    .cfi_adjust_cfa_offset -16
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get64_static

DEFINE_FUNCTION art_quick_get_obj_static
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME       // save ref containing registers for GC
    mov %esp, %edx                // remember SP
    mov 32(%esp), %ecx            // get referrer
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass referrer
    PUSH eax                      // pass field_idx
    call SYMBOL(artGetObjStaticFromCode)   // (field_idx, referrer, Thread*, SP)
    addl LITERAL(16), %esp        // pop arguments
    .cfi_adjust_cfa_offset -16
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME     // restore frame up to return address
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_get_obj_static

DEFINE_FUNCTION art_quick_proxy_invoke_handler
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME   // save frame and Method*
    PUSH esp                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass proxy method
    call SYMBOL(artQuickProxyInvokeHandler) // (proxy method, receiver, Thread*, SP)
    movd %eax, %xmm0              // place return value also into floating point return value
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    addl LITERAL(44), %esp        // pop arguments
    .cfi_adjust_cfa_offset -44
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_proxy_invoke_handler

DEFINE_FUNCTION art_quick_resolution_trampoline
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    PUSH esp                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // pass receiver
    PUSH eax                      // pass method
    call SYMBOL(artQuickResolutionTrampoline) // (Method* called, receiver, Thread*, SP)
    movl %eax, %edi               // remember code pointer in EDI
    addl LITERAL(16), %esp        // pop arguments
    test %eax, %eax               // if code pointer is NULL goto deliver pending exception
    jz 1f
    POP eax                       // called method
    POP ecx                       // restore args
    POP edx
    POP ebx
    POP ebp                       // restore callee saves except EDI
    POP esi
    xchgl 0(%esp),%edi            // restore EDI and place code pointer as only value on stack
    ret                           // tail call into method
1:
    RESTORE_REF_AND_ARGS_CALLEE_SAVE_FRAME
    DELIVER_PENDING_EXCEPTION
END_FUNCTION art_quick_resolution_trampoline

DEFINE_FUNCTION art_quick_to_interpreter_bridge
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME   // save frame
    mov %esp, %edx                // remember SP
    PUSH eax                      // alignment padding
    PUSH edx                      // pass SP
    pushl %fs:THREAD_SELF_OFFSET  // pass Thread::Current()
    .cfi_adjust_cfa_offset 4
    PUSH eax                      // pass  method
    call SYMBOL(artQuickToInterpreterBridge)  // (method, Thread*, SP)
    movd %eax, %xmm0              // place return value also into floating point return value
    movd %edx, %xmm1
    punpckldq %xmm1, %xmm0
    addl LITERAL(44), %esp        // pop arguments
    .cfi_adjust_cfa_offset -44
    RETURN_OR_DELIVER_PENDING_EXCEPTION    // return or deliver exception
END_FUNCTION art_quick_to_interpreter_bridge

    /*
     * Routine that intercepts method calls and returns.
     */
DEFINE_FUNCTION art_quick_instrumentation_entry
    SETUP_REF_AND_ARGS_CALLEE_SAVE_FRAME
    movl  %esp, %edx              // Save SP.
    PUSH eax                      // Save eax which will be clobbered by the callee-save method.
    subl LITERAL(8), %esp         // Align stack.
    .cfi_adjust_cfa_offset 8
    pushl 40(%esp)                // Pass LR.
    .cfi_adjust_cfa_offset 4
    PUSH edx                      // Pass SP.
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    .cfi_adjust_cfa_offset 4
    PUSH ecx                      // Pass receiver.
    PUSH eax                      // Pass Method*.
    call  SYMBOL(artInstrumentationMethodEntryFromCode) // (Method*, Object*, Thread*, SP, LR)
    addl  LITERAL(28), %esp       // Pop arguments upto saved Method*.
    movl 28(%esp), %edi           // Restore edi.
    movl %eax, 28(%esp)           // Place code* over edi, just under return pc.
    movl LITERAL(SYMBOL(art_quick_instrumentation_exit)), 32(%esp)
                                  // Place instrumentation exit as return pc.
    movl (%esp), %eax             // Restore eax.
    movl 8(%esp), %ecx            // Restore ecx.
    movl 12(%esp), %edx           // Restore edx.
    movl 16(%esp), %ebx           // Restore ebx.
    movl 20(%esp), %ebp           // Restore ebp.
    movl 24(%esp), %esi           // Restore esi.
    addl LITERAL(28), %esp        // Wind stack back upto code*.
    ret                           // Call method (and pop).
END_FUNCTION art_quick_instrumentation_entry

DEFINE_FUNCTION art_quick_instrumentation_exit
    pushl LITERAL(0)              // Push a fake return PC as there will be none on the stack.
    SETUP_REF_ONLY_CALLEE_SAVE_FRAME
    mov  %esp, %ecx               // Remember SP
    subl LITERAL(8), %esp         // Save float return value.
    .cfi_adjust_cfa_offset 8
    movd %xmm0, (%esp)
    PUSH edx                      // Save gpr return value.
    PUSH eax
    subl LITERAL(8), %esp         // Align stack
    movd %xmm0, (%esp)
    subl LITERAL(8), %esp         // Pass float return value.
    .cfi_adjust_cfa_offset 8
    movd %xmm0, (%esp)
    PUSH edx                      // Pass gpr return value.
    PUSH eax
    PUSH ecx                      // Pass SP.
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current.
    .cfi_adjust_cfa_offset 4
    call  SYMBOL(artInstrumentationMethodExitFromCode)  // (Thread*, SP, gpr_result, fpr_result)
    mov   %eax, %ecx              // Move returned link register.
    addl LITERAL(32), %esp        // Pop arguments.
    .cfi_adjust_cfa_offset -32
    movl %edx, %ebx               // Move returned link register for deopt
                                  // (ebx is pretending to be our LR).
    POP eax                       // Restore gpr return value.
    POP edx
    movd (%esp), %xmm0            // Restore fpr return value.
    addl LITERAL(8), %esp
    .cfi_adjust_cfa_offset -8
    RESTORE_REF_ONLY_CALLEE_SAVE_FRAME
    addl LITERAL(4), %esp         // Remove fake return pc.
    jmp   *%ecx                   // Return.
END_FUNCTION art_quick_instrumentation_exit

    /*
     * Instrumentation has requested that we deoptimize into the interpreter. The deoptimization
     * will long jump to the upcall with a special exception of -1.
     */
DEFINE_FUNCTION art_quick_deoptimize
    pushl %ebx                    // Fake that we were called.
    SETUP_SAVE_ALL_CALLEE_SAVE_FRAME
    mov  %esp, %ecx               // Remember SP.
    subl LITERAL(8), %esp         // Align stack.
    .cfi_adjust_cfa_offset 8
    PUSH ecx                      // Pass SP.
    pushl %fs:THREAD_SELF_OFFSET  // Pass Thread::Current().
    .cfi_adjust_cfa_offset 4
    call SYMBOL(artDeoptimize)    // artDeoptimize(Thread*, SP)
    int3                          // Unreachable.
END_FUNCTION art_quick_deoptimize

    /*
     * String's indexOf.
     *
     * On entry:
     *    eax:   string object (known non-null)
     *    ecx:   char to match (known <= 0xFFFF)
     *    edx:   Starting offset in string data
     */
DEFINE_FUNCTION art_quick_indexof
    PUSH edi                      // push callee save reg
    mov STRING_COUNT_OFFSET(%eax), %ebx
    mov STRING_VALUE_OFFSET(%eax), %edi
    mov STRING_OFFSET_OFFSET(%eax), %eax
    testl %edx, %edx              // check if start < 0
    jl   clamp_min
clamp_done:
    cmpl %ebx, %edx               // check if start >= count
    jge  not_found
    lea  STRING_DATA_OFFSET(%edi, %eax, 2), %edi  // build a pointer to the start of string data
    mov  %edi, %eax               // save a copy in eax to later compute result
    lea  (%edi, %edx, 2), %edi    // build pointer to start of data to compare
    subl  %edx, %ebx              // compute iteration count
    /*
     * At this point we have:
     *   eax: original start of string data
     *   ecx: char to compare
     *   ebx: length to compare
     *   edi: start of data to test
     */
    mov  %eax, %edx
    mov  %ecx, %eax               // put char to match in %eax
    mov  %ebx, %ecx               // put length to compare in %ecx
    repne scasw                   // find %ax, starting at [%edi], up to length %ecx
    jne  not_found
    subl %edx, %edi
    sar  LITERAL(1), %edi
    decl %edi                     // index = ((curr_ptr - orig_ptr) / 2) - 1
    mov  %edi, %eax
    POP edi                       // pop callee save reg
    ret
    .balign 16
not_found:
    mov  LITERAL(-1), %eax        // return -1 (not found)
    POP edi                       // pop callee save reg
    ret
clamp_min:
    xor  %edx, %edx               // clamp start to 0
    jmp  clamp_done
END_FUNCTION art_quick_indexof

    /*
     * String's compareTo.
     *
     * On entry:
     *    eax:   this string object (known non-null)
     *    ecx:   comp string object (known non-null)
     */
DEFINE_FUNCTION art_quick_string_compareto
    PUSH esi                    // push callee save reg
    PUSH edi                    // push callee save reg
    mov STRING_COUNT_OFFSET(%eax), %edx
    mov STRING_COUNT_OFFSET(%ecx), %ebx
    mov STRING_VALUE_OFFSET(%eax), %esi
    mov STRING_VALUE_OFFSET(%ecx), %edi
    mov STRING_OFFSET_OFFSET(%eax), %eax
    mov STRING_OFFSET_OFFSET(%ecx), %ecx
    /* Build pointers to the start of string data */
    lea  STRING_DATA_OFFSET(%esi, %eax, 2), %esi
    lea  STRING_DATA_OFFSET(%edi, %ecx, 2), %edi
    /* Calculate min length and count diff */
    mov   %edx, %ecx
    mov   %edx, %eax
    subl  %ebx, %eax
    cmovg %ebx, %ecx
    /*
     * At this point we have:
     *   eax: value to return if first part of strings are equal
     *   ecx: minimum among the lengths of the two strings
     *   esi: pointer to this string data
     *   edi: pointer to comp string data
     */
    repe cmpsw                    // find nonmatching chars in [%esi] and [%edi], up to length %ecx
    jne not_equal
    POP edi                       // pop callee save reg
    POP esi                       // pop callee save reg
    ret
    .balign 16
not_equal:
    movzwl  -2(%esi), %eax        // get last compared char from this string
    movzwl  -2(%edi), %ecx        // get last compared char from comp string
    subl  %ecx, %eax              // return the difference
    POP edi                       // pop callee save reg
    POP esi                       // pop callee save reg
    ret
END_FUNCTION art_quick_string_compareto

    // TODO: implement these!
UNIMPLEMENTED art_quick_memcmp16
