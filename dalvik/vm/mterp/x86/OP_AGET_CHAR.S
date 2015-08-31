%verify "executed"
%include "x86/OP_AGET.S" { "load":"movzwl", "shift":"2" }
