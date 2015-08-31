%verify "executed"
%include "mips/unopWide.S" {"preinstr":"not a0, a0", "instr":"not a1, a1"}
