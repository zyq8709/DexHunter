# DexHunter

#####NOTICE： The feature string is very important. It may be changed along with the evolution of hardening services. If it is incorrect, the unpacking process cannot be triggered.

DexHunter aims at unpacking hardened dex file automatically.

DexHunter is based on the source code of Android runtime. It is composed of modified ART and DVM runtime. You can use the modified runtime to replace the original content in Android source codes (Android 4.4.3). The modification is mainly in "art/runtime/class_linker.cc" (ART) and "dalvik/vm/native/dalvik_system_DexFile.cpp" (DVM).

###Usage:

If you want to unpack an app, you need to push the "dexname" file to "/data/" in the mobile before starting the app. The first line in "dexname" is the feature string (referring to "slide.pptx"). The second line is the data path of the target app (e.g. "/data/data/com.test.test/"). Its line ending should be in the style of Unix/Linux. You can observe the log using "logcat" to determine whether the unpacking procedure is finished. Once done, the generated "whole.dex" file is the wanted result which is located in the app's data directory.

###Tips:

1) DexHunter simply reuses the content before "class_def" section instead of parsing them for the efficiency. If there are some problems, you can parse and reassemble them again or amend them statically.

2) It is worth noting that some "annotation_off" or "debug_info_off" fields may be invalid in the result. These fileds have nothing to do with execution just to hinder decompiling. We do not deal with this situation specifically for the moment. You can just program some scripts to set the invalid fileds with 0x00000000. 

3) As is known, some hardening services can protect several methods in the dex file by restoring the instructions just before being executed and wiping them just after finished. So you also need to modify the "DoInvoke" (ART) or "dvmMterp_invokeMethod" (DVM) function to extract the protected instruction while being executed.

4)The feature string may be changed along with the evolution of hardening services.

5)If the "fwrite" and other libc functions fail, maybe these functions are hooked by hardening sevices. As a result, you cannot dump the memory via them. You can bypass this limitation by calling relevant system calls directly.

DexHunter has its own limitation. As the hardening services develop, DexHunter may be not effective in the future. If you are interested, you can amend DexHunter to keep pace with hardening services continuously.

###File description:

"slide.pptx" is the presentation material of HITCON 2015 (http://hitcon.org/2015/ENT/Activities-Enterprise-Agenda.html#zyq) depicting the design and implementation of DexHunter.

"demo.mp4" is the demonstration video of unpacking a hardened app by Ali.

"test.apk" is the sample used in the video.

"dexname" is the configuration file used in the video.

"art" directory is the modified runtime for ART.

"dalvik" directory is the modified runtime for DVM.

"image" 7z files contain the system image files used in the video.

If you have any question, please contact me via emails to zyq8709@gmail.com.

If you use this code, please cite the following paper. Thanks!

Yueqian Zhang, Xiapu Luo, and Haoyang Yin, DexHunter: Toward Extracting Hidden Code from Packed Android Applications, Proceedings of the 20th European Symposium on Research in Computer Security (ESORICS), Vienna, Austria, September 2015.

@inproceedings{DexHunter15,
Title = {DexHunter: Toward Extracting Hidden Code from Packed Android Applications},
Author = {Yueqian Zhang and Xiapu Luo and Haoyang Yin},
Booktitle = {Proc. ESORICS},
Year = {2015}}

#####Comment: 

I have tested the samples from 360 in July under DVM. The feature string is changed to "/data/app/XXX.apk" (referring to "silde.pptx"). This string is very important. If it is incorrect, the unpacking process will fail.
