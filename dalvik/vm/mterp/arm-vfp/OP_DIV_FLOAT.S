%verify "executed"
%include "arm-vfp/fbinop.S" {"instr":"fdivs   s2, s0, s1"}
