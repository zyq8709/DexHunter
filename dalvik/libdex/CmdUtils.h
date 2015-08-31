/*
 * Copyright (C) 2008 The Android Open Source Project
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
/*
 * Access .dex (Dalvik Executable Format) files.  The code here assumes that
 * the DEX file has been rewritten (byte-swapped, word-aligned) and that
 * the contents can be directly accessed as a collection of C arrays.  Please
 * see docs/dalvik/dex-format.html for a detailed description.
 *
 * The structure and field names were chosen to match those in the DEX spec.
 *
 * It's generally assumed that the DEX file will be stored in shared memory,
 * obviating the need to copy code and constant pool entries into newly
 * allocated storage.  Maintaining local pointers to items in the shared area
 * is valid and encouraged.
 *
 * All memory-mapped structures are 32-bit aligned unless otherwise noted.
 */
#ifndef LIBDEX_CMDUTILS_H_
#define LIBDEX_CMDUTILS_H_

/* encode the result of unzipping to a file */
enum UnzipToFileResult {
    kUTFRSuccess = 0,
    kUTFRGenericFailure,
    kUTFRBadArgs,
    kUTFRNotZip,
    kUTFRNoClassesDex,
    kUTFROutputFileProblem,
    kUTFRBadZip,
};

/*
 * Map the specified DEX file read-only (possibly after expanding it into a
 * temp file from a Jar).  Pass in a MemMapping struct to hold the info.
 * If the file is an unoptimized DEX file, then byte-swapping and structural
 * verification are performed on it before the memory is made read-only.
 *
 * The temp file is deleted after the map succeeds.
 *
 * This is intended for use by tools (e.g. dexdump) that need to get a
 * read-only copy of a DEX file that could be in a number of different states.
 *
 * If "tempFileName" is NULL, a default value is used.  The temp file is
 * deleted after the map succeeds.
 *
 * If "quiet" is set, don't report common errors.
 *
 * Returns 0 (kUTFRSuccess) on success.
 */
UnzipToFileResult dexOpenAndMap(const char* fileName, const char* tempFileName,
    MemMapping* pMap, bool quiet);

/*
 * Utility function to open a Zip archive, find "classes.dex", and extract
 * it to a file.
 */
UnzipToFileResult dexUnzipToFile(const char* zipFileName,
    const char* outFileName, bool quiet);

#endif  // LIBDEX_CMDUTILS_H_
