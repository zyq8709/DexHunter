%verify "executed"
%include "armv5te/unopWide.S" {"instr":"add     r1, r1, #0x80000000"}
