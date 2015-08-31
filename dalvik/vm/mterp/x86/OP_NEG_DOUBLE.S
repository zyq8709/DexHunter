%verify "executed"
%include "x86/fpcvt.S" {"instr":"fchs","load":"fldl","store":"fstpl"}
