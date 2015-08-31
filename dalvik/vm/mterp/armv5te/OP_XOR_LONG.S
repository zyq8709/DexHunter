%verify "executed"
%include "armv5te/binopWide.S" {"preinstr":"eor     r0, r0, r2", "instr":"eor     r1, r1, r3"}
