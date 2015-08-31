%verify "executed"
%verify "negative value is sign-extended"
%include "x86/OP_IGET.S" { "load":"movswl", "sqnum":"4" }
