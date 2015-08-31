%verify "executed"
%include "mips/binflop.S" {"instr":"JAL(__subsf3)", "instr_f":"sub.s fv0, fa0, fa1"}
