%verify "executed"
%include "mips/unopWide.S" {"instr":"addu a1, a1, 0x80000000"}
