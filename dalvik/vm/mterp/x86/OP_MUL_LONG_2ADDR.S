%verify "executed"
    /*
     * Signed 64-bit integer multiply, 2-addr version
     *
     * We could definately use more free registers for
     * this code.  We must spill %edx (rIBASE) because it
     * is used by imul.  We'll also spill rINST (ebx),
     * giving us eax, ebc, ecx and rIBASE as computational
     * temps.  On top of that, we'll spill %esi (edi)
     * for use as the vA pointer and rFP (esi) for use
     * as the vB pointer.  Yuck.
     */
    /* mul-long/2addr vA, vB */
    movzbl    rINSTbl,%eax             # eax<- BA
    andb      $$0xf,%al                # eax<- A
    sarl      $$4,rINST                # rINST<- B
    SPILL_TMP2(%esi)
    SPILL(rFP)
    SPILL(rIBASE)
    leal      (rFP,%eax,4),%esi        # %esi<- &v[A]
    leal      (rFP,rINST,4),rFP        # rFP<- &v[B]
    movl      4(%esi),%ecx             # ecx<- Amsw
    imull     (rFP),%ecx               # ecx<- (Amsw*Blsw)
    movl      4(rFP),%eax              # eax<- Bmsw
    imull     (%esi),%eax              # eax<- (Bmsw*Alsw)
    addl      %eax,%ecx                # ecx<- (Amsw*Blsw)+(Bmsw*Alsw)
    movl      (rFP),%eax               # eax<- Blsw
    mull      (%esi)                   # eax<- (Blsw*Alsw)
    leal      (%ecx,rIBASE),rIBASE     # full result now in %edx:%eax
    movl      rIBASE,4(%esi)           # v[A+1]<- rIBASE
    movl      %eax,(%esi)              # v[A]<- %eax
    UNSPILL_TMP2(%esi)
    FETCH_INST_OPCODE 1 %ecx
    UNSPILL(rIBASE)
    UNSPILL(rFP)
    ADVANCE_PC 1
    GOTO_NEXT_R %ecx
