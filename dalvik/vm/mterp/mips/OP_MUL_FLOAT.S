%verify "executed"
%include "mips/binflop.S" {"instr":"JAL(__mulsf3)", "instr_f":"mul.s fv0, fa0, fa1"}
