%verify "executed"
/* ldivmod returns quotient in r0/r1 and remainder in r2/r3 */
%include "armv6t2/binopWide2addr.S" {"instr":"bl      __aeabi_ldivmod", "result0":"r2", "result1":"r3", "chkzero":"1"}
