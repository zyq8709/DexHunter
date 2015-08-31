%verify "executed"
%include "x86/binop.S" {"instr":"andl   (rFP,%ecx,4),%eax"}
