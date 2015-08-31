%verify "executed"
%include "armv5te-vfp/fbinop.S" {"instr":"fadds   s2, s0, s1"}
