%verify "executed"
%include "x86/binopLit8.S" {"instr":"subl  %eax,%ecx" , "result":"%ecx"}
