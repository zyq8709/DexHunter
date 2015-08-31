%verify "executed"
%include "armv6t2/binop2addr.S" {"preinstr":"and     r1, r1, #31", "instr":"mov     r0, r0, asl r1"}
