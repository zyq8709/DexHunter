    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* shr-long vAA:vBB(rARG1:rARG0), vCC(a2) - result in (rRESULT1:rRESULT0) */
    sra     rRESULT1, rARG1, a2		#  rhi<- ahi >> (shift&31)
    srl     rRESULT0, rARG0, a2		#  rlo<- alo >> (shift&31)
    sra     a3, rARG1, 31		#  a3<- sign(ah)
    not     rARG0, a2			#  alo<- 31-shift (shift is 5b)
    sll     rARG1, 1
    sll     rARG1, rARG0		#  ahi<- ahi << (32-(shift&31))
    or      rRESULT0, rARG1		#  rlo<- rlo | ahi
    andi    a2, 0x20			#  shift & 0x20
    movn    rRESULT0, rRESULT1, a2	#  rlo<- rhi (if shift&0x20)
    movn    rRESULT1, a3, a2		#  rhi<- sign(ahi) (if shift&0x20)
    RETURN
