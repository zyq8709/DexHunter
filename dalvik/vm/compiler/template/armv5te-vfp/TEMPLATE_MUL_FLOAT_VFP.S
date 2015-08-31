%verify "executed"
%include "armv5te-vfp/fbinop.S" {"instr":"fmuls   s2, s0, s1"}
