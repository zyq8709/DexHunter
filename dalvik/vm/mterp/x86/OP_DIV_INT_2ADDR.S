%verify "executed"
%include "x86/bindiv2addr.S" {"result":"%eax","special":"$0x80000000"}
