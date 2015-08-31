%verify "executed"
%include "mips/funopWider.S" {"instr":"JAL(__floatsidf)","instr_f":"cvt.d.w    fv0, fa0"}
