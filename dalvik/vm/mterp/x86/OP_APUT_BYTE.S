%verify "executed"
%include "x86/OP_APUT.S" { "reg":"rINSTbl", "store":"movb", "shift":"1" }
