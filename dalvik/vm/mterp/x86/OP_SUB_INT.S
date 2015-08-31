%verify "executed"
%include "x86/binop.S" {"instr":"subl   (rFP,%ecx,4),%eax"}
