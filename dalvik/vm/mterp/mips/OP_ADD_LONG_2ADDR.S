%verify "executed"
/*
 *See OP_ADD_LONG.S for details
 */
%include "mips/binopWide2addr.S" { "result0":"v0", "result1":"v1", "preinstr":"addu v0, a2, a0", "instr":"addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1" }
