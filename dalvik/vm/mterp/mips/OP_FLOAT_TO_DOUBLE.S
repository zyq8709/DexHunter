%verify "executed"
%include "mips/unflopWider.S" {"instr":"JAL(__extendsfdf2)", "instr_f":"cvt.d.s fv0, fa0"}
