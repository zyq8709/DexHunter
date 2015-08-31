%verify "executed"
%include "x86/binflop2addr.S" {"instr":"fdivl","load":"fldl","store":"fstpl"}
