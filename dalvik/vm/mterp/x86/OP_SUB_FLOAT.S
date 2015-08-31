%verify "executed"
%include "x86/binflop.S" {"instr":"fsubs","load":"flds","store":"fstps"}
