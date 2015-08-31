%verify "executed"
%include "x86/binopLit16.S" {"instr":"subl %eax,%ecx","result":"%ecx"}
