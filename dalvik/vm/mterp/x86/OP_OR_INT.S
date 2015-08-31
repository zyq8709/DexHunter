%verify "executed"
%include "x86/binop.S" {"instr":"orl   (rFP,%ecx,4),%eax"}
