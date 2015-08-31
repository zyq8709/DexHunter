%verify "executed"
%include "armv5te/binop2addr.S" {"instr":"bl     __aeabi_idiv","chkzero":"1"}
