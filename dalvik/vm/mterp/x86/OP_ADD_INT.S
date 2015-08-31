%verify "executed"
%include "x86/binop.S" {"instr":"addl (rFP,%ecx,4),%eax"}
