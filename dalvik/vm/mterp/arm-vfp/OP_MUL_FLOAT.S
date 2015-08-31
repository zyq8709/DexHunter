%verify "executed"
%include "arm-vfp/fbinop.S" {"instr":"fmuls   s2, s0, s1"}
