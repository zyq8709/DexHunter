%verify "executed"
%include "armv5te/OP_SPUT.S" {"prebarrier":"SMP_DMB_ST", "postbarrier":"SMP_DMB"}
