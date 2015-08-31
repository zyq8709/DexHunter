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

#include <stdlib.h>
#include <string.h>
#include "DexClass.h"
#include "Leb128.h"

/* Helper for verification which reads and verifies a given number
 * of uleb128 values. */
static bool verifyUlebs(const u1* pData, const u1* pLimit, u4 count) {
    bool okay = true;
    u4 i;

    while (okay && (count-- != 0)) {
        readAndVerifyUnsignedLeb128(&pData, pLimit, &okay);
    }

    return okay;
}

/* Read and verify the header of a class_data_item. This updates the
 * given data pointer to point past the end of the read data and
 * returns an "okay" flag (that is, false == failure). */
bool dexReadAndVerifyClassDataHeader(const u1** pData, const u1* pLimit,
        DexClassDataHeader *pHeader) {
    if (! verifyUlebs(*pData, pLimit, 4)) {
        return false;
    }

    dexReadClassDataHeader(pData, pHeader);
    return true;
}

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
        DexField* pField, u4* lastIndex) {
    if (! verifyUlebs(*pData, pLimit, 2)) {
        return false;
    }

    dexReadClassDataField(pData, pField, lastIndex);
    return true;
}

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
        DexMethod* pMethod, u4* lastIndex) {
    if (! verifyUlebs(*pData, pLimit, 3)) {
        return false;
    }

    dexReadClassDataMethod(pData, pMethod, lastIndex);
    return true;
}

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
DexClassData* dexReadAndVerifyClassData(const u1** pData, const u1* pLimit) {
    DexClassDataHeader header;
    u4 lastIndex;

    if (*pData == NULL) {
        DexClassData* result = (DexClassData*) malloc(sizeof(DexClassData));
        memset(result, 0, sizeof(*result));
        return result;
    }

    if (! dexReadAndVerifyClassDataHeader(pData, pLimit, &header)) {
        return NULL;
    }

    size_t resultSize = sizeof(DexClassData) +
        (header.staticFieldsSize * sizeof(DexField)) +
        (header.instanceFieldsSize * sizeof(DexField)) +
        (header.directMethodsSize * sizeof(DexMethod)) +
        (header.virtualMethodsSize * sizeof(DexMethod));

    DexClassData* result = (DexClassData*) malloc(resultSize);
    u1* ptr = ((u1*) result) + sizeof(DexClassData);
    bool okay = true;
    u4 i;

    if (result == NULL) {
        return NULL;
    }

    result->header = header;

    if (header.staticFieldsSize != 0) {
        result->staticFields = (DexField*) ptr;
        ptr += header.staticFieldsSize * sizeof(DexField);
    } else {
        result->staticFields = NULL;
    }

    if (header.instanceFieldsSize != 0) {
        result->instanceFields = (DexField*) ptr;
        ptr += header.instanceFieldsSize * sizeof(DexField);
    } else {
        result->instanceFields = NULL;
    }

    if (header.directMethodsSize != 0) {
        result->directMethods = (DexMethod*) ptr;
        ptr += header.directMethodsSize * sizeof(DexMethod);
    } else {
        result->directMethods = NULL;
    }

    if (header.virtualMethodsSize != 0) {
        result->virtualMethods = (DexMethod*) ptr;
    } else {
        result->virtualMethods = NULL;
    }

    lastIndex = 0;
    for (i = 0; okay && (i < header.staticFieldsSize); i++) {
        okay = dexReadAndVerifyClassDataField(pData, pLimit,
                &result->staticFields[i], &lastIndex);
    }

    lastIndex = 0;
    for (i = 0; okay && (i < header.instanceFieldsSize); i++) {
        okay = dexReadAndVerifyClassDataField(pData, pLimit,
                &result->instanceFields[i], &lastIndex);
    }

    lastIndex = 0;
    for (i = 0; okay && (i < header.directMethodsSize); i++) {
        okay = dexReadAndVerifyClassDataMethod(pData, pLimit,
                &result->directMethods[i], &lastIndex);
    }

    lastIndex = 0;
    for (i = 0; okay && (i < header.virtualMethodsSize); i++) {
        okay = dexReadAndVerifyClassDataMethod(pData, pLimit,
                &result->virtualMethods[i], &lastIndex);
    }

    if (! okay) {
        free(result);
        return NULL;
    }

    return result;
}
