%verify "executed"
%verify "negative value is sign-extended"
%include "x86/OP_IGET.S" { "load":"movsbl", "sqnum":"2" }
