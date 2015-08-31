%verify "executed"
%include "x86/OP_APUT.S" { "reg":"rINSTw", "store":"movw", "shift":"2" }
