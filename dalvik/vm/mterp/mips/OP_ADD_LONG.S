%verify "executed"
/*
 *  The compiler generates the following sequence for
 *  [v1 v0] =  [a1 a0] + [a3 a2];
 *    addu v0,a2,a0
 *    addu a1,a3,a1
 *    sltu v1,v0,a2
 *    addu v1,v1,a1
 */
%include "mips/binopWide.S" { "result0":"v0", "result1":"v1", "preinstr":"addu v0, a2, a0", "instr":"addu a1, a3, a1; sltu v1, v0, a2; addu v1, v1, a1" }
