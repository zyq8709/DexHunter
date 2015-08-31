%verify "executed"
%include "armv6t2/binop2addr.S" {"instr":"bl     __aeabi_idiv","chkzero":"1"}
