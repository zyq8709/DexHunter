%verify "executed"
%include "mips/unflop.S" {"instr":"JAL(__floatsisf)", "instr_f":"cvt.s.w fv0, fa0"}
