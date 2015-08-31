/*
 * Copyright (C) 2008 The Android Open Source Project
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
/*
 * JNI method invocation.  This is used to call a C/C++ JNI method.  The
 * argument list has to be pushed onto the native stack according to
 * local calling conventions.
 *
 * This version supports 32-bit x86
 */

/*
Function prototype:

void dvmPlatformInvoke(void* pEnv, ClassObject* clazz, int argInfo, int argc,
    const u4* argv, const char* signature, void* func, JValue* pReturn)

The method we are calling has the form:

  return_type func(JNIEnv* pEnv, ClassObject* clazz, ...)
    -or-
  return_type func(JNIEnv* pEnv, Object* this, ...)

We receive a collection of 32-bit values which correspond to arguments from
the interpreter (e.g. float occupies one, double occupies two).  It's up to
us to convert these into local calling conventions.
*/

/*
x86 notes:

The native code expects arguments on the stack, pushed from right to left.
This matches what Dalvik is passing here.

EAX, EDX and ECX are scratch.

4-byte alignment is required for long long and double, so we won't pad

Non-FP return types <= 4 bytes come back in EAX
Non-FP return types of 8 bytes come back in EAX:EDX, with lsw in EAX.
Float and double returned on top of FP stack.

*/

    .text
    .align  4
    .global dvmPlatformInvoke
    .type   dvmPlatformInvoke, @function

/*
 * On entry:
 *  [ 8]  arg0  JNIEnv (can be left alone)
 *  [12]  arg1  clazz (NULL for virtual method calls, non-NULL for static)
 *  [16]  arg2  arg info
 *  [20]  arg3  argc
 *  [24]  arg4  argv
 *  [28]  arg5  short signature
 *  [32]  arg6  func
 *  [36]  arg7  pReturn
 *
 * For a virtual method call, the "this" reference is in argv[0].
 *
 * argInfo (32-bit int) layout:
 *   SRRRZZZZ ZZZZZZZZ AAAAAAAA AAAAAAAA
 *
 *   Z - reserved
 *   S - if set, argInfo hints are invalid
 *   R - return type enumeration (see jniInternal.h)
 *       VOID   -> 0
 *       FLOAT  -> 1
 *       DOUBLE -> 2
 *       S8     -> 3
 *       S4     -> 4
 *   A - size of the variable argument block in 32-bit words
 *
 */
dvmPlatformInvoke:
/* Establish the frame pointer, spill & align to 16b */
    pushl    %ebp
    movl     %esp,%ebp
    pushl    %edi
    pushl    %esi
    pushl    %ebx
    subl     $12,%esp
/* For 386 ABI, argInfo hints should always be valid.  Abort if not. */
    movl     16(%ebp),%ebx
    testl    %ebx,%ebx
    js       dvmAbort
/*
 * Get the size of the variable region, add two more slots for the first
 * two arguments and grow (preserving alignment)
 */
    movl     %ebx,%ecx
    leal     20(,%ecx,4),%ecx
    andl     $0x0003FFF0,%ecx
    subl     %ecx,%esp
/* Handle this/class */
    movl     8(%ebp),%ecx
    movl     12(%ebp),%eax
    movl     24(%ebp),%esi
    testl    %eax,%eax
    jne      isClass
    movl     (%esi),%eax
    addl     $4,%esi
isClass:
    movl     %eax,4(%esp)
    movl     %ecx,0(%esp)
/* Now, copy the variable arguments region */
    movl     %ebx,%ecx
    andl     $0x0000FFFF,%ecx
    leal     8(%esp),%edi
    cld
    rep
    movsd
/* Ready to go - call the native code */
    call     *32(%ebp)
/* Store the result. */
    sarl      $28,%ebx
    /* Is void? */
    testl     %ebx,%ebx
    je       cleanUpAndExit
    movl     36(%ebp),%ecx
    /* Is FP? */
    cmpl     $2,%ebx
    jle      isFP
    cmpl     $4,%ebx  /* smaller than 32-bits? */
    jg       isSmall
storeRetval:
    /* Blindly storing 64-bits won't hurt 32-bit case */
    movl     %eax,(%ecx)
    movl     %edx,4(%ecx)
    jmp      cleanUpAndExit
isSmall:
    cmpl     $7,%ebx  /* S1? */
    jne      checkShort
    movsbl   %al,%eax
    movl     %eax,(%ecx)
    jmp      cleanUpAndExit
checkShort:
    cmpl     $6,%ebx  /* U2? */
    jne      isSignedShort
    movzwl   %ax,%eax
    movl     %eax,(%ecx)
    jmp      cleanUpAndExit
isSignedShort:
    /* Must be S2 */
    movswl   %ax,%eax
    movl     %eax,(%ecx)
    jmp      cleanUpAndExit
isFP:
    /* Is Float? */
    cmpl    $1,%ebx
    je       saveFloat
    fstpl    (%ecx)
    jmp      cleanUpAndExit
saveFloat:
    fstps    (%ecx)
cleanUpAndExit:
    leal     -12(%ebp),%esp
    pop      %ebx
    pop      %esi
    pop      %edi
    pop      %ebp
    ret
    .size    dvmPlatformInvoke, .-dvmPlatformInvoke
    .section .note.GNU-stack,"",@progbits
