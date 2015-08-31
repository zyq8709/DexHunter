Step 1

If you see a native crash in the bugreport and the PC/LR are pointing to the
code cache address range*, copy them into codePC and codeLR in gdbjithelper.c,
respectively.

*Caveats: debuggerd doesn't know the range of code cache. So apply this tool if
the crashing address is not contained by any shared library.

       #00  pc 463ba204
       #01  lr 463ba1c9  <unknown>

code around pc:
463ba1e4 4300e119 4284aa7a f927f7b7 40112268
463ba1f4 419da7f8 00002000 01000100 00080000
463ba204 4191debc 01010000 4284aa74 68b00054
463ba214 045cf205 cc016468 0718f2a5 d0102800
463ba224 4c13c701 a20aa108 efb0f775 e008e010

code around lr:
463ba1a8 42e19e58 f2050050 cc01045c 0718f2a5
463ba1b8 d00f2800 4c13c701 a20aa108 efe4f775
463ba1c8 e007e010 29006bf8 6e77dc01 a10347b8
463ba1d8 ef60f775 6db1480b 1c2d4788 4300e119
463ba1e8 4284aa7a f927f7b7 40112268 419da7f8


Step 2

Push $OUT/EXECUTABLES/gdbjithelper_intermediates/LINKED/gdbjithelper to
/system/bin on the device or emulator


Step 3

Debug the executable as usual:

adb forward tcp:5039 tcp:5039
adb shell gdbserver :5039 /system/bin/gdbjithelper &
arm-eabi-gdb $OUT/symbols/system/bin/gdbjithelper
(gdb) tar r :5039
Remote debugging using :5039
Remote debugging from host 127.0.0.1
gdb: Unable to get location for thread creation breakpoint: requested event is not supported
__dl__start () at bionic/linker/arch/arm/begin.S:35
35      mov r0, sp
gdb: Unable to get location for thread creation breakpoint: requested event is not supported
Current language:  auto; currently asm
(gdb) c
Continuing.
[New Thread 596]
codePC[0]: 0x4300e119
codePC[1]: 0x4284aa7a
         :


Step 4

Hit ctrl-C

Issue the following command to see code around PC
x /20i (char *) &codePC+1

Issue the following command to see code around LR
x /20i (char *) &codeLR+1
