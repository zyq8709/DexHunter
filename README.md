# DexHunter
DexHunter aims at unpacking hardened dex file automatically.

DexHunter is based on the source code of Android runtime. It is composed of modified ART and DVM runtime. You can use the modified runtime to replace the original content in Android source codes (Android 4.4.3).

If you want to unpack an app, you need to push the "dexname" file to "/data/" in the mobile before starting the app. The first line in "dexname" is the feature string (referring to "slide.pptx"). The second line is the data path of the target app (e.g. "/data/data/com.test.test/"). The feature string may be changed along with the evolution of hardening services. You can observe the log using "logcat" to determine whether the unpacking procedure is finished. Once done, the generated "whole.dex" file is the wanted result.

It is worth noting that some "annotation_off" or "debug_info_off" fields may be invalid in the result. These fileds have nothing to do with execution just to hinder decompiling. We do not deal with this situation specifically for the moment. You can just program some scripts to set the invalid fileds with 0x00000000. 

As is known, some hardening services can protect several methods in the dex file by restoring the instructions just before being executed and wiping them just after finished. So you also need to modify the "DoInvoke" (ART) or "dvmMterp_invokeMethod" (DVM) function to extract the protected instruction while being executed.

"slide.pptx" is the presentation material of HITCON 2015 depicting the design and implementation of DexHunter.

"demo.mp4" is the demonstration video of DexHunter.

"art" directory is the modified runtime for ART.

"dalvik" directory is the modified runtime for DVM.
