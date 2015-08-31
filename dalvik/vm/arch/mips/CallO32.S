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
 * This version supports the MIPS O32 ABI.
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

Please notice that argc in dvmPlatformInvoke does NOT include pEnv and clazz/this.
*/

    .text
    .align  2
    .globl dvmPlatformInvoke
    .ent dvmPlatformInvoke
/*
 * On entry:
 *   a0  JNIEnv (can be left alone)
 *   a1  clazz (NULL for virtual method calls, non-NULL for static)
 *   a2  argInfo
 *   a3  argc (number of 32-bit values in argv)
 *   MIPS reservers 16 bytes on stack even if the first 4 args are passed by
 *   reg a0-a3. That's different from ARM.
 *   [sp + 16]  argv
 *   [sp + 20]  short signature
 *   [sp + 24]  func
 *   [sp + 28]  pReturn
 *
 * For a virtual method call, the "this" reference is in argv[0].
 *
 * argInfo (32-bit int) layout:
 *   SRRRLLLL FFFFFFFF FFFFFFFF FFFFFFFF
 *
 *   S - if set, do things the hard way (scan the signature)
 *   R - return type enumeration, really only important for hardware FP
 *   L - number of double-words (64 bits!) of storage required on stack (0-30 words)
 *   F - pad flag -- if set, write a pad word to the stack
 *
 * With this arrangement we can efficiently push up to 24 words of arguments
 * onto the stack.  Anything requiring more than that -- which should happen
 * rarely to never -- can do the slow signature scan.
 *
 * (We could pack the Fs more efficiently -- we know we never push two pads
 * in a row, and the first word can never be a pad -- but there's really
 * no need for it.)
 *
 * NOTE: if the called function has more than 4 words of arguments, gdb
 * will not be able to unwind the stack past this method.  The only way
 * around this is to convince gdb to respect an explicit frame pointer.
 */

 /* Stack:
  *                     High
  *                 ____________
  *                 |__28______| pReturn
  *                 |__24______| func
  *                 |__20______| short signature
  *                 |__16______| argv
  *                 |__12______| reserved (a3: argc)
  *                 |__8_______| reserved (a2: arg)
  *                 |__4_______| reserved (a1: clazz)
  *__sp on entry_->_|__0_______|_reserved (a0: JNIenv)
  *                 |__________| saved ra
  *                 |__________| saved fp
  *                 |__________| saved s0
  *                 |__________| spare
  *                 |__________| saved s2
  *"framepointer"->_|__________| pad for 8 bytes aligned
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| other argv or pad
  *                 |__________| reserved for a3
  *                 |__________| reserved for a2
  *                 |__________| reserved for a1
  *_____new sp___-> |__________| reserved for a0
  * (new sp: sp when call native method)
  */

 /* Register usage:
  *
  *  s0: pReturn
  *  s2: Return type
  * These registers should be saved to and restored from stack.
  *
  *  t0: argv
  *  t9: func
  * These registers do not need to be saved.
  *
  * We put the stack size into register s1 because we can not know the size
  * of stack at the beginning. This size can be calculated with the help
  * of hints in jniarginfo.
  *
  */

dvmPlatformInvoke:
	.set noreorder
	.cpload $t9
	.set reorder

	/*  Do we have arg padding flags in "argInfo"? Check bit 31 */
	bltz	$a2,.Lno_arginfo

	/* Fast path. We have hints. */
	/* save fp and ra to stack */
#define FSIZE 24
	subu	$sp,FSIZE
	sw	$ra,20($sp)
	sw	$fp,16($sp)
	sw	$s0,12($sp)
	sw	$s2,4($sp)
	move	$fp,$sp

	lw	$t0,FSIZE+16($sp)	/* t0 <- argv */
	lw	$t9,FSIZE+24($sp)	/* t9 <- func */
	lw	$s0,FSIZE+28($sp)	/* s0 <- pReturn */

	/* Is the method static? */
	bnez	$a1,1f
	/* Not static: a1 <- *argv++ ("this"), argc-- */
	lw	$a1,($t0)
	addiu	$t0,4
	addiu	$a3,-1
1:
	/* expand the stack for args */
	srl	$s2,$a2,28	/* s2 <- returnType */
	srl	$t1,$a2,21
	andi	$t1,0x78	/* t1 <- stackSize in bytes */

	addiu	$t1,16		/* include space for a0/a1/a2/a3 */
	subu	$sp,$t1
	addiu	$t1,$sp,8

	/*
	 * t0 :argv
	 * t1 :sp+8(first arg position in stack except pEnv and clazz/this)
	 * a2 :argInfo
	 * a3 :argc
	 * sp :new stack bottom
	 */

	/* first two args or one args and pad */
	blez	$a3,.Largs_done
	lw	$t2,($t0)
	addiu	$t0,4
	addiu	$a3,-1
	sw	$t2,($t1)
	addiu	$t1,4
	srl	$a2,1
	blez	$a3,.Largs_done

	andi	$t3,$a2,0x1	/* the second position is a pad? */
	bnez	$t3,.Lpad0

	lw	$t2,($t0)
	addiu	$t0,4
	addiu	$a3,-1
	sw	$t2,($t1)
.Lpad0:
	addiu	$t1,4
	srl	$a2,1
	blez	$a3,.Largs_done

.Lloop1:
	/* copy other args
	 * $fp: sp top for args
	 * $t1: sp for next arg
	 */
	beq	$t1,$fp,.Largs_done
	andi	$t3,$a2,0x1
	srl	$a2,1
	bnez	$t3,.Lpad
	lw	$t2,($t0)
	addiu	$t0,4
	sw	$t2,($t1)
.Lpad:
	addiu	$t1,4
	b	.Lloop1

.Largs_done:

	/*
	 * We have copied args into stacks. Then copy argv[0]/argv[1] into
	 * reg a2/a3. You may find that if argv[0] is 32 bits and argv[1]
	 * is 64 bits, then we do not need to set reg a3 since it is a pad.
	 * However, copy a3 from argv is harmless. We do not need to set
	 * a0(pEnv)/a1(clazz/this) since they are already there.
	 */

	/*
	 * sp: new stack
	 * s0: pReturn
	 * s2: Return type
	 *
	 */
	lw	$a2,8($sp)
	lw	$a3,12($sp)

	/* Linux/PIC needs $t9 points to function address.
	 * call the function
	 */
	jalr $t9

	/* function call return */
	/* 1. check the return type
	 * 2. if the return type is not DALVIK_JNI_RETURN_VOID then copy v0/v1
	 *    to pReturn
	 */
	beqz	$s2,.Lend	/* don't set result if return type is void */

#ifdef __mips_hard_float
	mfc1	$t0,$f0		/* Get float ($f0) or double ($f1$f0) result */
	mfc1	$t1,$f1
	sltiu	$t2,$s2,3	/* set t2 if return type is float or double */
#ifdef HAVE_LITTLE_ENDIAN
        /* Note: for little endian, the double result is in $v1:$v0 and float result is in $v0 */
	movn	$v0,$t0,$t2	/* If the result type is float or double overwrite $v1/$v0 */
	movn	$v1,$t1,$t2
#else
        /* Note: for big endian, the double result is in $v0:$v1 and float result is in $v0 */
	movn	$v1,$t0,$t2	/* If the result type is float or double overwrite $v0/$v1 */
	movn	$v0,$t1,$t2
	sltiu	$t3,$s2,2	/* set t3 if return type is float */
	movn	$v0,$t0,$t3	/* If the result type is float overwrite $v0 */
#endif
#endif

	/* Store the result */
	sw	$v0,0($s0)
	sw	$v1,4($s0)

.Lend:
	/* restore saved registers */
	move	$sp,$fp
	lw	$ra,20($sp)
	lw	$fp,16($sp)
	lw	$s0,12($sp)
	lw	$s2,4($sp)
	addiu	$sp,FSIZE
	jr	$ra

/* Slow path - just tail call the generic routine */
.Lno_arginfo:

	la $t9,dvmPlatformInvokeFFI
	j $t9

.end dvmPlatformInvoke
