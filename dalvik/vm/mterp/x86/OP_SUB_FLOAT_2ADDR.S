%verify "executed"
%include "x86/binflop2addr.S" {"instr":"fsubs","load":"flds","store":"fstps"}
