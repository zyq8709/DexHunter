%verify "executed"
%include "x86/binop.S" {"instr":"xorl   (rFP,%ecx,4),%eax"}
