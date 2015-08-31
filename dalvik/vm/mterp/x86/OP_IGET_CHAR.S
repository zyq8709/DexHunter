%verify "executed"
%verify "large values are not sign-extended"
%include "x86/OP_IGET.S" { "load":"movzwl", "sqnum":"3" }
