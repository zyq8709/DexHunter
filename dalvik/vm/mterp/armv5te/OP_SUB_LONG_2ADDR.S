%verify "executed"
%include "armv5te/binopWide2addr.S" {"preinstr":"subs    r0, r0, r2", "instr":"sbc     r1, r1, r3"}
