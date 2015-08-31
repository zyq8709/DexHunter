%verify "executed"
%include "armv5te/unopWider.S" {"instr":"mov     r1, r0, asr #31"}
