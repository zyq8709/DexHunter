/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "Dalvik.h"
#include "NcgHelper.h"
#include "interp/InterpDefs.h"


/*
 * Find the matching case.  Returns the offset to the handler instructions.
 *
 * Returns 3 if we don't find a match (it's the size of the packed-switch
 * instruction).
 */
s4 dvmNcgHandlePackedSwitch(const s4* entries, s4 firstKey, u2 size, s4 testVal)
{
    //skip add_reg_reg (ADD_REG_REG_SIZE) and jump_reg (JUMP_REG_SIZE)
    const int kInstrLen = 4; //default to next bytecode
    if (testVal < firstKey || testVal >= firstKey + size) {
        LOGVV("Value %d not found in switch (%d-%d)",
            testVal, firstKey, firstKey+size-1);
        return kInstrLen;
    }

    assert(testVal - firstKey >= 0 && testVal - firstKey < size);
    LOGVV("Value %d found in slot %d (goto 0x%02x)",
        testVal, testVal - firstKey,
        s4FromSwitchData(&entries[testVal - firstKey]));
    return s4FromSwitchData(&entries[testVal - firstKey]);

}
/* return the number of bytes to increase the bytecode pointer by */
s4 dvmJitHandlePackedSwitch(const s4* entries, s4 firstKey, u2 size, s4 testVal)
{
    if (testVal < firstKey || testVal >= firstKey + size) {
        LOGVV("Value %d not found in switch (%d-%d)",
            testVal, firstKey, firstKey+size-1);
        return 2*3;//bytecode packed_switch is 6(2*3) bytes long
    }

    LOGVV("Value %d found in slot %d (goto 0x%02x)",
        testVal, testVal - firstKey,
        s4FromSwitchData(&entries[testVal - firstKey]));
    return 2*s4FromSwitchData(&entries[testVal - firstKey]); //convert from u2 to byte

}
/*
 * Find the matching case.  Returns the offset to the handler instructions.
 *
 * Returns 3 if we don't find a match (it's the size of the sparse-switch
 * instruction).
 */
s4 dvmNcgHandleSparseSwitch(const s4* keys, u2 size, s4 testVal)
{
    const int kInstrLen = 4; //CHECK
    const s4* entries = keys + size;
    int i;
    for (i = 0; i < size; i++) {
        s4 k = s4FromSwitchData(&keys[i]);
        if (k == testVal) {
            LOGVV("Value %d found in entry %d (goto 0x%02x)",
                testVal, i, s4FromSwitchData(&entries[i]));
            return s4FromSwitchData(&entries[i]);
        } else if (k > testVal) {
            break;
        }
    }

    LOGVV("Value %d not found in switch", testVal);
    return kInstrLen;
}
/* return the number of bytes to increase the bytecode pointer by */
s4 dvmJitHandleSparseSwitch(const s4* keys, u2 size, s4 testVal)
{
    const s4* entries = keys + size;
    int i;
    for (i = 0; i < size; i++) {
        s4 k = s4FromSwitchData(&keys[i]);
        if (k == testVal) {
            LOGVV("Value %d found in entry %d (goto 0x%02x)",
                testVal, i, s4FromSwitchData(&entries[i]));
            return 2*s4FromSwitchData(&entries[i]); //convert from u2 to byte
        } else if (k > testVal) {
            break;
        }
    }

    LOGVV("Value %d not found in switch", testVal);
    return 2*3; //bytecode sparse_switch is 6(2*3) bytes long
}
