%verify "executed"
%include "armv5te-vfp/fbinop.S" {"instr":"fsubs   s2, s0, s1"}
