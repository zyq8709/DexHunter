%verify "executed"
%include "armv5te/binopWide2addr.S" {"instr":"bl      __aeabi_ldivmod", "chkzero":"1"}
