%verify "executed"
%include "x86/bindiv.S" {"result":"%eax","special":"$0x80000000"}
