%verify "executed"
/*
 * See comments in OP_SUB_LONG.S
 */
%include "mips/binopWide2addr.S" { "result0":"v0", "result1":"v1", "preinstr":"subu v0, a0, a2", "instr":"subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0" }
