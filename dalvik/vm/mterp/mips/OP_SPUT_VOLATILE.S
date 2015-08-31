%verify "executed"
%include "mips/OP_SPUT.S" {"prebarrier":"SMP_DMB_ST", "postbarrier":"SMP_DMB"}
