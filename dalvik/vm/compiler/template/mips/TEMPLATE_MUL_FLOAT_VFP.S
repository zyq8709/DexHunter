%verify "executed"
%include "mips/fbinop.S" {"instr":"JAL(__mulsf3)","instr_f":"mul.s fv0, fa0, fa1"}
