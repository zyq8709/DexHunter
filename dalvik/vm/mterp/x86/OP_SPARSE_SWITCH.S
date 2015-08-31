%verify "executed"
%include "x86/OP_PACKED_SWITCH.S" { "func":"dvmInterpHandleSparseSwitch" }
