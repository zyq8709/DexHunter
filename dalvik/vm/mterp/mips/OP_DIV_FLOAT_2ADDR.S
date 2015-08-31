%verify "executed"
%include "mips/binflop2addr.S" {"instr":"JAL(__divsf3)", "instr_f":"div.s fv0, fa0, fa1"}
