%verify "executed"
    /*
     * Signed 64-bit integer multiply.
     *
     * We could definately use more free registers for
     * this code.   We spill rINSTw (ebx),
     * giving us eax, ebc, ecx and edx as computational
     * temps.  On top of that, we'll spill edi (rFP)
     * for use as the vB pointer and esi (rPC) for use
     * as the vC pointer.  Yuck.
     */
    /* mul-long vAA, vBB, vCC */
    movzbl    2(rPC),%eax              # eax<- B
    movzbl    3(rPC),%ecx              # ecx<- C
    SPILL_TMP2(%esi)                   # save Dalvik PC
    SPILL(rFP)
    SPILL(rINST)
    SPILL(rIBASE)
    leal      (rFP,%eax,4),%esi        # esi<- &v[B]
    leal      (rFP,%ecx,4),rFP         # rFP<- &v[C]
    movl      4(%esi),%ecx             # ecx<- Bmsw
    imull     (rFP),%ecx               # ecx<- (Bmsw*Clsw)
    movl      4(rFP),%eax              # eax<- Cmsw
    imull     (%esi),%eax              # eax<- (Cmsw*Blsw)
    addl      %eax,%ecx                # ecx<- (Bmsw*Clsw)+(Cmsw*Blsw)
    movl      (rFP),%eax               # eax<- Clsw
    mull      (%esi)                   # eax<- (Clsw*Alsw)
    UNSPILL(rINST)
    UNSPILL(rFP)
    leal      (%ecx,rIBASE),rIBASE # full result now in rIBASE:%eax
    UNSPILL_TMP2(%esi)             # Restore Dalvik PC
    FETCH_INST_OPCODE 2 %ecx       # Fetch next instruction
    movl      rIBASE,4(rFP,rINST,4)# v[B+1]<- rIBASE
    UNSPILL(rIBASE)
    movl      %eax,(rFP,rINST,4)   # v[B]<- %eax
    ADVANCE_PC 2
    GOTO_NEXT_R %ecx
