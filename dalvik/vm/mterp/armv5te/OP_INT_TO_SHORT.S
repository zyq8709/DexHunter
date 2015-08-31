%verify "executed"
%include "armv5te/unop.S" {"preinstr":"mov     r0, r0, asl #16", "instr":"mov     r0, r0, asr #16"}
