%verify "executed"
%include "mips/OP_SPUT_OBJECT.S" {"prebarrier":"SMP_DMB_ST", "postbarrier":"SMP_DMB"}
