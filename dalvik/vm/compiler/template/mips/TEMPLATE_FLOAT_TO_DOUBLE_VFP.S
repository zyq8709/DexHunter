%verify "executed"
%include "mips/funopWider.S" {"instr":"JAL(__extendsfdf2)","instr_f":"cvt.d.s fv0, fa0"}
