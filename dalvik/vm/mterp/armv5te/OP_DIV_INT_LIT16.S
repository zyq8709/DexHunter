%verify "executed"
%include "armv5te/binopLit16.S" {"instr":"bl     __aeabi_idiv","chkzero":"1"}
