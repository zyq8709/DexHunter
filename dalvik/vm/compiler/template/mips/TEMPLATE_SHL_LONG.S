    /*
     * Long integer shift.  This is different from the generic 32/64-bit
     * binary operations because vAA/vBB are 64-bit but vCC (the shift
     * distance) is 32-bit.  Also, Dalvik requires us to ignore all but the low
     * 6 bits.
     */
    /* shl-long vAA:vBB(rARG1:rARG0), vCC(a2) - result in (rRESULT1:rRESULT0) */
    sll     rRESULT0, rARG0, a2		#  rlo<- alo << (shift&31)
    not     rRESULT1, a2		#  rhi<- 31-shift  (shift is 5b)
    srl     rARG0, 1
    srl     rARG0, rRESULT1		#  alo<- alo >> (32-(shift&31))
    sll     rRESULT1, rARG1, a2		#  rhi<- ahi << (shift&31)
    or      rRESULT1, rARG0		#  rhi<- rhi | alo
    andi    a2, 0x20			#  shift< shift & 0x20
    movn    rRESULT1, rRESULT0, a2	#  rhi<- rlo (if shift&0x20)
    movn    rRESULT0, zero, a2		#  rlo<- 0  (if shift&0x20)
    RETURN
