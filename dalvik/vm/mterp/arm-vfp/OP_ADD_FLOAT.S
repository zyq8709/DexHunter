%verify "executed"
%include "arm-vfp/fbinop.S" {"instr":"fadds   s2, s0, s1"}
