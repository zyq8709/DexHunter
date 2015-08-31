%verify "executed"
%include "armv5te/binopWide.S" {"preinstr":"and     r0, r0, r2", "instr":"and     r1, r1, r3"}
