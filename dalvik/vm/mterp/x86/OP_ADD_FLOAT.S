%verify "executed"
%include "x86/binflop.S" {"instr":"fadds","load":"flds","store":"fstps"}
