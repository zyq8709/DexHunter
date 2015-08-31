# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

---------------------------------------------------------------------
Notes on updating the sets of defined opcodes and instruction formats
---------------------------------------------------------------------

##########

If you want to add, delete, or change opcodes:

* Update the file bytecode.txt, in this directory.

* Run the regen-all script, in this directory. This will regenerate a
  number of tables, definitions, and declarations in the code, in
  dalvik/dx, dalvik/libdex, and libcore/dalvik.

* Implement/update the opcode in C in vm/mterp/c/...
  * Verify new code by running with "dalvik.vm.execution-mode = int:portable"
    or "-Xint:portable".

* Implement/update the instruction in assembly in vm/mterp/{arm*,x86*}/...
  * Verify by enabling the assembly (e.g. ARM) handler for that instruction
    in mterp/config-* and running "int:fast" as above.

* Implement/update the instruction in
  vm/compiler/codegen/{arm,x86}/CodegenDriver.c.

* Rebuild the interpreter code. See the notes in vm/mterp/ReadMe.txt for
  details.

* Look in the directory vm/analysis at the files CodeVerify.c,
  DexVerify.c, and Optimize.c. You may need to update them to account
  for your changes.
  * If you change anything here, be sure to try running the system with
    the verifier enabled (which is in fact the default).

##########

If you want to add, delete, or change instruction formats:

This is a more manual affair than changing opcodes.

* Update the file bytecode.txt, and run regen-all, as per above.

* Update the instruction format list in libdex/InstrUtils.h.

* Update dexDecodeInstruction() in libdex/InstrUtils.c.

* Update dumpInstruction() and its helper code in dexdump/DexDump.c.

* Update the switch inside dvmCompilerMIR2LIR() in
  vm/compiler/codegen/{arm,x86}/CodegenDriver.c. (There may be other
  architectures to deal with too.)

##########

Testing your work:

The Dalvik VM tests (in the vm/tests directory) provide a convenient
way to test most of the above without doing any rebuilds. In
particular, test 003-omnibus-opcodes will exercise most of the
opcodes.
