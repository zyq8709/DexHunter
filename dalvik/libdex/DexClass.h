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
 * Functions to deal with class definition structures in DEX files
 */

#ifndef LIBDEX_DEXCLASS_H_
#define LIBDEX_DEXCLASS_H_

#include "DexFile.h"
#include "Leb128.h"

/* expanded form of a class_data_item header */
struct DexClassDataHeader {
    u4 staticFieldsSize;
    u4 instanceFieldsSize;
    u4 directMethodsSize;
    u4 virtualMethodsSize;
};

/* expanded form of encoded_field */
struct DexField {
    u4 fieldIdx;    /* index to a field_id_item */
    u4 accessFlags;
};

/* expanded form of encoded_method */
struct DexMethod {
    u4 methodIdx;    /* index to a method_id_item */
    u4 accessFlags;
    u4 codeOff;      /* file offset to a code_item */
};

/* expanded form of class_data_item. Note: If a particular item is
 * absent (e.g., no static fields), then the corresponding pointer
 * is set to NULL. */
struct DexClassData {
    DexClassDataHeader header;
    DexField*          staticFields;
    DexField*          instanceFields;
    DexMethod*         directMethods;
    DexMethod*         virtualMethods;
};

/* Read and verify the header of a class_data_item. This updates the
 * given data pointer to point past the end of the read data and
 * returns an "okay" flag (that is, false == failure). */
bool dexReadAndVerifyClassDataHeader(const u1** pData, const u1* pLimit,
        DexClassDataHeader *pHeader);

/* Read and verify an encoded_field. This updates the
 * given data pointer to point past the end of the read data and
 * returns an "okay" flag (that is, false == failure).
 *
 * The lastIndex value should be set to 0 before the first field in
 * a list is read. It is updated as fields are read and used in the
 * decode process.
 *
 * The verification done by this function is of the raw data format
 * only; it does not verify that access flags or indices
 * are valid. */
bool dexReadAndVerifyClassDataField(const u1** pData, const u1* pLimit,
        DexField* pField, u4* lastIndex);

/* Read and verify an encoded_method. This updates the
 * given data pointer to point past the end of the read data and
 * returns an "okay" flag (that is, false == failure).
 *
 * The lastIndex value should be set to 0 before the first method in
 * a list is read. It is updated as fields are read and used in the
 * decode process.
 *
 * The verification done by this function is of the raw data format
 * only; it does not verify that access flags, indices, or offsets
 * are valid. */
bool dexReadAndVerifyClassDataMethod(const u1** pData, const u1* pLimit,
        DexMethod* pMethod, u4* lastIndex);

/* Read, verify, and return an entire class_data_item. This updates
 * the given data pointer to point past the end of the read data. This
 * function allocates a single chunk of memory for the result, which
 * must subsequently be free()d. This function returns NULL if there
 * was trouble parsing the data. If this function is passed NULL, it
 * returns an initialized empty DexClassData structure.
 *
 * The verification done by this function is of the raw data format
 * only; it does not verify that access flags, indices, or offsets
 * are valid. */
DexClassData* dexReadAndVerifyClassData(const u1** pData, const u1* pLimit);

/*
 * Get the DexCode for a DexMethod.  Returns NULL if the class is native
 * or abstract.
 */
DEX_INLINE const DexCode* dexGetCode(const DexFile* pDexFile,
    const DexMethod* pDexMethod)
{
    if (pDexMethod->codeOff == 0)
        return NULL;
    return (const DexCode*) (pDexFile->baseAddr + pDexMethod->codeOff);
}


/* Read the header of a class_data_item without verification. This
 * updates the given data pointer to point past the end of the read
 * data. */
DEX_INLINE void dexReadClassDataHeader(const u1** pData,
        DexClassDataHeader *pHeader) {
    pHeader->staticFieldsSize = readUnsignedLeb128(pData);
    pHeader->instanceFieldsSize = readUnsignedLeb128(pData);
    pHeader->directMethodsSize = readUnsignedLeb128(pData);
    pHeader->virtualMethodsSize = readUnsignedLeb128(pData);
}

/* Read an encoded_field without verification. This updates the
 * given data pointer to point past the end of the read data.
 *
 * The lastIndex value should be set to 0 before the first field in
 * a list is read. It is updated as fields are read and used in the
 * decode process.
 */
DEX_INLINE void dexReadClassDataField(const u1** pData, DexField* pField,
        u4* lastIndex) {
    u4 index = *lastIndex + readUnsignedLeb128(pData);

    pField->accessFlags = readUnsignedLeb128(pData);
    pField->fieldIdx = index;
    *lastIndex = index;
}

/* Read an encoded_method without verification. This updates the
 * given data pointer to point past the end of the read data.
 *
 * The lastIndex value should be set to 0 before the first method in
 * a list is read. It is updated as fields are read and used in the
 * decode process.
 */
DEX_INLINE void dexReadClassDataMethod(const u1** pData, DexMethod* pMethod,
        u4* lastIndex) {
    u4 index = *lastIndex + readUnsignedLeb128(pData);

    pMethod->accessFlags = readUnsignedLeb128(pData);
    pMethod->codeOff = readUnsignedLeb128(pData);
    pMethod->methodIdx = index;
    *lastIndex = index;
}

#endif  // LIBDEX_DEXCLASS_H_
