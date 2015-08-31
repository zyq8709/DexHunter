%verify "executed"
%include "x86/fpcvt.S" {"instr":"fchs","load":"flds","store":"fstps"}
