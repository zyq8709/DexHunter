%verify "executed"
%include "x86/binflop.S" {"instr":"fdivl","load":"fldl","store":"fstpl"}
