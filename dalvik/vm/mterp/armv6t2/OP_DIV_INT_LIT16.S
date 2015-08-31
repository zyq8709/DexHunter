%verify "executed"
%include "armv6t2/binopLit16.S" {"instr":"bl     __aeabi_idiv","chkzero":"1"}
