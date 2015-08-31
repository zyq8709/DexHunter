%verify "executed"
%include "arm-vfp/fbinop2addr.S" {"instr":"fmuls   s2, s0, s1"}
