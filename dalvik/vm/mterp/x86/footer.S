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
 * Common subroutines and data.
 */

#if defined(WITH_JIT)
/*
 * JIT-related re-entries into the interpreter.  In general, if the
 * exit from a translation can at some point be chained, the entry
 * here requires that control arrived via a call, and that the "rp"
 * on TOS is actually a pointer to a 32-bit cell containing the Dalvik PC
 * of the next insn to handle.  If no chaining will happen, the entry
 * should be reached via a direct jump and rPC set beforehand.
 */

    .global dvmJitToInterpPunt
/*
 * The compiler will generate a jump to this entry point when it is
 * having difficulty translating a Dalvik instruction.  We must skip
 * the code cache lookup & prevent chaining to avoid bouncing between
 * the interpreter and code cache. rPC must be set on entry.
 */
dvmJitToInterpPunt:
    GET_PC
#if defined(WITH_JIT_TUNING)
    movl   rPC, OUT_ARG0(%esp)
    call   dvmBumpPunt
#endif
    movl   rSELF, %ecx
    movl   offThread_curHandlerTable(%ecx),rIBASE
    movl        $$0, offThread_inJitCodeCache(%ecx)
    FETCH_INST_R %ecx
    GOTO_NEXT_R %ecx

    .global dvmJitToInterpSingleStep
/*
 * Return to the interpreter to handle a single instruction.
 * Should be reached via a call.
 * On entry:
 *   0(%esp)          <= native return address within trace
 *   rPC              <= Dalvik PC of this instruction
 *   OUT_ARG0+4(%esp) <= Dalvik PC of next instruction
 */
dvmJitToInterpSingleStep:
/* TODO */
    call     dvmAbort
#if 0
    pop    %eax
    movl   rSELF, %ecx
    movl   OUT_ARG0(%esp), %edx
    movl   %eax,offThread_jitResumeNPC(%ecx)
    movl   %edx,offThread_jitResumeDPC(%ecx)
    movl   $$kInterpEntryInstr,offThread_entryPoint(%ecx)
    movl   $$1,rINST     # changeInterp <= true
    jmp    common_gotoBail
#endif

    .global dvmJitToInterpNoChainNoProfile
/*
 * Return from the translation cache to the interpreter to do method
 * invocation.  Check if the translation exists for the callee, but don't
 * chain to it. rPC must be set on entry.
 */
dvmJitToInterpNoChainNoProfile:
#if defined(WITH_JIT_TUNING)
    SPILL_TMP1(%eax)
    call   dvmBumpNoChain
    UNSPILL_TMP1(%eax)
#endif
    movl   %eax, rPC
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   %eax,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread  # (pc, self)
    movl   rSELF,%ecx                # ecx <- self
    movl   %eax,offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    cmpl   $$0, %eax
    jz     1f
    jmp    *%eax                     # exec translation if we've got one
    # won't return
1:
    EXPORT_PC
    movl   rSELF, %ecx
    movl   offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_R %ecx
    GOTO_NEXT_R %ecx

/*
 * Return from the translation cache and immediately request a
 * translation from the exit target, but don't attempt to chain.
 * rPC set on entry.
 */
    .global dvmJitToInterpTraceSelectNoChain
dvmJitToInterpTraceSelectNoChain:
#if defined(WITH_JIT_TUNING)
    movl   %edx, OUT_ARG0(%esp)
    call   dvmBumpNoChain
#endif
    movl   %ebx, rPC
    lea    4(%esp), %esp #to recover the esp update due to function call
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   %eax,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread  # (pc, self)
    movl   rSELF,%ecx
    cmpl   $$0,%eax
    movl   %eax,offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    jz     1f
    jmp    *%eax              # jump to tranlation
    # won't return

/* No Translation - request one */
1:
    GET_JIT_PROF_TABLE %ecx %eax
    cmpl   $$0, %eax          # JIT enabled?
    jnz    2f                 # Request one if so
    movl   rSELF, %ecx
    movl   offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST_R %ecx         # Continue interpreting if not
    GOTO_NEXT_R %ecx
2:
    ## Looks like an EXPORT_PC is needed here. Now jmp to common_selectTrace2
    movl   $$kJitTSelectRequestHot,%eax # ask for trace select
    jmp    common_selectTrace

/*
 * Return from the translation cache and immediately request a
 * translation for the exit target.  Reached via a call, and
 * (TOS)->rPC.
 */
    .global dvmJitToInterpTraceSelect
dvmJitToInterpTraceSelect:
    movl   0(%esp), %eax          # get return address
    movl   %ebx, rPC              # get first argument (target rPC)

    ## TODO, need to clean up stack manipulation ... this isn't signal safe and
    ## doesn't use the calling conventions of header.S
    lea    4(%esp), %esp #to recover the esp update due to function call

    ## An additional 5B instruction "jump 0" was added for a thread-safe
    ## chaining cell update in JIT code cache. So the offset is now -17=-12-5.
    lea    -17(%eax), %ebx #$$JIT_OFFSET_CHAIN_START(%eax), %ebx
    lea    -4(%esp), %esp
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   %eax,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread # (pc, self)
    lea    4(%esp), %esp
    cmpl   $$0,%eax
    movl   rSELF, %ecx
    movl   %eax,offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    jz     1b                 # no - ask for one
    movl   %eax,OUT_ARG0(%esp)
    movl   rINST,OUT_ARG1(%esp)
    call   dvmJitChain        # Attempt dvmJitChain(codeAddr,chainAddr)
    cmpl   $$0,%eax           # Success?
    jz     toInterpreter      # didn't chain - interpret
    jmp    *%eax
    # won't return

/*
 * Placeholder entries for x86 JIT
 */
    .global dvmJitToInterpBackwardBranch
dvmJitToInterpBackwardBranch:

    .global     dvmJitToExceptionThrown
dvmJitToExceptionThrown: //rPC in
    movl   rSELF, %edx
    GET_PC
    movl   $$0, offThread_inJitCodeCache(%edx)
    jmp common_exceptionThrown

    .global dvmJitToInterpNormal
dvmJitToInterpNormal:
/* one input: the target rPC value */
    movl        0(%esp), %eax          # get return address
    movl        %ebx, rPC              # get first argument (target rPC)

    ## TODO, need to clean up stack manipulation ... this isn't signal safe and
    ## doesn't use the calling conventions of header.S

    ## An additional 5B instruction "jump 0" was added for a thread-safe
    ## chaining cell update in JIT code cache. So the offset is now -17=-12-5.
    lea         -17(%eax), %ebx #$$JIT_OFFSET_CHAIN_START(%eax), %ebx
    lea         4(%esp), %esp
    movl        rPC, OUT_ARG0(%esp)
    movl        rSELF, %ecx
    movl        %ecx, OUT_ARG1(%esp)
    call        dvmJitGetTraceAddrThread
    ## Here is the change from using rGLUE to rSELF for accessing the
    ## JIT code cache flag
    movl        rSELF, %ecx
    movl        %eax, offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    #lea         4(%esp), %esp
    cmp         $$0, %eax
    je          toInterpreter
    #lea         -8(%esp), %esp
    movl        %ebx, OUT_ARG1(%esp)    # %ebx live thorugh dvmJitGetTraceAddrThread
    movl        %eax, OUT_ARG0(%esp)    # first argument
    call        dvmJitChain
    #lea         8(%esp), %esp
    cmp         $$0, %eax
    je          toInterpreter
    jmp         *%eax                   #to native address

    .global dvmJitToInterpNoChain
dvmJitToInterpNoChain:
dvmJitToInterpNoChain: #rPC in eax
#if defined(WITH_JIT_TUNING)
    SPILL_TMP1(%eax)
    call   dvmBumpNoChain
    UNSPILL_TMP1(%eax)
#endif
    ## TODO, need to clean up stack manipulation ... this isn't signal safe and
    ## doesn't use the calling conventions of header.S
    movl        %eax, rPC
    movl        rPC, OUT_ARG0(%esp)
    movl        rSELF, %ecx
    movl        %ecx, OUT_ARG1(%esp)
    call        dvmJitGetTraceAddrThread
    ## Here is the change from using rGLUE to rSELF for accessing the
    ## JIT code cache flag
    movl        rSELF, %ecx
    movl        %eax, offThread_inJitCodeCache(%ecx)  # set inJitCodeCache flag
    cmp         $$0, %eax
    je          toInterpreter
    jmp         *%eax                   #to native address

toInterpreter:
    EXPORT_PC
    movl        rSELF, %ecx
    movl        offThread_curHandlerTable(%ecx), rIBASE
    FETCH_INST
    movl        offThread_pJitProfTable(%ecx), %eax
    #Fallthrough

/* ebx holds the pointer to the jit profile table
   edx has the opCode */
common_testUpdateProfile:
    cmp         $$0, %eax
    je          4f
/* eax holds the pointer to the jit profile table
   edx has the opCode
   rPC points to the next bytecode */

common_updateProfile:
    # quick & dirty hash
    movl   rPC, %ecx
    shrl   $$12, %ecx
    xorl   rPC, %ecx
    andl   $$((1<<JIT_PROF_SIZE_LOG_2)-1), %ecx
    decb   (%ecx,%eax)
    #jmp    1f # remove
    jz     2f
1:
    GOTO_NEXT
2:
common_Profile:
/*
 * Here, we switch to the debug interpreter to request
 * trace selection.  First, though, check to see if there
 * is already a native translation in place (and, if so,
 * jump to it now.
 */
    SPILL(rIBASE)
    SPILL_TMP1(rINST)
    movl        rSELF, rIBASE
    GET_JIT_THRESHOLD rIBASE rINST  # leaves rSELF in %ecx
    EXPORT_PC
    movb   rINSTbl,(%ecx,%eax)   # reset counter
    movl   rIBASE,rINST            # preserve rSELF
    movl   rSELF, %eax
    movl   rPC,OUT_ARG0(%esp)
    movl   rIBASE,OUT_ARG1(%esp)
    call   dvmJitGetTraceAddrThread  # (pc, self)
    UNSPILL(rIBASE)
    movl   %eax,offThread_inJitCodeCache(rINST)   # set the inJitCodeCache flag
    UNSPILL_TMP1(rINST)
    cmpl   $$0,%eax
    #jmp    1f # remove
    jz     1f
    jmp   *%eax        # TODO: decide call vs/ jmp!.  No return either way
1:
    movl   $$kJitTSelectRequest,%eax
    # On entry, eax<- jitState, rPC valid
common_selectTrace:
    mov         %ebx, EBX_SPILL(%ebp)
    movl        rSELF, %ebx
    movzwl      offThread_subMode(%ebx), %ecx
    and         $$(kSubModeJitTraceBuild | kSubModeJitSV), %ecx
    jne         3f                     # already doing JIT work, continue
    movl        %eax, offThread_jitState(%ebx)
    movl        rSELF, %eax
    movl       %eax, OUT_ARG0(%esp)

/*
 * Call out to validate trace-building request. If successful, rIBASE will be swapped
 * to send us into single-steppign trace building mode, so we need to refresh before
 * we continue.
 */

   EXPORT_PC
   SAVE_PC_FP_TO_SELF %ecx
   call dvmJitCheckTraceRequest
3:
   mov          EBX_SPILL(%ebp), %ebx
   FETCH_INST
   movl rSELF, %ecx
   movl offThread_curHandlerTable(%ecx), rIBASE
4:
   GOTO_NEXT

common_selectTrace2:
    mov         %ebx, EBX_SPILL(%ebp)
    movl        rSELF, %ebx
    movl        %ebx, OUT_ARG0(%esp)
    movl        %eax, offThread_jitState(%ebx)
    movzwl      offThread_subMode(%ebx), %ecx
    mov         EBX_SPILL(%ebp), %ebx
    and         (kSubModeJitTraceBuild | kSubModeJitSV), %ecx
    jne         3f                     # already doing JIT work, continue



/*
 * Call out to validate trace-building request. If successful, rIBASE will be swapped
 * to send us into single-steppign trace building mode, so we need to refresh before
 * we continue.
 */

   EXPORT_PC
   SAVE_PC_FP_TO_SELF %ecx
   call dvmJitCheckTraceRequest
3:
   FETCH_INST
   movl rSELF, %ecx
   movl offThread_curHandlerTable(%ecx), rIBASE
4:
   GOTO_NEXT

#endif

/*
 * For the invoke codes we need to know what register holds the "this" pointer. However
 * it seems the this pointer is assigned consistently most times it is in %ecx but other
 * times it is in OP_INVOKE_INTERFACE, OP_INVOKE_SUPER_QUICK, or OP_INVOKE_VIRTUAL_QUICK.
*/

/*
 * Common code for method invocation with range.
 *
 * On entry:
 *   eax = Method* methodToCall
 *   ecx = "this"
 *   rINSTw trashed, must reload
 *   rIBASE trashed, must reload before resuming interpreter
 */

common_invokeMethodRange:
.LinvokeNewRange:
#if defined(WITH_JIT)
    SPILL_TMP1(%edx)
    SPILL_TMP2(%ebx)
    movl        rSELF, %edx
    movzwl      offThread_subMode(%edx), %ebx
    and         $$kSubModeJitTraceBuild, %ebx
    jz          6f
    call        save_callsiteinfo
6:
    UNSPILL_TMP2(%ebx)
    UNSPILL_TMP1(%edx)
#endif
   /*
    * prepare to copy args to "outs" area of current frame
    */

    movzbl      1(rPC),rINST       # rINST<- AA
    movzwl      4(rPC), %ecx            # %ecx<- CCCC
    SAVEAREA_FROM_FP %edx               # %edx<- &StackSaveArea
    test        rINST, rINST
    movl        rINST, LOCAL0_OFFSET(%ebp) # LOCAL0_OFFSET(%ebp)<- AA
    jz          .LinvokeArgsDone        # no args; jump to args done


   /*
    * %eax=methodToCall, %ecx=CCCC, LOCAL0_OFFSET(%ebp)=count,
    * %edx=&outs (&stackSaveArea).  (very few methods have > 10 args;
    * could unroll for common cases)
    */

.LinvokeRangeArgs:
    movl        %ebx, LOCAL1_OFFSET(%ebp)       # LOCAL1_OFFSET(%ebp)<- save %ebx
    lea         (rFP, %ecx, 4), %ecx    # %ecx<- &vCCCC
    shll        $$2, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET(%ebp)<- offset
    subl        LOCAL0_OFFSET(%ebp), %edx       # %edx<- update &outs
    shrl        $$2, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET(%ebp)<- offset
1:
    movl        (%ecx), %ebx            # %ebx<- vCCCC
    lea         4(%ecx), %ecx           # %ecx<- &vCCCC++
    subl        $$1, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET<- LOCAL0_OFFSET--
    movl        %ebx, (%edx)            # *outs<- vCCCC
    lea         4(%edx), %edx           # outs++
    jne         1b                      # loop if count (LOCAL0_OFFSET(%ebp)) not zero
    movl        LOCAL1_OFFSET(%ebp), %ebx       # %ebx<- restore %ebx
    jmp         .LinvokeArgsDone        # continue

   /*
    * %eax is "Method* methodToCall", the method we're trying to call
    * prepare to copy args to "outs" area of current frame
    * rIBASE trashed, must reload before resuming interpreter
    */

common_invokeMethodNoRange:
#if defined(WITH_JIT)
    SPILL_TMP1(%edx)
    SPILL_TMP2(%ebx)
    movl        rSELF, %edx
    movzwl      offThread_subMode(%edx), %ebx
    and         $$kSubModeJitTraceBuild, %ebx
    jz          6f
    call        save_callsiteinfo
6:
    UNSPILL_TMP2(%ebx)
    UNSPILL_TMP1(%edx)
#endif
.LinvokeNewNoRange:
    movzbl      1(rPC),rINST       # rINST<- BA
    movl        rINST, LOCAL0_OFFSET(%ebp) # LOCAL0_OFFSET(%ebp)<- BA
    shrl        $$4, LOCAL0_OFFSET(%ebp)        # LOCAL0_OFFSET(%ebp)<- B
    je          .LinvokeArgsDone        # no args; jump to args done
    movzwl      4(rPC), %ecx            # %ecx<- GFED
    SAVEAREA_FROM_FP %edx               # %edx<- &StackSaveArea

   /*
    * %eax=methodToCall, %ecx=GFED, LOCAL0_OFFSET(%ebp)=count, %edx=outs
    */

.LinvokeNonRange:
    cmp         $$2, LOCAL0_OFFSET(%ebp)        # compare LOCAL0_OFFSET(%ebp) to 2
    movl        %ecx, LOCAL1_OFFSET(%ebp)       # LOCAL1_OFFSET(%ebp)<- GFED
    jl          1f                      # handle 1 arg
    je          2f                      # handle 2 args
    cmp         $$4, LOCAL0_OFFSET(%ebp)        # compare LOCAL0_OFFSET(%ebp) to 4
    jl          3f                      # handle 3 args
    je          4f                      # handle 4 args
5:
    andl        $$15, rINST             # rINSTw<- A
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, rINST, 4), %ecx   # %ecx<- vA
    movl        %ecx, (%edx)            # *outs<- vA
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
4:
    shr         $$12, %ecx              # %ecx<- G
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vG
    movl        %ecx, (%edx)            # *outs<- vG
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
3:
    and         $$0x0f00, %ecx          # %ecx<- 0F00
    shr         $$8, %ecx               # %ecx<- F
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vF
    movl        %ecx, (%edx)            # *outs<- vF
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
2:
    and         $$0x00f0, %ecx          # %ecx<- 00E0
    shr         $$4, %ecx               # %ecx<- E
    lea         -4(%edx), %edx          # %edx<- update &outs; &outs--
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vE
    movl        %ecx, (%edx)            # *outs<- vE
    movl        LOCAL1_OFFSET(%ebp), %ecx       # %ecx<- GFED
1:
    and         $$0x000f, %ecx          # %ecx<- 000D
    movl        (rFP, %ecx, 4), %ecx    # %ecx<- vD
    movl        %ecx, -4(%edx)          # *--outs<- vD
0:

   /*
    * %eax is "Method* methodToCall", the method we're trying to call
    * find space for the new stack frame, check for overflow
    */

.LinvokeArgsDone:
    movzwl      offMethod_registersSize(%eax), %edx # %edx<- methodToCall->regsSize
    movzwl      offMethod_outsSize(%eax), %ecx # %ecx<- methodToCall->outsSize
    movl        %eax, LOCAL0_OFFSET(%ebp)       # LOCAL0_OFFSET<- methodToCall
    shl         $$2, %edx               # %edx<- update offset
    SAVEAREA_FROM_FP %eax               # %eax<- &StackSaveArea
    subl        %edx, %eax              # %eax<- newFP; (old savearea - regsSize)
    movl        rSELF,%edx              # %edx<- pthread
    movl        %eax, LOCAL1_OFFSET(%ebp)       # LOCAL1_OFFSET(%ebp)<- &outs
    subl        $$sizeofStackSaveArea, %eax # %eax<- newSaveArea (stack save area using newFP)
    movl        offThread_interpStackEnd(%edx), %edx # %edx<- self->interpStackEnd
    movl        %edx, TMP_SPILL1(%ebp)  # spill self->interpStackEnd
    shl         $$2, %ecx               # %ecx<- update offset for outsSize
    movl        %eax, %edx              # %edx<- newSaveArea
    sub         %ecx, %eax              # %eax<- bottom; (newSaveArea - outsSize)
    cmp         TMP_SPILL1(%ebp), %eax  # compare interpStackEnd and bottom
    movl        LOCAL0_OFFSET(%ebp), %eax       # %eax<- restore methodToCall
    jl          .LstackOverflow         # handle frame overflow

   /*
    * set up newSaveArea
    */

#ifdef EASY_GDB
    SAVEAREA_FROM_FP %ecx               # %ecx<- &StackSaveArea
    movl        %ecx, offStackSaveArea_prevSave(%edx) # newSaveArea->prevSave<- &outs
#endif
    movl        rSELF,%ecx              # %ecx<- pthread
    movl        rFP, offStackSaveArea_prevFrame(%edx) # newSaveArea->prevFrame<- rFP
    movl        rPC, offStackSaveArea_savedPc(%edx) # newSaveArea->savedPc<- rPC
#if defined(WITH_JIT)
    movl        $$0, offStackSaveArea_returnAddr(%edx)
#endif

    /* Any special actions to take? */
    cmpw        $$0, offThread_subMode(%ecx)
    jne         2f                     # Yes - handle them
1:
    testl       $$ACC_NATIVE, offMethod_accessFlags(%eax) # check for native call
    movl        %eax, offStackSaveArea_method(%edx) # newSaveArea->method<- method to call
    jne         .LinvokeNative          # handle native call

   /*
    * Update "self" values for the new method
    * %eax=methodToCall, LOCAL1_OFFSET(%ebp)=newFp
    */
    movl        offMethod_clazz(%eax), %edx # %edx<- method->clazz
    movl        offClassObject_pDvmDex(%edx), %edx # %edx<- method->clazz->pDvmDex
    movl        %eax, offThread_method(%ecx) # self->method<- methodToCall
    movl        %edx, offThread_methodClassDex(%ecx) # self->methodClassDex<- method->clazz->pDvmDex
    movl        offMethod_insns(%eax), rPC # rPC<- methodToCall->insns
    movl        $$1, offThread_debugIsMethodEntry(%ecx)
    movl        LOCAL1_OFFSET(%ebp), rFP # rFP<- newFP
    movl        rFP, offThread_curFrame(%ecx) # curFrame<-newFP
    movl        offThread_curHandlerTable(%ecx),rIBASE
    FETCH_INST
#if defined(WITH_JIT)
    /* rPC is already updated */
    GET_JIT_PROF_TABLE %ecx %eax
    cmp         $$0, %eax
    jne         common_updateProfile # set up %ebx & %edx & rPC
#endif
    GOTO_NEXT                           # jump to methodToCall->insns

2:
    /*
     * On entry, preserve all:
     *  %eax: method
     *  %ecx: self
     *  %edx: new save area
     */
    SPILL_TMP1(%eax)                   # preserve methodToCall
    SPILL_TMP2(%edx)                   # preserve newSaveArea
    movl        rPC, offThread_pc(%ecx) # update interpSave.pc
    movl        %ecx, OUT_ARG0(%esp)
    movl        %eax, OUT_ARG1(%esp)
    call        dvmReportInvoke        # (self, method)
    UNSPILL_TMP1(%eax)
    UNSPILL_TMP2(%edx)
    movl        rSELF,%ecx             # restore rSELF
    jmp         1b

   /*
    * Prep for the native call
    * %eax=methodToCall, LOCAL1_OFFSET(%ebp)=newFP, %edx=newSaveArea, %ecx=self
    */

.LinvokeNative:
    movl        offThread_jniLocal_topCookie(%ecx), rINST # rINST<- self->localRef->...
    movl        rINST, offStackSaveArea_localRefCookie(%edx) # newSaveArea->localRefCookie<- top
    movl        %edx, LOCAL2_OFFSET(%ebp)  # save newSaveArea
    movl        LOCAL1_OFFSET(%ebp), rINST # rINST<- newFP
    movl        rINST, offThread_curFrame(%ecx)  # curFrame<- newFP
    cmpw        $$0, offThread_subMode(%ecx)  # Anything special going on?
    jne         11f                     # yes - handle it
    movl        %ecx, OUT_ARG3(%esp)    # push parameter self
    movl        %eax, OUT_ARG2(%esp)    # push parameter methodToCall
    lea         offThread_retval(%ecx), %ecx # %ecx<- &retval
    movl        %ecx, OUT_ARG1(%esp)    # push parameter &retval
    movl        rINST, OUT_ARG0(%esp)    # push parameter newFP
    call        *offMethod_nativeFunc(%eax) # call methodToCall->nativeFunc
7:
    movl        LOCAL2_OFFSET(%ebp), %ecx    # %ecx<- newSaveArea
    movl        rSELF, %eax             # %eax<- self
    movl        offStackSaveArea_localRefCookie(%ecx), %edx # %edx<- old top
    cmp         $$0, offThread_exception(%eax) # check for exception
    movl        rFP, offThread_curFrame(%eax) # curFrame<- rFP
    movl        %edx, offThread_jniLocal_topCookie(%eax) # new top <- old top
    jne         common_exceptionThrown  # handle exception
    movl        offThread_curHandlerTable(%eax),rIBASE
    FETCH_INST_OPCODE 3 %ecx
    ADVANCE_PC 3
    GOTO_NEXT_R %ecx                    # jump to next instruction

11:
    /*
     * Handle any special subMode actions
     * %eax=methodToCall, rINST=newFP, %ecx=self
     */
    SPILL_TMP1(%eax)                    # save methodTocall
    movl        rPC, offThread_pc(%ecx)
    movl        %ecx, OUT_ARG1(%esp)
    movl        %eax, OUT_ARG0(%esp)
    movl        rFP, OUT_ARG2(%esp)
    call        dvmReportPreNativeInvoke # (methodToCall, self, fp)
    UNSPILL_TMP1(%eax)                  # restore methodToCall
    movl        rSELF,%ecx              # restore self

    /* Do the native call */
    movl        %ecx, OUT_ARG3(%esp)    # push parameter self
    lea         offThread_retval(%ecx), %ecx # %ecx<- &retval
    movl        %eax, OUT_ARG2(%esp)    # push parameter methodToCall
    movl        %ecx, OUT_ARG1(%esp)    # push parameter &retval
    movl        rINST, OUT_ARG0(%esp)   # push parameter newFP
    call        *offMethod_nativeFunc(%eax) # call methodToCall->nativeFunc

    UNSPILL_TMP1(%eax)                  # restore methodToCall
    movl        rSELF, %ecx
    movl        %ecx, OUT_ARG1(%esp)
    movl        %eax, OUT_ARG0(%esp)
    movl        rFP, OUT_ARG2(%esp)
    call        dvmReportPostNativeInvoke # (methodToCall, self, fp)
    jmp         7b                      # rejoin

.LstackOverflow:    # eax=methodToCall
    movl        %eax, OUT_ARG1(%esp)    # push parameter methodToCall
    movl        rSELF,%eax              # %eax<- self
    movl        %eax, OUT_ARG0(%esp)    # push parameter self
    call        dvmHandleStackOverflow  # call: (Thread* self, Method* meth)
    jmp         common_exceptionThrown  # handle exception


/*
 * Common code for handling a return instruction
 */
common_returnFromMethod:
    movl    rSELF, %ecx
    SAVEAREA_FROM_FP %eax                       # %eax<- saveArea(old)
    cmpw    $$0, offThread_subMode(%ecx)          # special action needed?
    jne     19f                                   # go if so
14:

    movl        offStackSaveArea_prevFrame(%eax), rFP # rFP<- saveArea->PrevFrame
    movl        (offStackSaveArea_method - sizeofStackSaveArea)(rFP), rINST # rINST<- method we are returning to
    cmpl        $$0, rINST               # check for break frame
    je          common_gotoBail         # bail if break frame
    movl        offThread_curHandlerTable(%ecx),rIBASE
    movl        offStackSaveArea_savedPc(%eax), rPC # rPC<- saveAreaOld->savedPc
#if defined(WITH_JIT)
    movl        offStackSaveArea_returnAddr(%eax), %ecx
#endif
    movl        rSELF, %eax
    movl        rINST, offThread_method(%eax) # glue->method<- newSave->method
    movl        offMethod_clazz(rINST), rINST # rINST<- method->clazz
    movl        rFP, offThread_curFrame(%eax) # glue->self->curFrame<- rFP
#if defined(WITH_JIT)
    //update self->offThread_inJitCodeCache
    movl        %ecx, offThread_inJitCodeCache(%eax)
#endif
    movl        offClassObject_pDvmDex(rINST), rINST # rINST<- method->clazz->pDvmDex
    movl        rINST, offThread_methodClassDex(%eax) # glue->pDvmDex<- method->clazz->pDvmDex
#if defined(WITH_JIT)
    cmp         $$0, %ecx
    je          .returnToBC
    movl        %ecx, %eax
    jmp         *%eax
#endif

.returnToBC:

#if defined(WITH_JIT)
    FETCH_INST_OPCODE  3, %ecx                 # %eax<- next instruction hi; fetch, advance
    // %ecx has the opcode
    addl         $$6, rPC               # 3*2 = 6
    SPILL_TMP1   (%ecx)
    movl         rSELF, %ecx
    FETCH_INST
    UNSPILL_TMP1   (%ecx)
    movzbl      1(rPC), rINST
    jmp     *(rIBASE,%ecx,4)
#else
    FETCH_INST_WORD 3
    ADVANCE_PC 3
    GOTO_NEXT
#endif

19:
    /*
     * Handle special subMode actions
     * On entry, rFP: prevFP, %ecx: self, %eax: saveArea
     */
    SPILL_TMP1(%ebx)
    movl     offStackSaveArea_prevFrame(%eax), %ebx # %ebx<- saveArea->PrevFrame
    movl     rPC, offThread_pc(%ecx)          # update interpSave.pc
    movl     %ebx, offThread_curFrame(%ecx)    # update interpSave.curFrame
    movl     %ecx, OUT_ARG0(%esp)             # parameter self
    call     dvmReportReturn                  # (self)
    UNSPILL_TMP1(%ebx)
    movl     rSELF, %ecx                      # restore self
    SAVEAREA_FROM_FP %eax                     # restore saveArea
    jmp      14b


/*
 * Prepare to strip the current frame and "longjump" back to caller of
 * dvmMterpStdRun.
 *
 * on entry:
 *    rINST holds changeInterp
 *    ecx holds self pointer
 *
 * expected profile: dvmMterpStdBail(Thread *self, bool changeInterp)
 */
common_gotoBail:
    movl   rPC,offThread_pc(%ecx)     # export state to self
    movl   rFP,offThread_curFrame(%ecx)
    movl   %ecx,OUT_ARG0(%esp)      # self in arg0
    movl   rINST,OUT_ARG1(%esp)     # changeInterp in arg1
    call   dvmMterpStdBail          # bail out....

/*
 * The JIT's invoke method needs to remember the callsite class and
 * target pair.  Save them here so that they are available to
 * dvmCheckJit following the interpretation of this invoke.
 *
 * eax = Method* methodToCall
 * ecx = "this"
 * edx = rSELF
 * ebx = free to use
 */
#if defined(WITH_JIT)
save_callsiteinfo:
    cmp     $$0, %ecx
    je      2f
    movl    offObject_clazz(%ecx), %ecx
2:
    movl    rSELF, %ebx
    movl    %eax, offThread_methodToCall(%ebx)
    movl    %ecx, offThread_callsiteClass(%ebx)
    ret
#endif

#if defined(WITH_JIT)

    /*
     * If the JIT is actively building a trace we need to make sure
     * that the field is fully resolved before including the current
     * instruction.
     *
     * On entry:
     *     %ecx: &dvmDex->pResFields[field]
     *     %eax:  field pointer (must preserve)
     */
common_verifyField:
    movl    %ebx, TMP_SPILL1(%ebp)
    movl     rSELF, %ebx
    movzwl   offThread_subMode(%ebx), %ebx
    andl     $$kSubModeJitTraceBuild, %ebx
    movl    TMP_SPILL1(%ebp), %ebx
    jne      1f
    ret
1:
    movl    (%ecx), %ecx
    cmp     $$0, %ecx
    je      1f
    ret
1:
    SPILL_TMP1(%eax)
    SPILL_TMP2(%edx)
    movl     rSELF, %ecx
    # Because we call into this helper from a bytecode, we have
    # to be careful not to write over the return address when using
    # the OUT_ARG macros
    lea      -8(%esp), %esp
    movl     %ecx, OUT_ARG0(%esp)
    movl     rPC, OUT_ARG1(%esp)
    call     dvmJitEndTraceSelect
    lea      8(%esp), %esp
    UNSPILL_TMP2(%edx)
    UNSPILL_TMP1(%eax)
    ret
#endif

/*
 * After returning from a "selfd" function, pull out the updated values
 * and start executing at the next instruction.
 */
common_resumeAfterGlueCall:
     movl  rSELF, %eax
     movl  offThread_pc(%eax),rPC
     movl  offThread_curFrame(%eax),rFP
     movl  offThread_curHandlerTable(%eax),rIBASE
     FETCH_INST
     GOTO_NEXT

/*
 * Integer divide or mod by zero
 */
common_errDivideByZero:
    EXPORT_PC
    movl    $$.LstrDivideByZero,%eax
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowArithmeticException
    jmp     common_exceptionThrown

/*
 * Attempt to allocate an array with a negative size.
 * On entry, len in eax
 */
common_errNegativeArraySize:
    EXPORT_PC
    movl    %eax,OUT_ARG0(%esp)                  # arg0<- len
    call    dvmThrowNegativeArraySizeException   # (len)
    jmp     common_exceptionThrown

/*
 * Attempt to allocate an array with a negative size.
 * On entry, method name in eax
 */
common_errNoSuchMethod:
    EXPORT_PC
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowNoSuchMethodError
    jmp     common_exceptionThrown

/*
 * Hit a null object when we weren't expecting one.  Export the PC, throw a
 * NullPointerException and goto the exception processing code.
 */
common_errNullObject:
    EXPORT_PC
    xorl    %eax,%eax
    movl    %eax,OUT_ARG0(%esp)
    call    dvmThrowNullPointerException
    jmp     common_exceptionThrown

/*
 * Array index exceeds max.
 * On entry:
 *    eax <- array object
 *    ecx <- index
 */
common_errArrayIndex:
    EXPORT_PC
    movl    offArrayObject_length(%eax), %eax
    movl    %eax,OUT_ARG0(%esp)
    movl    %ecx,OUT_ARG1(%esp)
    call    dvmThrowArrayIndexOutOfBoundsException   # args (length, index)
    jmp     common_exceptionThrown

/*
 * Somebody has thrown an exception.  Handle it.
 *
 * If the exception processing code returns to us (instead of falling
 * out of the interpreter), continue with whatever the next instruction
 * now happens to be.
 *
 * NOTE: special subMode handling done in dvmMterp_exceptionThrown
 *
 * This does not return.
 */
common_exceptionThrown:
.LexceptionNew:

    EXPORT_PC
    movl       rSELF, %ecx
    movl       %ecx, OUT_ARG0(%esp)
    call       dvmCheckSuspendPending

    movl       rSELF, %ecx
    movl       offThread_exception(%ecx), %edx   # %edx <- self->exception
    movl       %edx, OUT_ARG0(%esp)
    movl       %ecx, OUT_ARG1(%esp)
    SPILL_TMP1(%edx)
    call       dvmAddTrackedAlloc      # don't let the exception be GCed
    UNSPILL_TMP1(%edx)
    movl       rSELF, %ecx
    movl       offThread_subMode(%ecx), %eax    # get subMode flags
    movl       $$0, offThread_exception(%ecx)

    # Special subMode?
    cmpl       $$0, %eax                # any special subMode handling needed?
    je         8f                      # go if so

    # Manage debugger bookkeeping
    movl       rPC, offThread_pc(%ecx) # update interpSave.pc
    movl       rFP, offThread_curFrame(%ecx) # update interpSave.curFrame
    movl       %ecx, OUT_ARG0(%esp)
    movl       %edx, OUT_ARG1(%esp)
    SPILL_TMP1(%edx)
    call       dvmReportExceptionThrow # (self, exception)
    UNSPILL_TMP1(%edx)
    movl       rSELF, %ecx

8:
    /*
    * set up args and a local for &fp
    */
    lea        20(%esp), %esp          # raise %esp
    movl       rFP, (%esp)               # save fp
    movl       %esp, %eax              # %eax = &fp
    lea        -20(%esp), %esp         # reset %esp
    movl       %eax, OUT_ARG4(%esp)    # Arg 4 = &fp
    movl       $$0, OUT_ARG3(%esp)      # Arg 3 = false
    movl       %edx, OUT_ARG2(%esp)    # Arg 2 = exception
    movl       %ecx, OUT_ARG0(%esp)    # Arg 0 = self

    movl       offThread_method(%ecx), %eax # %eax = self->method
    movl       offMethod_insns(%eax), %eax  # %eax = self->method->insn
    movl       rPC, %ecx
    subl       %eax, %ecx              # %ecx = pc - self->method->insn
    sar        $$1, %ecx                # adjust %ecx for code offset
    movl       %ecx, OUT_ARG1(%esp)    # Arg 1 = %ecx

    /* call, %eax gets catchRelPc (a code-unit offset) */
    SPILL_TMP1(%edx)                   # save exception
    call       dvmFindCatchBlock       # call(self, relPc, exc, scan?, &fp)
    UNSPILL_TMP1(%edx)                 # restore exception

    /* fix earlier stack overflow if necessary; may trash rFP */
    movl       rSELF, %ecx
    cmpl       $$0, offThread_stackOverflowed(%ecx) # did we overflow?
    je         1f                         # no, skip ahead
    movl       %eax, rFP                  # save relPc result in rFP
    movl       %ecx, OUT_ARG0(%esp)       # Arg 0 = self
    movl       %edx, OUT_ARG1(%esp)       # Arg 1 = exception
    SPILL_TMP1(%edx)
    call       dvmCleanupStackOverflow    # call(self, exception)
    UNSPILL_TMP1(%edx)
    movl       rFP, %eax                  # restore result
    movl       rSELF, %ecx
1:

    /* update frame pointer and check result from dvmFindCatchBlock */
    movl       20(%esp), rFP              # retrieve the updated rFP
    cmpl       $$0, %eax                  # is catchRelPc < 0?
    jl         .LnotCaughtLocally

    /* adjust locals to match self->interpSave.curFrame and updated PC */
    SAVEAREA_FROM_FP rINST             # rINST<- new save area
    movl       offStackSaveArea_method(rINST), rINST # rINST<- new method
    movl       rINST, offThread_method(%ecx)         # self->method = new method
    movl       offMethod_clazz(rINST), %ecx          # %ecx = method->clazz
    movl       offMethod_insns(rINST), rINST         # rINST = method->insn
    movl       offClassObject_pDvmDex(%ecx), %ecx    # %ecx = method->clazz->pDvmDex
    lea        (rINST, %eax, 2), rPC      # rPC<- method->insns + catchRelPc
    movl       rSELF, rINST
    movl       %ecx, offThread_methodClassDex(rINST) # self->pDvmDex = method->clazz->pDvmDex

    /* release the tracked alloc on the exception */
    movl       %edx, OUT_ARG0(%esp)       # Arg 0 = exception
    movl       rINST, OUT_ARG1(%esp)      # Arg 1 = self
    SPILL_TMP1(%edx)
    call       dvmReleaseTrackedAlloc     # release the exception
    UNSPILL_TMP1(%edx)

    /* restore the exception if the handler wants it */
    movl       rSELF, %ecx
    FETCH_INST
    movzbl     rINSTbl, %eax
    cmpl       $$OP_MOVE_EXCEPTION, %eax   # is it "move-exception"?
    jne        1f
    movl       %edx, offThread_exception(%ecx) # restore exception
1:
    movl       offThread_curHandlerTable(%ecx), rIBASE # refresh rIBASE
    GOTO_NEXT

.LnotCaughtLocally: # %edx = exception
    /* fix stack overflow if necessary */
    movl       rSELF, %ecx
    movl       offThread_stackOverflowed(%ecx), %eax
    cmpl       $$0, %eax                   # did we overflow earlier?
    je         1f
    movl       %ecx, OUT_ARG0(%esp)
    movl       %edx, OUT_ARG1(%esp)
    SPILL_TMP1(%edx)
    call       dvmCleanupStackOverflow
    UNSPILL_TMP1(%edx)

1:
    movl       rSELF, %ecx
    movl       %edx, offThread_exception(%ecx) #restore exception
    movl       %edx, OUT_ARG0(%esp)
    movl       %ecx, OUT_ARG1(%esp)
    call       dvmReleaseTrackedAlloc     # release the exception
    movl       rSELF, %ecx
    jmp        common_gotoBail            # bail out

common_abort:
    movl    $$0xdeadf00d,%eax
    call     *%eax


/*
 * Strings
 */

    .section     .rodata
.LstrDivideByZero:
    .asciz  "divide by zero"
.LstrFilledNewArrayNotImplA:
    .asciz  "filled-new-array only implemented for 'int'"
