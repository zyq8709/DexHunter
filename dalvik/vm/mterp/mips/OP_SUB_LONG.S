%verify "executed"
/*
 * For little endian the code sequence looks as follows:
 *    subu    v0,a0,a2
 *    subu    v1,a1,a3
 *    sltu    a0,a0,v0
 *    subu    v1,v1,a0
 */
%include "mips/binopWide.S" { "result0":"v0", "result1":"v1", "preinstr":"subu v0, a0, a2", "instr":"subu v1, a1, a3; sltu a0, a0, v0; subu v1, v1, a0" }

