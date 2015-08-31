/*
 * Copyright (C) 2010 The Android Open Source Project
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
 * Functions to parse and manipulate the additional data tables added
 * to optimized .dex files.
 */

#ifndef _LIBDEX_DEXOPTDATA
#define _LIBDEX_DEXOPTDATA

#include "libdex/DexFile.h"

/*
 * Parse the optimized data tables in the given dex file.
 *
 * @param data pointer to the start of the entire dex file
 * @param length length of the entire dex file, in bytes
 * @param pDexFile pointer to the associated dex file structure
 */
bool dexParseOptData(const u1* data, size_t length, DexFile* pDexFile);

/*
 * Compute the checksum of the optimized data tables pointed at by the given
 * header.
 */
u4 dexComputeOptChecksum(const DexOptHeader* pOptHeader);

#endif /* def _LIBDEX_DEXOPTDATA */
