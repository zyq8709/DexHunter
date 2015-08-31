%verify "executed"
%include "armv5te/binopWide.S" {"instr":"bl      __aeabi_dadd"}
