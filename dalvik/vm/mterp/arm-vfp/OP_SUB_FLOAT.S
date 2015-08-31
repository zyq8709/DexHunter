%verify "executed"
%include "arm-vfp/fbinop.S" {"instr":"fsubs   s2, s0, s1"}
