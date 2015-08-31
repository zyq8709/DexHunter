%verify "executed"
%include "mips/unopWide.S" {"result0":"v0", "result1":"v1", "preinstr":"negu v0, a0", "instr":"negu v1, a1; sltu a0, zero, v0; subu v1, v1, a0"}

