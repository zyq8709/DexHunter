%verify "executed"
%include "armv6t2/binopWide2addr.S" {"instr":"bl      __aeabi_ldivmod", "chkzero":"1"}
