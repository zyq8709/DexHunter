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
 * Byte-swapping and verification of dex files.
 */

#include "DexFile.h"
#include "DexClass.h"
#include "DexDataMap.h"
#include "DexProto.h"
#include "DexUtf.h"
#include "Leb128.h"

#include <safe_iop.h>
#include <zlib.h>

#include <stdlib.h>
#include <string.h>

#ifndef __BYTE_ORDER
# error "byte ordering not defined"
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define SWAP2(_value)      (_value)
# define SWAP4(_value)      (_value)
# define SWAP8(_value)      (_value)
#else
# define SWAP2(_value)      endianSwapU2((_value))
# define SWAP4(_value)      endianSwapU4((_value))
# define SWAP8(_value)      endianSwapU8((_value))
static u2 endianSwapU2(u2 value) {
    return (value >> 8) | (value << 8);
}
static u4 endianSwapU4(u4 value) {
    /* ABCD --> CDAB --> DCBA */
    value = (value >> 16) | (value << 16);
    return ((value & 0xff00ff00) >> 8) | ((value << 8) & 0xff00ff00);
}
static u8 endianSwapU8(u8 value) {
    /* ABCDEFGH --> EFGHABCD --> GHEFCDAB --> HGFEDCBA */
    value = (value >> 32) | (value << 32);
    value = ((value & 0xffff0000ffff0000ULL) >> 16) |
            ((value << 16) & 0xffff0000ffff0000ULL);
    return ((value & 0xff00ff00ff00ff00ULL) >> 8) |
           ((value << 8) & 0xff00ff00ff00ff00ULL);
}
#endif

#define SWAP_FIELD2(_field) (_field) = SWAP2(_field)
#define SWAP_FIELD4(_field) (_field) = SWAP4(_field)
#define SWAP_FIELD8(_field) (_field) = SWAP8(_field)

/*
 * Some information we pass around to help verify values.
 */
struct CheckState {
    const DexHeader*  pHeader;
    const u1*         fileStart;
    const u1*         fileEnd;      // points to fileStart + fileLen
    u4                fileLen;
    DexDataMap*       pDataMap;     // set after map verification
    const DexFile*    pDexFile;     // set after intraitem verification

    /*
     * bitmap of type_id indices that have been used to define classes;
     * initialized immediately before class_def cross-verification, and
     * freed immediately after it
     */
    u4*               pDefinedClassBits;

    const void*       previousItem; // set during section iteration
};

/*
 * Return the file offset of the given pointer.
 */
static inline u4 fileOffset(const CheckState* state, const void* ptr) {
    return ((const u1*) ptr) - state->fileStart;
}

/*
 * Return a pointer for the given file offset.
 */
static inline void* filePointer(const CheckState* state, u4 offset) {
    return (void*) (state->fileStart + offset);
}

/*
 * Verify that a pointer range, start inclusive to end exclusive, only
 * covers bytes in the file and doesn't point beyond the end of the
 * file. That is, the start must indicate a valid byte or may point at
 * the byte just past the end of the file (but no further), and the
 * end must be no less than the start and must also not point beyond
 * the byte just past the end of the file.
 */
static inline bool checkPtrRange(const CheckState* state,
        const void* start, const void* end, const char* label) {
    const void* fileStart = state->fileStart;
    const void* fileEnd = state->fileEnd;
    if ((start < fileStart) || (start > fileEnd)
            || (end < start) || (end > fileEnd)) {
        ALOGW("Bad offset range for %s: %#x..%#x", label,
                fileOffset(state, start), fileOffset(state, end));
        return false;
    }
    return true;
}

/*
 * Verify that a range of offsets, start inclusive to end exclusive,
 * are all valid. That is, the start must indicate a valid byte or may
 * point at the byte just past the end of the file (but no further),
 * and the end must be no less than the start and must also not point
 * beyond the byte just past the end of the file.
 *
 * Assumes "const CheckState* state".
 */
#define CHECK_OFFSET_RANGE(_start, _end) {                                  \
        const u1* _startPtr = (const u1*) filePointer(state, (_start));     \
        const u1* _endPtr = (const u1*) filePointer(state, (_end));         \
        if (!checkPtrRange(state, _startPtr, _endPtr,                       \
                        #_start ".." #_end)) {                              \
            return 0;                                                       \
        }                                                                   \
    }

/*
 * Verify that a pointer range, start inclusive to end exclusive, only
 * covers bytes in the file and doesn't point beyond the end of the
 * file. That is, the start must indicate a valid byte or may point at
 * the byte just past the end of the file (but no further), and the
 * end must be no less than the start and must also not point beyond
 * the byte just past the end of the file.
 *
 * Assumes "const CheckState* state".
 */
#define CHECK_PTR_RANGE(_start, _end) {                                     \
        if (!checkPtrRange(state, (_start), (_end), #_start ".." #_end)) {  \
            return 0;                                                       \
        }                                                                   \
    }

/*
 * Make sure a list of items fits entirely within the file.
 *
 * Assumes "const CheckState* state" and "typeof(_count) == typeof(_elemSize)"
 * If the type sizes or signs are mismatched, this will return 0.
 */
#define CHECK_LIST_SIZE(_ptr, _count, _elemSize) {                          \
        const u1* _start = (const u1*) (_ptr);                              \
        const u1* _end = _start + ((_count) * (_elemSize));                 \
        if (!safe_mul(NULL, (_count), (_elemSize)) ||                       \
            !checkPtrRange(state, _start, _end, #_ptr)) {                   \
            return 0;                                                       \
        }                                                                   \
    }

/*
 * Swap a field that is known to hold an absolute DEX file offset. Note:
 * This does not check to see that the swapped offset points within the
 * mapped file, since that should be handled (with even more rigor) by
 * the cross-verification phase.
 *
 * Assumes "const CheckState* state".
 */
#define SWAP_OFFSET4(_field) {                                              \
        SWAP_FIELD4((_field));                                              \
    }

/*
 * Verify that an index falls in a valid range.
 */
#define CHECK_INDEX(_field, _limit) {                                       \
        if ((_field) >= (_limit)) {                                         \
            ALOGW("Bad index: %s(%u) > %s(%u)",                             \
                #_field, (u4)(_field), #_limit, (u4)(_limit));              \
            return 0;                                                       \
        }                                                                   \
    }

/*
 * Swap an index, and verify that it falls in a valid range.
 */
#define SWAP_INDEX2(_field, _limit) {                                       \
        SWAP_FIELD2((_field));                                              \
        CHECK_INDEX((_field), (_limit));                                    \
    }

/*
 * Verify that an index falls in a valid range or is kDexNoIndex.
 */
#define CHECK_INDEX_OR_NOINDEX(_field, _limit) {                            \
        if ((_field) != kDexNoIndex && (_field) >= (_limit)) {              \
            ALOGW("Bad index: %s(%u) > %s(%u)",                             \
                #_field, (u4)(_field), #_limit, (u4)(_limit));              \
            return 0;                                                       \
        }                                                                   \
    }

/*
 * Swap an index, and verify that it falls in a valid range.
 */
#define SWAP_INDEX4(_field, _limit) {                                       \
        SWAP_FIELD4((_field));                                              \
        CHECK_INDEX((_field), (_limit));                                    \
    }

/*
 * Swap an index, and verify that it falls in a valid range or is
 * kDexNoIndex.
 */
#define SWAP_INDEX4_OR_NOINDEX(_field, _limit) {                            \
        SWAP_FIELD4((_field));                                              \
        CHECK_INDEX_OR_NOINDEX((_field), (_limit));                         \
    }

/* Verify the definer of a given field_idx. */
static bool verifyFieldDefiner(const CheckState* state, u4 definingClass,
        u4 fieldIdx) {
    const DexFieldId* field = dexGetFieldId(state->pDexFile, fieldIdx);
    return field->classIdx == definingClass;
}

/* Verify the definer of a given method_idx. */
static bool verifyMethodDefiner(const CheckState* state, u4 definingClass,
        u4 methodIdx) {
    const DexMethodId* meth = dexGetMethodId(state->pDexFile, methodIdx);
    return meth->classIdx == definingClass;
}

/*
 * Calculate the required size (in elements) of the array pointed at by
 * pDefinedClassBits.
 */
static size_t calcDefinedClassBitsSize(const CheckState* state)
{
    // Divide typeIdsSize by 32 (0x20), rounding up.
    return (state->pHeader->typeIdsSize + 0x1f) >> 5;
}

/*
 * Set the given bit in pDefinedClassBits, returning its former value.
 */
static bool setDefinedClassBit(const CheckState* state, u4 typeIdx) {
    u4 arrayIdx = typeIdx >> 5;
    u4 bit = 1 << (typeIdx & 0x1f);
    u4* element = &state->pDefinedClassBits[arrayIdx];
    bool result = (*element & bit) != 0;

    *element |= bit;

    return result;
}

/*
 * Swap the header_item.
 */
static bool swapDexHeader(const CheckState* state, DexHeader* pHeader)
{
    CHECK_PTR_RANGE(pHeader, pHeader + 1);

    // magic is ok
    SWAP_FIELD4(pHeader->checksum);
    // signature is ok
    SWAP_FIELD4(pHeader->fileSize);
    SWAP_FIELD4(pHeader->headerSize);
    SWAP_FIELD4(pHeader->endianTag);
    SWAP_FIELD4(pHeader->linkSize);
    SWAP_OFFSET4(pHeader->linkOff);
    SWAP_OFFSET4(pHeader->mapOff);
    SWAP_FIELD4(pHeader->stringIdsSize);
    SWAP_OFFSET4(pHeader->stringIdsOff);
    SWAP_FIELD4(pHeader->typeIdsSize);
    SWAP_OFFSET4(pHeader->typeIdsOff);
    SWAP_FIELD4(pHeader->fieldIdsSize);
    SWAP_OFFSET4(pHeader->fieldIdsOff);
    SWAP_FIELD4(pHeader->methodIdsSize);
    SWAP_OFFSET4(pHeader->methodIdsOff);
    SWAP_FIELD4(pHeader->protoIdsSize);
    SWAP_OFFSET4(pHeader->protoIdsOff);
    SWAP_FIELD4(pHeader->classDefsSize);
    SWAP_OFFSET4(pHeader->classDefsOff);
    SWAP_FIELD4(pHeader->dataSize);
    SWAP_OFFSET4(pHeader->dataOff);

    if (pHeader->endianTag != kDexEndianConstant) {
        ALOGE("Unexpected endian_tag: %#x", pHeader->endianTag);
        return false;
    }

    // Assign variables so the diagnostic is prettier. (Hooray for macros.)
    u4 linkOff = pHeader->linkOff;
    u4 linkEnd = linkOff + pHeader->linkSize;
    u4 dataOff = pHeader->dataOff;
    u4 dataEnd = dataOff + pHeader->dataSize;
    CHECK_OFFSET_RANGE(linkOff, linkEnd);
    CHECK_OFFSET_RANGE(dataOff, dataEnd);

    /*
     * Note: The offsets and ranges of the other header items end up getting
     * checked during the first iteration over the map.
     */

    return true;
}

/* Check the header section for sanity. */
static bool checkHeaderSection(const CheckState* state, u4 sectionOffset,
        u4 sectionCount, u4* endOffset) {
    if (sectionCount != 1) {
        ALOGE("Multiple header items");
        return false;
    }

    if (sectionOffset != 0) {
        ALOGE("Header at %#x; not at start of file", sectionOffset);
        return false;
    }

    const DexHeader* pHeader = (const DexHeader*) filePointer(state, 0);
    *endOffset = pHeader->headerSize;
    return true;
}

/*
 * Helper for swapMap(), which turns a map type constant into a small
 * one-bit-on integer, suitable for use in an int-sized bit set.
 */
static u4 mapTypeToBitMask(int mapType) {
    switch (mapType) {
        case kDexTypeHeaderItem:               return 1 << 0;
        case kDexTypeStringIdItem:             return 1 << 1;
        case kDexTypeTypeIdItem:               return 1 << 2;
        case kDexTypeProtoIdItem:              return 1 << 3;
        case kDexTypeFieldIdItem:              return 1 << 4;
        case kDexTypeMethodIdItem:             return 1 << 5;
        case kDexTypeClassDefItem:             return 1 << 6;
        case kDexTypeMapList:                  return 1 << 7;
        case kDexTypeTypeList:                 return 1 << 8;
        case kDexTypeAnnotationSetRefList:     return 1 << 9;
        case kDexTypeAnnotationSetItem:        return 1 << 10;
        case kDexTypeClassDataItem:            return 1 << 11;
        case kDexTypeCodeItem:                 return 1 << 12;
        case kDexTypeStringDataItem:           return 1 << 13;
        case kDexTypeDebugInfoItem:            return 1 << 14;
        case kDexTypeAnnotationItem:           return 1 << 15;
        case kDexTypeEncodedArrayItem:         return 1 << 16;
        case kDexTypeAnnotationsDirectoryItem: return 1 << 17;
        default: {
            ALOGE("Unknown map item type %04x", mapType);
            return 0;
        }
    }
}

/*
 * Helper for swapMap(), which indicates if an item type should appear
 * in the data section.
 */
static bool isDataSectionType(int mapType) {
    switch (mapType) {
        case kDexTypeHeaderItem:
        case kDexTypeStringIdItem:
        case kDexTypeTypeIdItem:
        case kDexTypeProtoIdItem:
        case kDexTypeFieldIdItem:
        case kDexTypeMethodIdItem:
        case kDexTypeClassDefItem: {
            return false;
        }
    }

    return true;
}

/*
 * Swap the map_list and verify what we can about it. Also, if verification
 * passes, allocate the state's DexDataMap.
 */
static bool swapMap(CheckState* state, DexMapList* pMap)
{
    DexMapItem* item = pMap->list;
    u4 count;
    u4 dataItemCount = 0; // Total count of items in the data section.
    u4 dataItemsLeft = state->pHeader->dataSize; // See use below.
    u4 usedBits = 0;      // Bit set: one bit per section
    bool first = true;
    u4 lastOffset = 0;

    SWAP_FIELD4(pMap->size);
    count = pMap->size;

    CHECK_LIST_SIZE(item, count, sizeof(DexMapItem));

    while (count--) {
        SWAP_FIELD2(item->type);
        SWAP_FIELD2(item->unused);
        SWAP_FIELD4(item->size);
        SWAP_OFFSET4(item->offset);

        if (first) {
            first = false;
        } else if (lastOffset >= item->offset) {
            ALOGE("Out-of-order map item: %#x then %#x",
                    lastOffset, item->offset);
            return false;
        }

        if (item->offset >= state->pHeader->fileSize) {
            ALOGE("Map item after end of file: %x, size %#x",
                    item->offset, state->pHeader->fileSize);
            return false;
        }

        if (isDataSectionType(item->type)) {
            u4 icount = item->size;

            /*
             * This sanity check on the data section items ensures that
             * there are no more items than the number of bytes in
             * the data section.
             */
            if (icount > dataItemsLeft) {
                ALOGE("Unrealistically many items in the data section: "
                        "at least %d", dataItemCount + icount);
                return false;
            }

            dataItemsLeft -= icount;
            dataItemCount += icount;
        }

        u4 bit = mapTypeToBitMask(item->type);

        if (bit == 0) {
            return false;
        }

        if ((usedBits & bit) != 0) {
            ALOGE("Duplicate map section of type %#x", item->type);
            return false;
        }

        usedBits |= bit;
        lastOffset = item->offset;
        item++;
    }

    if ((usedBits & mapTypeToBitMask(kDexTypeHeaderItem)) == 0) {
        ALOGE("Map is missing header entry");
        return false;
    }

    if ((usedBits & mapTypeToBitMask(kDexTypeMapList)) == 0) {
        ALOGE("Map is missing map_list entry");
        return false;
    }

    if (((usedBits & mapTypeToBitMask(kDexTypeStringIdItem)) == 0)
            && ((state->pHeader->stringIdsOff != 0)
                    || (state->pHeader->stringIdsSize != 0))) {
        ALOGE("Map is missing string_ids entry");
        return false;
    }

    if (((usedBits & mapTypeToBitMask(kDexTypeTypeIdItem)) == 0)
            && ((state->pHeader->typeIdsOff != 0)
                    || (state->pHeader->typeIdsSize != 0))) {
        ALOGE("Map is missing type_ids entry");
        return false;
    }

    if (((usedBits & mapTypeToBitMask(kDexTypeProtoIdItem)) == 0)
            && ((state->pHeader->protoIdsOff != 0)
                    || (state->pHeader->protoIdsSize != 0))) {
        ALOGE("Map is missing proto_ids entry");
        return false;
    }

    if (((usedBits & mapTypeToBitMask(kDexTypeFieldIdItem)) == 0)
            && ((state->pHeader->fieldIdsOff != 0)
                    || (state->pHeader->fieldIdsSize != 0))) {
        ALOGE("Map is missing field_ids entry");
        return false;
    }

    if (((usedBits & mapTypeToBitMask(kDexTypeMethodIdItem)) == 0)
            && ((state->pHeader->methodIdsOff != 0)
                    || (state->pHeader->methodIdsSize != 0))) {
        ALOGE("Map is missing method_ids entry");
        return false;
    }

    if (((usedBits & mapTypeToBitMask(kDexTypeClassDefItem)) == 0)
            && ((state->pHeader->classDefsOff != 0)
                    || (state->pHeader->classDefsSize != 0))) {
        ALOGE("Map is missing class_defs entry");
        return false;
    }

    state->pDataMap = dexDataMapAlloc(dataItemCount);
    if (state->pDataMap == NULL) {
        ALOGE("Unable to allocate data map (size %#x)", dataItemCount);
        return false;
    }

    return true;
}

/* Check the map section for sanity. */
static bool checkMapSection(const CheckState* state, u4 sectionOffset,
        u4 sectionCount, u4* endOffset) {
    if (sectionCount != 1) {
        ALOGE("Multiple map list items");
        return false;
    }

    if (sectionOffset != state->pHeader->mapOff) {
        ALOGE("Map not at header-defined offset: %#x, expected %#x",
                sectionOffset, state->pHeader->mapOff);
        return false;
    }

    const DexMapList* pMap = (const DexMapList*) filePointer(state, sectionOffset);

    *endOffset =
        sectionOffset + sizeof(u4) + (pMap->size * sizeof(DexMapItem));
    return true;
}

/* Perform byte-swapping and intra-item verification on string_id_item. */
static void* swapStringIdItem(const CheckState* state, void* ptr) {
    DexStringId* item = (DexStringId*) ptr;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_OFFSET4(item->stringDataOff);

    return item + 1;
}

/* Perform cross-item verification of string_id_item. */
static void* crossVerifyStringIdItem(const CheckState* state, void* ptr) {
    const DexStringId* item = (const DexStringId*) ptr;

    if (!dexDataMapVerify(state->pDataMap,
                    item->stringDataOff, kDexTypeStringDataItem)) {
        return NULL;
    }

    const DexStringId* item0 = (const DexStringId*) state->previousItem;
    if (item0 != NULL) {
        // Check ordering.
        const char* s0 = dexGetStringData(state->pDexFile, item0);
        const char* s1 = dexGetStringData(state->pDexFile, item);
        if (dexUtf8Cmp(s0, s1) >= 0) {
            ALOGE("Out-of-order string_ids: '%s' then '%s'", s0, s1);
            return NULL;
        }
    }

    return (void*) (item + 1);
}

/* Perform byte-swapping and intra-item verification on type_id_item. */
static void* swapTypeIdItem(const CheckState* state, void* ptr) {
    DexTypeId* item = (DexTypeId*) ptr;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_INDEX4(item->descriptorIdx, state->pHeader->stringIdsSize);

    return item + 1;
}

/* Perform cross-item verification of type_id_item. */
static void* crossVerifyTypeIdItem(const CheckState* state, void* ptr) {
    const DexTypeId* item = (const DexTypeId*) ptr;
    const char* descriptor =
        dexStringById(state->pDexFile, item->descriptorIdx);

    if (!dexIsValidTypeDescriptor(descriptor)) {
        ALOGE("Invalid type descriptor: '%s'", descriptor);
        return NULL;
    }

    const DexTypeId* item0 = (const DexTypeId*) state->previousItem;
    if (item0 != NULL) {
        // Check ordering. This relies on string_ids being in order.
        if (item0->descriptorIdx >= item->descriptorIdx) {
            ALOGE("Out-of-order type_ids: %#x then %#x",
                    item0->descriptorIdx, item->descriptorIdx);
            return NULL;
        }
    }

    return (void*) (item + 1);
}

/* Perform byte-swapping and intra-item verification on proto_id_item. */
static void* swapProtoIdItem(const CheckState* state, void* ptr) {
    DexProtoId* item = (DexProtoId*) ptr;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_INDEX4(item->shortyIdx, state->pHeader->stringIdsSize);
    SWAP_INDEX4(item->returnTypeIdx, state->pHeader->typeIdsSize);
    SWAP_OFFSET4(item->parametersOff);

    return item + 1;
}

/* Helper for crossVerifyProtoIdItem(), which checks a shorty character
 * to see if it is compatible with a type descriptor. Returns true if
 * so, false if not. */
static bool shortyDescMatch(char shorty, const char* descriptor, bool
        isReturnType) {
    switch (shorty) {
        case 'V': {
            if (!isReturnType) {
                ALOGE("Invalid use of void");
                return false;
            }
            // Fall through.
        }
        case 'B':
        case 'C':
        case 'D':
        case 'F':
        case 'I':
        case 'J':
        case 'S':
        case 'Z': {
            if ((descriptor[0] != shorty) || (descriptor[1] != '\0')) {
                ALOGE("Shorty vs. primitive type mismatch: '%c', '%s'",
                        shorty, descriptor);
                return false;
            }
            break;
        }
        case 'L': {
            if ((descriptor[0] != 'L') && (descriptor[0] != '[')) {
                ALOGE("Shorty vs. type mismatch: '%c', '%s'",
                        shorty, descriptor);
                return false;
            }
            break;
        }
        default: {
            ALOGE("Bogus shorty: '%c'", shorty);
            return false;
        }
    }

    return true;
}

/* Perform cross-item verification of proto_id_item. */
static void* crossVerifyProtoIdItem(const CheckState* state, void* ptr) {
    const DexProtoId* item = (const DexProtoId*) ptr;
    const char* shorty =
        dexStringById(state->pDexFile, item->shortyIdx);

    if (!dexDataMapVerify0Ok(state->pDataMap,
                    item->parametersOff, kDexTypeTypeList)) {
        return NULL;
    }

    if (!shortyDescMatch(*shorty,
                    dexStringByTypeIdx(state->pDexFile, item->returnTypeIdx),
                    true)) {
        return NULL;
    }

    u4 protoIdx = item - state->pDexFile->pProtoIds;
    DexProto proto = { state->pDexFile, protoIdx };
    DexParameterIterator iterator;

    dexParameterIteratorInit(&iterator, &proto);
    shorty++; // Skip the return type.

    for (;;) {
        const char *desc = dexParameterIteratorNextDescriptor(&iterator);

        if (desc == NULL) {
            break;
        }

        if (*shorty == '\0') {
            ALOGE("Shorty is too short");
            return NULL;
        }

        if (!shortyDescMatch(*shorty, desc, false)) {
            return NULL;
        }

        shorty++;
    }

    if (*shorty != '\0') {
        ALOGE("Shorty is too long");
        return NULL;
    }

    const DexProtoId* item0 = (const DexProtoId*) state->previousItem;
    if (item0 != NULL) {
        // Check ordering. This relies on type_ids being in order.
        if (item0->returnTypeIdx > item->returnTypeIdx) {
            ALOGE("Out-of-order proto_id return types");
            return NULL;
        } else if (item0->returnTypeIdx == item->returnTypeIdx) {
            bool badOrder = false;
            DexProto proto0 = { state->pDexFile, protoIdx - 1 };
            DexParameterIterator iterator0;

            dexParameterIteratorInit(&iterator, &proto);
            dexParameterIteratorInit(&iterator0, &proto0);

            for (;;) {
                u4 idx0 = dexParameterIteratorNextIndex(&iterator0);
                u4 idx1 = dexParameterIteratorNextIndex(&iterator);

                if (idx1 == kDexNoIndex) {
                    badOrder = true;
                    break;
                }

                if (idx0 == kDexNoIndex) {
                    break;
                }

                if (idx0 < idx1) {
                    break;
                } else if (idx0 > idx1) {
                    badOrder = true;
                    break;
                }
            }

            if (badOrder) {
                ALOGE("Out-of-order proto_id arguments");
                return NULL;
            }
        }
    }

    return (void*) (item + 1);
}

/* Perform byte-swapping and intra-item verification on field_id_item. */
static void* swapFieldIdItem(const CheckState* state, void* ptr) {
    DexFieldId* item = (DexFieldId*) ptr;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_INDEX2(item->classIdx, state->pHeader->typeIdsSize);
    SWAP_INDEX2(item->typeIdx, state->pHeader->typeIdsSize);
    SWAP_INDEX4(item->nameIdx, state->pHeader->stringIdsSize);

    return item + 1;
}

/* Perform cross-item verification of field_id_item. */
static void* crossVerifyFieldIdItem(const CheckState* state, void* ptr) {
    const DexFieldId* item = (const DexFieldId*) ptr;
    const char* s;

    s = dexStringByTypeIdx(state->pDexFile, item->classIdx);
    if (!dexIsClassDescriptor(s)) {
        ALOGE("Invalid descriptor for class_idx: '%s'", s);
        return NULL;
    }

    s = dexStringByTypeIdx(state->pDexFile, item->typeIdx);
    if (!dexIsFieldDescriptor(s)) {
        ALOGE("Invalid descriptor for type_idx: '%s'", s);
        return NULL;
    }

    s = dexStringById(state->pDexFile, item->nameIdx);
    if (!dexIsValidMemberName(s)) {
        ALOGE("Invalid name: '%s'", s);
        return NULL;
    }

    const DexFieldId* item0 = (const DexFieldId*) state->previousItem;
    if (item0 != NULL) {
        // Check ordering. This relies on the other sections being in order.
        bool done = false;
        bool bogus = false;

        if (item0->classIdx > item->classIdx) {
            bogus = true;
            done = true;
        } else if (item0->classIdx < item->classIdx) {
            done = true;
        }

        if (!done) {
            if (item0->nameIdx > item->nameIdx) {
                bogus = true;
                done = true;
            } else if (item0->nameIdx < item->nameIdx) {
                done = true;
            }
        }

        if (!done) {
            if (item0->typeIdx >= item->typeIdx) {
                bogus = true;
            }
        }

        if (bogus) {
            ALOGE("Out-of-order field_ids");
            return NULL;
        }
    }

    return (void*) (item + 1);
}

/* Perform byte-swapping and intra-item verification on method_id_item. */
static void* swapMethodIdItem(const CheckState* state, void* ptr) {
    DexMethodId* item = (DexMethodId*) ptr;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_INDEX2(item->classIdx, state->pHeader->typeIdsSize);
    SWAP_INDEX2(item->protoIdx, state->pHeader->protoIdsSize);
    SWAP_INDEX4(item->nameIdx, state->pHeader->stringIdsSize);

    return item + 1;
}

/* Perform cross-item verification of method_id_item. */
static void* crossVerifyMethodIdItem(const CheckState* state, void* ptr) {
    const DexMethodId* item = (const DexMethodId*) ptr;
    const char* s;

    s = dexStringByTypeIdx(state->pDexFile, item->classIdx);
    if (!dexIsReferenceDescriptor(s)) {
        ALOGE("Invalid descriptor for class_idx: '%s'", s);
        return NULL;
    }

    s = dexStringById(state->pDexFile, item->nameIdx);
    if (!dexIsValidMemberName(s)) {
        ALOGE("Invalid name: '%s'", s);
        return NULL;
    }

    const DexMethodId* item0 = (const DexMethodId*) state->previousItem;
    if (item0 != NULL) {
        // Check ordering. This relies on the other sections being in order.
        bool done = false;
        bool bogus = false;

        if (item0->classIdx > item->classIdx) {
            bogus = true;
            done = true;
        } else if (item0->classIdx < item->classIdx) {
            done = true;
        }

        if (!done) {
            if (item0->nameIdx > item->nameIdx) {
                bogus = true;
                done = true;
            } else if (item0->nameIdx < item->nameIdx) {
                done = true;
            }
        }

        if (!done) {
            if (item0->protoIdx >= item->protoIdx) {
                bogus = true;
            }
        }

        if (bogus) {
            ALOGE("Out-of-order method_ids");
            return NULL;
        }
    }

    return (void*) (item + 1);
}

/* Perform byte-swapping and intra-item verification on class_def_item. */
static void* swapClassDefItem(const CheckState* state, void* ptr) {
    DexClassDef* item = (DexClassDef*) ptr;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_INDEX4(item->classIdx, state->pHeader->typeIdsSize);
    SWAP_FIELD4(item->accessFlags);
    SWAP_INDEX4_OR_NOINDEX(item->superclassIdx, state->pHeader->typeIdsSize);
    SWAP_OFFSET4(item->interfacesOff);
    SWAP_INDEX4_OR_NOINDEX(item->sourceFileIdx, state->pHeader->stringIdsSize);
    SWAP_OFFSET4(item->annotationsOff);
    SWAP_OFFSET4(item->classDataOff);

    if ((item->accessFlags & ~ACC_CLASS_MASK) != 0) {
        // The VM specification says that unknown flags should be ignored.
        ALOGV("Bogus class access flags %x", item->accessFlags);
        item->accessFlags &= ACC_CLASS_MASK;
    }

    return item + 1;
}

/* defined below */
static u4 findFirstClassDataDefiner(const CheckState* state,
        DexClassData* classData);
static u4 findFirstAnnotationsDirectoryDefiner(const CheckState* state,
        const DexAnnotationsDirectoryItem* dir);

/* Helper for crossVerifyClassDefItem(), which checks a class_data_item to
 * make sure all its references are to a given class. */
static bool verifyClassDataIsForDef(const CheckState* state, u4 offset,
        u4 definerIdx) {
    if (offset == 0) {
        return true;
    }

    const u1* data = (const u1*) filePointer(state, offset);
    DexClassData* classData = dexReadAndVerifyClassData(&data, NULL);

    if (classData == NULL) {
        // Shouldn't happen, but bail here just in case.
        return false;
    }

    /*
     * The class_data_item verification ensures that
     * it consistently refers to the same definer, so all we need to
     * do is check the first one.
     */
    u4 dataDefiner = findFirstClassDataDefiner(state, classData);
    bool result = (dataDefiner == definerIdx) || (dataDefiner == kDexNoIndex);

    free(classData);
    return result;
}

/* Helper for crossVerifyClassDefItem(), which checks an
 * annotations_directory_item to make sure all its references are to a
 * given class. */
static bool verifyAnnotationsDirectoryIsForDef(const CheckState* state,
        u4 offset, u4 definerIdx) {
    if (offset == 0) {
        return true;
    }

    const DexAnnotationsDirectoryItem* dir =
        (const DexAnnotationsDirectoryItem*) filePointer(state, offset);
    u4 annoDefiner = findFirstAnnotationsDirectoryDefiner(state, dir);

    return (annoDefiner == definerIdx) || (annoDefiner == kDexNoIndex);
}

/* Perform cross-item verification of class_def_item. */
static void* crossVerifyClassDefItem(const CheckState* state, void* ptr) {
    const DexClassDef* item = (const DexClassDef*) ptr;
    u4 classIdx = item->classIdx;
    const char* descriptor = dexStringByTypeIdx(state->pDexFile, classIdx);

    if (!dexIsClassDescriptor(descriptor)) {
        ALOGE("Invalid class: '%s'", descriptor);
        return NULL;
    }

    if (setDefinedClassBit(state, classIdx)) {
        ALOGE("Duplicate class definition: '%s'", descriptor);
        return NULL;
    }

    bool okay =
        dexDataMapVerify0Ok(state->pDataMap,
                item->interfacesOff, kDexTypeTypeList)
        && dexDataMapVerify0Ok(state->pDataMap,
                item->annotationsOff, kDexTypeAnnotationsDirectoryItem)
        && dexDataMapVerify0Ok(state->pDataMap,
                item->classDataOff, kDexTypeClassDataItem)
        && dexDataMapVerify0Ok(state->pDataMap,
                item->staticValuesOff, kDexTypeEncodedArrayItem);

    if (!okay) {
        return NULL;
    }

    if (item->superclassIdx != kDexNoIndex) {
        descriptor = dexStringByTypeIdx(state->pDexFile, item->superclassIdx);
        if (!dexIsClassDescriptor(descriptor)) {
            ALOGE("Invalid superclass: '%s'", descriptor);
            return NULL;
        }
    }

    const DexTypeList* interfaces =
        dexGetInterfacesList(state->pDexFile, item);
    if (interfaces != NULL) {
        u4 size = interfaces->size;
        u4 i;

        /*
         * Ensure that all interfaces refer to classes (not arrays or
         * primitives).
         */
        for (i = 0; i < size; i++) {
            descriptor = dexStringByTypeIdx(state->pDexFile,
                    dexTypeListGetIdx(interfaces, i));
            if (!dexIsClassDescriptor(descriptor)) {
                ALOGE("Invalid interface: '%s'", descriptor);
                return NULL;
            }
        }

        /*
         * Ensure that there are no duplicates. This is an O(N^2) test,
         * but in practice the number of interfaces implemented by any
         * given class is low. I will buy a milkshake for the
         * first person to show me a realistic case for which this test
         * would be unacceptably slow.
         */
        for (i = 1; i < size; i++) {
            u4 idx1 = dexTypeListGetIdx(interfaces, i);
            u4 j;
            for (j = 0; j < i; j++) {
                u4 idx2 = dexTypeListGetIdx(interfaces, j);
                if (idx1 == idx2) {
                    ALOGE("Duplicate interface: '%s'",
                            dexStringByTypeIdx(state->pDexFile, idx1));
                    return NULL;
                }
            }
        }
    }

    if (!verifyClassDataIsForDef(state, item->classDataOff, item->classIdx)) {
        ALOGE("Invalid class_data_item");
        return NULL;
    }

    if (!verifyAnnotationsDirectoryIsForDef(state, item->annotationsOff,
                    item->classIdx)) {
        ALOGE("Invalid annotations_directory_item");
        return NULL;
    }

    return (void*) (item + 1);
}

/* Helper for swapAnnotationsDirectoryItem(), which performs
 * byte-swapping and intra-item verification on an
 * annotation_directory_item's field elements. */
static u1* swapFieldAnnotations(const CheckState* state, u4 count, u1* addr) {
    DexFieldAnnotationsItem* item = (DexFieldAnnotationsItem*) addr;
    bool first = true;
    u4 lastIdx = 0;

    CHECK_LIST_SIZE(item, count, sizeof(DexFieldAnnotationsItem));

    while (count--) {
        SWAP_INDEX4(item->fieldIdx, state->pHeader->fieldIdsSize);
        SWAP_OFFSET4(item->annotationsOff);

        if (first) {
            first = false;
        } else if (lastIdx >= item->fieldIdx) {
            ALOGE("Out-of-order field_idx: %#x then %#x", lastIdx,
                 item->fieldIdx);
            return NULL;
        }

        lastIdx = item->fieldIdx;
        item++;
    }

    return (u1*) item;
}

/* Helper for swapAnnotationsDirectoryItem(), which performs
 * byte-swapping and intra-item verification on an
 * annotation_directory_item's method elements. */
static u1* swapMethodAnnotations(const CheckState* state, u4 count, u1* addr) {
    DexMethodAnnotationsItem* item = (DexMethodAnnotationsItem*) addr;
    bool first = true;
    u4 lastIdx = 0;

    CHECK_LIST_SIZE(item, count, sizeof(DexMethodAnnotationsItem));

    while (count--) {
        SWAP_INDEX4(item->methodIdx, state->pHeader->methodIdsSize);
        SWAP_OFFSET4(item->annotationsOff);

        if (first) {
            first = false;
        } else if (lastIdx >= item->methodIdx) {
            ALOGE("Out-of-order method_idx: %#x then %#x", lastIdx,
                 item->methodIdx);
            return NULL;
        }

        lastIdx = item->methodIdx;
        item++;
    }

    return (u1*) item;
}

/* Helper for swapAnnotationsDirectoryItem(), which performs
 * byte-swapping and intra-item verification on an
 * annotation_directory_item's parameter elements. */
static u1* swapParameterAnnotations(const CheckState* state, u4 count,
        u1* addr) {
    DexParameterAnnotationsItem* item = (DexParameterAnnotationsItem*) addr;
    bool first = true;
    u4 lastIdx = 0;

    CHECK_LIST_SIZE(item, count, sizeof(DexParameterAnnotationsItem));

    while (count--) {
        SWAP_INDEX4(item->methodIdx, state->pHeader->methodIdsSize);
        SWAP_OFFSET4(item->annotationsOff);

        if (first) {
            first = false;
        } else if (lastIdx >= item->methodIdx) {
            ALOGE("Out-of-order method_idx: %#x then %#x", lastIdx,
                 item->methodIdx);
            return NULL;
        }

        lastIdx = item->methodIdx;
        item++;
    }

    return (u1*) item;
}

/* Perform byte-swapping and intra-item verification on
 * annotations_directory_item. */
static void* swapAnnotationsDirectoryItem(const CheckState* state, void* ptr) {
    DexAnnotationsDirectoryItem* item = (DexAnnotationsDirectoryItem*) ptr;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_OFFSET4(item->classAnnotationsOff);
    SWAP_FIELD4(item->fieldsSize);
    SWAP_FIELD4(item->methodsSize);
    SWAP_FIELD4(item->parametersSize);

    u1* addr = (u1*) (item + 1);

    if (item->fieldsSize != 0) {
        addr = swapFieldAnnotations(state, item->fieldsSize, addr);
        if (addr == NULL) {
            return NULL;
        }
    }

    if (item->methodsSize != 0) {
        addr = swapMethodAnnotations(state, item->methodsSize, addr);
        if (addr == NULL) {
            return NULL;
        }
    }

    if (item->parametersSize != 0) {
        addr = swapParameterAnnotations(state, item->parametersSize, addr);
        if (addr == NULL) {
            return NULL;
        }
    }

    return addr;
}

/* Helper for crossVerifyAnnotationsDirectoryItem(), which checks the
 * field elements. */
static const u1* crossVerifyFieldAnnotations(const CheckState* state, u4 count,
        const u1* addr, u4 definingClass) {
    const DexFieldAnnotationsItem* item = (DexFieldAnnotationsItem*) addr;

    while (count--) {
        if (!verifyFieldDefiner(state, definingClass, item->fieldIdx)) {
            return NULL;
        }
        if (!dexDataMapVerify(state->pDataMap, item->annotationsOff,
                        kDexTypeAnnotationSetItem)) {
            return NULL;
        }
        item++;
    }

    return (const u1*) item;
}

/* Helper for crossVerifyAnnotationsDirectoryItem(), which checks the
 * method elements. */
static const u1* crossVerifyMethodAnnotations(const CheckState* state,
        u4 count, const u1* addr, u4 definingClass) {
    const DexMethodAnnotationsItem* item = (DexMethodAnnotationsItem*) addr;

    while (count--) {
        if (!verifyMethodDefiner(state, definingClass, item->methodIdx)) {
            return NULL;
        }
        if (!dexDataMapVerify(state->pDataMap, item->annotationsOff,
                        kDexTypeAnnotationSetItem)) {
            return NULL;
        }
        item++;
    }

    return (const u1*) item;
}

/* Helper for crossVerifyAnnotationsDirectoryItem(), which checks the
 * parameter elements. */
static const u1* crossVerifyParameterAnnotations(const CheckState* state,
        u4 count, const u1* addr, u4 definingClass) {
    const DexParameterAnnotationsItem* item =
        (DexParameterAnnotationsItem*) addr;

    while (count--) {
        if (!verifyMethodDefiner(state, definingClass, item->methodIdx)) {
            return NULL;
        }
        if (!dexDataMapVerify(state->pDataMap, item->annotationsOff,
                        kDexTypeAnnotationSetRefList)) {
            return NULL;
        }
        item++;
    }

    return (const u1*) item;
}

/* Helper for crossVerifyClassDefItem() and
 * crossVerifyAnnotationsDirectoryItem(), which finds the type_idx of
 * the definer of the first item in the data. */
static u4 findFirstAnnotationsDirectoryDefiner(const CheckState* state,
        const DexAnnotationsDirectoryItem* dir) {
    if (dir->fieldsSize != 0) {
        const DexFieldAnnotationsItem* fields =
            dexGetFieldAnnotations(state->pDexFile, dir);
        const DexFieldId* field =
            dexGetFieldId(state->pDexFile, fields[0].fieldIdx);
        return field->classIdx;
    }

    if (dir->methodsSize != 0) {
        const DexMethodAnnotationsItem* methods =
            dexGetMethodAnnotations(state->pDexFile, dir);
        const DexMethodId* method =
            dexGetMethodId(state->pDexFile, methods[0].methodIdx);
        return method->classIdx;
    }

    if (dir->parametersSize != 0) {
        const DexParameterAnnotationsItem* parameters =
            dexGetParameterAnnotations(state->pDexFile, dir);
        const DexMethodId* method =
            dexGetMethodId(state->pDexFile, parameters[0].methodIdx);
        return method->classIdx;
    }

    return kDexNoIndex;
}

/* Perform cross-item verification of annotations_directory_item. */
static void* crossVerifyAnnotationsDirectoryItem(const CheckState* state,
        void* ptr) {
    const DexAnnotationsDirectoryItem* item = (const DexAnnotationsDirectoryItem*) ptr;
    u4 definingClass = findFirstAnnotationsDirectoryDefiner(state, item);

    if (!dexDataMapVerify0Ok(state->pDataMap,
                    item->classAnnotationsOff, kDexTypeAnnotationSetItem)) {
        return NULL;
    }

    const u1* addr = (const u1*) (item + 1);

    if (item->fieldsSize != 0) {
        addr = crossVerifyFieldAnnotations(state, item->fieldsSize, addr,
                definingClass);
        if (addr == NULL) {
            return NULL;
        }
    }

    if (item->methodsSize != 0) {
        addr = crossVerifyMethodAnnotations(state, item->methodsSize, addr,
                definingClass);
        if (addr == NULL) {
            return NULL;
        }
    }

    if (item->parametersSize != 0) {
        addr = crossVerifyParameterAnnotations(state, item->parametersSize,
                addr, definingClass);
        if (addr == NULL) {
            return NULL;
        }
    }

    return (void*) addr;
}

/* Perform byte-swapping and intra-item verification on type_list. */
static void* swapTypeList(const CheckState* state, void* ptr)
{
    DexTypeList* pTypeList = (DexTypeList*) ptr;
    DexTypeItem* pType;
    u4 count;

    CHECK_PTR_RANGE(pTypeList, pTypeList + 1);
    SWAP_FIELD4(pTypeList->size);
    count = pTypeList->size;
    pType = pTypeList->list;
    CHECK_LIST_SIZE(pType, count, sizeof(DexTypeItem));

    while (count--) {
        SWAP_INDEX2(pType->typeIdx, state->pHeader->typeIdsSize);
        pType++;
    }

    return pType;
}

/* Perform byte-swapping and intra-item verification on
 * annotation_set_ref_list. */
static void* swapAnnotationSetRefList(const CheckState* state, void* ptr) {
    DexAnnotationSetRefList* list = (DexAnnotationSetRefList*) ptr;
    DexAnnotationSetRefItem* item;
    u4 count;

    CHECK_PTR_RANGE(list, list + 1);
    SWAP_FIELD4(list->size);
    count = list->size;
    item = list->list;
    CHECK_LIST_SIZE(item, count, sizeof(DexAnnotationSetRefItem));

    while (count--) {
        SWAP_OFFSET4(item->annotationsOff);
        item++;
    }

    return item;
}

/* Perform cross-item verification of annotation_set_ref_list. */
static void* crossVerifyAnnotationSetRefList(const CheckState* state,
        void* ptr) {
    const DexAnnotationSetRefList* list = (const DexAnnotationSetRefList*) ptr;
    const DexAnnotationSetRefItem* item = list->list;
    int count = list->size;

    while (count--) {
        if (!dexDataMapVerify0Ok(state->pDataMap,
                        item->annotationsOff, kDexTypeAnnotationSetItem)) {
            return NULL;
        }
        item++;
    }

    return (void*) item;
}

/* Perform byte-swapping and intra-item verification on
 * annotation_set_item. */
static void* swapAnnotationSetItem(const CheckState* state, void* ptr) {
    DexAnnotationSetItem* set = (DexAnnotationSetItem*) ptr;
    u4* item;
    u4 count;

    CHECK_PTR_RANGE(set, set + 1);
    SWAP_FIELD4(set->size);
    count = set->size;
    item = set->entries;
    CHECK_LIST_SIZE(item, count, sizeof(u4));

    while (count--) {
        SWAP_OFFSET4(*item);
        item++;
    }

    return item;
}

/* Helper for crossVerifyAnnotationSetItem(), which extracts the type_idx
 * out of an annotation_item. */
static u4 annotationItemTypeIdx(const DexAnnotationItem* item) {
    const u1* data = item->annotation;
    return readUnsignedLeb128(&data);
}

/* Perform cross-item verification of annotation_set_item. */
static void* crossVerifyAnnotationSetItem(const CheckState* state, void* ptr) {
    const DexAnnotationSetItem* set = (const DexAnnotationSetItem*) ptr;
    int count = set->size;
    u4 lastIdx = 0;
    bool first = true;
    int i;

    for (i = 0; i < count; i++) {
        if (!dexDataMapVerify0Ok(state->pDataMap,
                        dexGetAnnotationOff(set, i), kDexTypeAnnotationItem)) {
            return NULL;
        }

        const DexAnnotationItem* annotation =
            dexGetAnnotationItem(state->pDexFile, set, i);
        u4 idx = annotationItemTypeIdx(annotation);

        if (first) {
            first = false;
        } else if (lastIdx >= idx) {
            ALOGE("Out-of-order entry types: %#x then %#x",
                    lastIdx, idx);
            return NULL;
        }

        lastIdx = idx;
    }

    return (void*) (set->entries + count);
}

/* Helper for verifyClassDataItem(), which checks a list of fields. */
static bool verifyFields(const CheckState* state, u4 size,
        DexField* fields, bool expectStatic) {
    u4 i;

    for (i = 0; i < size; i++) {
        DexField* field = &fields[i];
        u4 accessFlags = field->accessFlags;
        bool isStatic = (accessFlags & ACC_STATIC) != 0;

        CHECK_INDEX(field->fieldIdx, state->pHeader->fieldIdsSize);

        if (isStatic != expectStatic) {
            ALOGE("Field in wrong list @ %d", i);
            return false;
        }

        if ((accessFlags & ~ACC_FIELD_MASK) != 0) {
            // The VM specification says that unknown flags should be ignored.
            ALOGV("Bogus field access flags %x @ %d", accessFlags, i);
            field->accessFlags &= ACC_FIELD_MASK;
        }
    }

    return true;
}

/* Helper for verifyClassDataItem(), which checks a list of methods. */
static bool verifyMethods(const CheckState* state, u4 size,
        DexMethod* methods, bool expectDirect) {
    u4 i;

    for (i = 0; i < size; i++) {
        DexMethod* method = &methods[i];

        CHECK_INDEX(method->methodIdx, state->pHeader->methodIdsSize);

        u4 accessFlags = method->accessFlags;
        bool isDirect =
            (accessFlags & (ACC_STATIC | ACC_PRIVATE | ACC_CONSTRUCTOR)) != 0;
        bool expectCode = (accessFlags & (ACC_NATIVE | ACC_ABSTRACT)) == 0;
        bool isSynchronized = (accessFlags & ACC_SYNCHRONIZED) != 0;
        bool allowSynchronized = (accessFlags & ACC_NATIVE) != 0;

        if (isDirect != expectDirect) {
            ALOGE("Method in wrong list @ %d", i);
            return false;
        }

        if (isSynchronized && !allowSynchronized) {
            ALOGE("Bogus method access flags (synchronization) %x @ %d", accessFlags, i);
            return false;
        }

        if ((accessFlags & ~ACC_METHOD_MASK) != 0) {
            // The VM specification says that unknown flags should be ignored.
            ALOGV("Bogus method access flags %x @ %d", accessFlags, i);
            method->accessFlags &= ACC_METHOD_MASK;
        }

        if (expectCode) {
            if (method->codeOff == 0) {
                ALOGE("Unexpected zero code_off for access_flags %x",
                        accessFlags);
                return false;
            }
        } else if (method->codeOff != 0) {
            ALOGE("Unexpected non-zero code_off %#x for access_flags %x",
                    method->codeOff, accessFlags);
            return false;
        }
    }

    return true;
}

/* Helper for verifyClassDataItem(), which does most of the work. */
static bool verifyClassDataItem0(const CheckState* state,
        DexClassData* classData) {
    bool okay;

    okay = verifyFields(state, classData->header.staticFieldsSize,
            classData->staticFields, true);

    if (!okay) {
        ALOGE("Trouble with static fields");
        return false;
    }

    verifyFields(state, classData->header.instanceFieldsSize,
            classData->instanceFields, false);

    if (!okay) {
        ALOGE("Trouble with instance fields");
        return false;
    }

    okay = verifyMethods(state, classData->header.directMethodsSize,
            classData->directMethods, true);

    if (!okay) {
        ALOGE("Trouble with direct methods");
        return false;
    }

    okay = verifyMethods(state, classData->header.virtualMethodsSize,
            classData->virtualMethods, false);

    if (!okay) {
        ALOGE("Trouble with virtual methods");
        return false;
    }

    return true;
}

/* Perform intra-item verification on class_data_item. */
static void* intraVerifyClassDataItem(const CheckState* state, void* ptr) {
    const u1* data = (const u1*) ptr;
    DexClassData* classData = dexReadAndVerifyClassData(&data, state->fileEnd);

    if (classData == NULL) {
        ALOGE("Unable to parse class_data_item");
        return NULL;
    }

    bool okay = verifyClassDataItem0(state, classData);

    free(classData);

    if (!okay) {
        return NULL;
    }

    return (void*) data;
}

/* Helper for crossVerifyClassDefItem() and
 * crossVerifyClassDataItem(), which finds the type_idx of the definer
 * of the first item in the data. */
static u4 findFirstClassDataDefiner(const CheckState* state,
        DexClassData* classData) {
    if (classData->header.staticFieldsSize != 0) {
        u4 fieldIdx = classData->staticFields[0].fieldIdx;
        const DexFieldId* field = dexGetFieldId(state->pDexFile, fieldIdx);
        return field->classIdx;
    }

    if (classData->header.instanceFieldsSize != 0) {
        u4 fieldIdx = classData->instanceFields[0].fieldIdx;
        const DexFieldId* field = dexGetFieldId(state->pDexFile, fieldIdx);
        return field->classIdx;
    }

    if (classData->header.directMethodsSize != 0) {
        u4 methodIdx = classData->directMethods[0].methodIdx;
        const DexMethodId* meth = dexGetMethodId(state->pDexFile, methodIdx);
        return meth->classIdx;
    }

    if (classData->header.virtualMethodsSize != 0) {
        u4 methodIdx = classData->virtualMethods[0].methodIdx;
        const DexMethodId* meth = dexGetMethodId(state->pDexFile, methodIdx);
        return meth->classIdx;
    }

    return kDexNoIndex;
}

/* Perform cross-item verification of class_data_item. */
static void* crossVerifyClassDataItem(const CheckState* state, void* ptr) {
    const u1* data = (const u1*) ptr;
    DexClassData* classData = dexReadAndVerifyClassData(&data, state->fileEnd);
    u4 definingClass = findFirstClassDataDefiner(state, classData);
    bool okay = true;
    u4 i;

    for (i = classData->header.staticFieldsSize; okay && (i > 0); /*i*/) {
        i--;
        const DexField* field = &classData->staticFields[i];
        okay = verifyFieldDefiner(state, definingClass, field->fieldIdx);
    }

    for (i = classData->header.instanceFieldsSize; okay && (i > 0); /*i*/) {
        i--;
        const DexField* field = &classData->instanceFields[i];
        okay = verifyFieldDefiner(state, definingClass, field->fieldIdx);
    }

    for (i = classData->header.directMethodsSize; okay && (i > 0); /*i*/) {
        i--;
        const DexMethod* meth = &classData->directMethods[i];
        okay = dexDataMapVerify0Ok(state->pDataMap, meth->codeOff,
                kDexTypeCodeItem)
            && verifyMethodDefiner(state, definingClass, meth->methodIdx);
    }

    for (i = classData->header.virtualMethodsSize; okay && (i > 0); /*i*/) {
        i--;
        const DexMethod* meth = &classData->virtualMethods[i];
        okay = dexDataMapVerify0Ok(state->pDataMap, meth->codeOff,
                kDexTypeCodeItem)
            && verifyMethodDefiner(state, definingClass, meth->methodIdx);
    }

    free(classData);

    if (!okay) {
        return NULL;
    }

    return (void*) data;
}

/* Helper for swapCodeItem(), which fills an array with all the valid
 * handlerOff values for catch handlers and also verifies the handler
 * contents. */
static u4 setHandlerOffsAndVerify(const CheckState* state,
        DexCode* code, u4 firstOffset, u4 handlersSize, u4* handlerOffs) {
    const u1* fileEnd = state->fileEnd;
    const u1* handlersBase = dexGetCatchHandlerData(code);
    u4 offset = firstOffset;
    bool okay = true;
    u4 i;

    for (i = 0; i < handlersSize; i++) {
        const u1* ptr = handlersBase + offset;
        int size = readAndVerifySignedLeb128(&ptr, fileEnd, &okay);
        bool catchAll;

        if (!okay) {
            ALOGE("Bogus size");
            return 0;
        }

        if ((size < -65536) || (size > 65536)) {
            ALOGE("Invalid size: %d", size);
            return 0;
        }

        if (size <= 0) {
            catchAll = true;
            size = -size;
        } else {
            catchAll = false;
        }

        handlerOffs[i] = offset;

        while (size-- > 0) {
            u4 typeIdx =
                readAndVerifyUnsignedLeb128(&ptr, fileEnd, &okay);

            if (!okay) {
                ALOGE("Bogus type_idx");
                return 0;
            }

            CHECK_INDEX(typeIdx, state->pHeader->typeIdsSize);

            u4 addr = readAndVerifyUnsignedLeb128(&ptr, fileEnd, &okay);

            if (!okay) {
                ALOGE("Bogus addr");
                return 0;
            }

            if (addr >= code->insnsSize) {
                ALOGE("Invalid addr: %#x", addr);
                return 0;
            }
        }

        if (catchAll) {
            u4 addr = readAndVerifyUnsignedLeb128(&ptr, fileEnd, &okay);

            if (!okay) {
                ALOGE("Bogus catch_all_addr");
                return 0;
            }

            if (addr >= code->insnsSize) {
                ALOGE("Invalid catch_all_addr: %#x", addr);
                return 0;
            }
        }

        offset = ptr - handlersBase;
    }

    return offset;
}

/* Helper for swapCodeItem(), which does all the try-catch related
 * swapping and verification. */
static void* swapTriesAndCatches(const CheckState* state, DexCode* code) {
    const u1* encodedHandlers = dexGetCatchHandlerData(code);
    const u1* encodedPtr = encodedHandlers;
    bool okay = true;
    u4 handlersSize =
        readAndVerifyUnsignedLeb128(&encodedPtr, state->fileEnd, &okay);

    if (!okay) {
        ALOGE("Bogus handlers_size");
        return NULL;
    }

    if ((handlersSize == 0) || (handlersSize >= 65536)) {
        ALOGE("Invalid handlers_size: %d", handlersSize);
        return NULL;
    }

    u4 handlerOffs[handlersSize]; // list of valid handlerOff values
    u4 endOffset = setHandlerOffsAndVerify(state, code,
            encodedPtr - encodedHandlers,
            handlersSize, handlerOffs);

    if (endOffset == 0) {
        return NULL;
    }

    DexTry* tries = (DexTry*) dexGetTries(code);
    u4 count = code->triesSize;
    u4 lastEnd = 0;

    CHECK_LIST_SIZE(tries, count, sizeof(DexTry));

    while (count--) {
        u4 i;

        SWAP_FIELD4(tries->startAddr);
        SWAP_FIELD2(tries->insnCount);
        SWAP_FIELD2(tries->handlerOff);

        if (tries->startAddr < lastEnd) {
            ALOGE("Out-of-order try");
            return NULL;
        }

        if (tries->startAddr >= code->insnsSize) {
            ALOGE("Invalid start_addr: %#x", tries->startAddr);
            return NULL;
        }

        for (i = 0; i < handlersSize; i++) {
            if (tries->handlerOff == handlerOffs[i]) {
                break;
            }
        }

        if (i == handlersSize) {
            ALOGE("Bogus handler offset: %#x", tries->handlerOff);
            return NULL;
        }

        lastEnd = tries->startAddr + tries->insnCount;

        if (lastEnd > code->insnsSize) {
            ALOGE("Invalid insn_count: %#x (end addr %#x)",
                    tries->insnCount, lastEnd);
            return NULL;
        }

        tries++;
    }

    return (u1*) encodedHandlers + endOffset;
}

/* Perform byte-swapping and intra-item verification on code_item. */
static void* swapCodeItem(const CheckState* state, void* ptr) {
    DexCode* item = (DexCode*) ptr;
    u2* insns;
    u4 count;

    CHECK_PTR_RANGE(item, item + 1);
    SWAP_FIELD2(item->registersSize);
    SWAP_FIELD2(item->insSize);
    SWAP_FIELD2(item->outsSize);
    SWAP_FIELD2(item->triesSize);
    SWAP_OFFSET4(item->debugInfoOff);
    SWAP_FIELD4(item->insnsSize);

    if (item->insSize > item->registersSize) {
        ALOGE("insSize (%u) > registersSize (%u)", item->insSize,
                item->registersSize);
        return NULL;
    }

    if ((item->outsSize > 5) && (item->outsSize > item->registersSize)) {
        /*
         * It's okay for outsSize to be up to five, even if registersSize
         * is smaller, since the short forms of method invocation allow
         * repetition of a register multiple times within a single parameter
         * list. Longer parameter lists, though, need to be represented
         * in-order in the register file.
         */
        ALOGE("outsSize (%u) > registersSize (%u)", item->outsSize,
                item->registersSize);
        return NULL;
    }

    count = item->insnsSize;
    insns = item->insns;
    CHECK_LIST_SIZE(insns, count, sizeof(u2));

    while (count--) {
        *insns = SWAP2(*insns);
        insns++;
    }

    if (item->triesSize == 0) {
        ptr = insns;
    } else {
        if ((((uintptr_t) insns) & 3) != 0) {
            // Four-byte alignment for the tries. Verify the spacer is a 0.
            if (*insns != 0) {
                ALOGE("Non-zero padding: %#x", (u4) *insns);
                return NULL;
            }
        }

        ptr = swapTriesAndCatches(state, item);
    }

    return ptr;
}

/* Perform intra-item verification on string_data_item. */
static void* intraVerifyStringDataItem(const CheckState* state, void* ptr) {
    const u1* fileEnd = state->fileEnd;
    const u1* data = (const u1*) ptr;
    bool okay = true;
    u4 utf16Size = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
    u4 i;

    if (!okay) {
        ALOGE("Bogus utf16_size");
        return NULL;
    }

    for (i = 0; i < utf16Size; i++) {
        if (data >= fileEnd) {
            ALOGE("String data would go beyond end-of-file");
            return NULL;
        }

        u1 byte1 = *(data++);

        // Switch on the high four bits.
        switch (byte1 >> 4) {
            case 0x00: {
                // Special case of bit pattern 0xxx.
                if (byte1 == 0) {
                    ALOGE("String shorter than indicated utf16_size %#x",
                            utf16Size);
                    return NULL;
                }
                break;
            }
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07: {
                // Bit pattern 0xxx. No need for any extra bytes or checks.
                break;
            }
            case 0x08:
            case 0x09:
            case 0x0a:
            case 0x0b:
            case 0x0f: {
                /*
                 * Bit pattern 10xx or 1111, which are illegal start bytes.
                 * Note: 1111 is valid for normal UTF-8, but not the
                 * modified UTF-8 used here.
                 */
                ALOGE("Illegal start byte %#x", byte1);
                return NULL;
            }
            case 0x0e: {
                // Bit pattern 1110, so there are two additional bytes.
                u1 byte2 = *(data++);
                if ((byte2 & 0xc0) != 0x80) {
                    ALOGE("Illegal continuation byte %#x", byte2);
                    return NULL;
                }
                u1 byte3 = *(data++);
                if ((byte3 & 0xc0) != 0x80) {
                    ALOGE("Illegal continuation byte %#x", byte3);
                    return NULL;
                }
                u2 value = ((byte1 & 0x0f) << 12) | ((byte2 & 0x3f) << 6)
                    | (byte3 & 0x3f);
                if (value < 0x800) {
                    ALOGE("Illegal representation for value %x", value);
                    return NULL;
                }
                break;
            }
            case 0x0c:
            case 0x0d: {
                // Bit pattern 110x, so there is one additional byte.
                u1 byte2 = *(data++);
                if ((byte2 & 0xc0) != 0x80) {
                    ALOGE("Illegal continuation byte %#x", byte2);
                    return NULL;
                }
                u2 value = ((byte1 & 0x1f) << 6) | (byte2 & 0x3f);
                if ((value != 0) && (value < 0x80)) {
                    ALOGE("Illegal representation for value %x", value);
                    return NULL;
                }
                break;
            }
        }
    }

    if (*(data++) != '\0') {
        ALOGE("String longer than indicated utf16_size %#x", utf16Size);
        return NULL;
    }

    return (void*) data;
}

/* Perform intra-item verification on debug_info_item. */
static void* intraVerifyDebugInfoItem(const CheckState* state, void* ptr) {
    const u1* fileEnd = state->fileEnd;
    const u1* data = (const u1*) ptr;
    bool okay = true;
    u4 i;

    readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);

    if (!okay) {
        ALOGE("Bogus line_start");
        return NULL;
    }

    u4 parametersSize =
        readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);

    if (!okay) {
        ALOGE("Bogus parameters_size");
        return NULL;
    }

    if (parametersSize > 65536) {
        ALOGE("Invalid parameters_size: %#x", parametersSize);
        return NULL;
    }

    for (i = 0; i < parametersSize; i++) {
        u4 parameterName =
            readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);

        if (!okay) {
            ALOGE("Bogus parameter_name");
            return NULL;
        }

        if (parameterName != 0) {
            parameterName--;
            CHECK_INDEX(parameterName, state->pHeader->stringIdsSize);
        }
    }

    bool done = false;
    while (!done) {
        u1 opcode = *(data++);

        switch (opcode) {
            case DBG_END_SEQUENCE: {
                done = true;
                break;
            }
            case DBG_ADVANCE_PC: {
                readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                break;
            }
            case DBG_ADVANCE_LINE: {
                readAndVerifySignedLeb128(&data, fileEnd, &okay);
                break;
            }
            case DBG_START_LOCAL: {
                u4 idx;
                u4 regNum = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (regNum >= 65536) {
                    okay = false;
                    break;
                }
                idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (idx != 0) {
                    idx--;
                    CHECK_INDEX(idx, state->pHeader->stringIdsSize);
                }
                idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (idx != 0) {
                    idx--;
                    CHECK_INDEX(idx, state->pHeader->stringIdsSize);
                }
                break;
            }
            case DBG_END_LOCAL:
            case DBG_RESTART_LOCAL: {
                u4 regNum = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (regNum >= 65536) {
                    okay = false;
                    break;
                }
                break;
            }
            case DBG_START_LOCAL_EXTENDED: {
                u4 idx;
                u4 regNum = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (regNum >= 65536) {
                    okay = false;
                    break;
                }
                idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (idx != 0) {
                    idx--;
                    CHECK_INDEX(idx, state->pHeader->stringIdsSize);
                }
                idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (idx != 0) {
                    idx--;
                    CHECK_INDEX(idx, state->pHeader->stringIdsSize);
                }
                idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (idx != 0) {
                    idx--;
                    CHECK_INDEX(idx, state->pHeader->stringIdsSize);
                }
                break;
            }
            case DBG_SET_FILE: {
                u4 idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
                if (!okay) break;
                if (idx != 0) {
                    idx--;
                    CHECK_INDEX(idx, state->pHeader->stringIdsSize);
                }
                break;
            }
            default: {
                // No arguments to parse for anything else.
            }
        }

        if (!okay) {
            ALOGE("Bogus syntax for opcode %02x", opcode);
            return NULL;
        }
    }

    return (void*) data;
}

/* defined below */
static const u1* verifyEncodedValue(const CheckState* state, const u1* data,
        bool crossVerify);
static const u1* verifyEncodedAnnotation(const CheckState* state,
        const u1* data, bool crossVerify);

/* Helper for verifyEncodedValue(), which reads a 1- to 4- byte unsigned
 * little endian value. */
static u4 readUnsignedLittleEndian(const CheckState* state, const u1** pData,
        u4 size) {
    const u1* data = *pData;
    u4 result = 0;
    u4 i;

    CHECK_PTR_RANGE(data, data + size);

    for (i = 0; i < size; i++) {
        result |= ((u4) *(data++)) << (i * 8);
    }

    *pData = data;
    return result;
}

/* Helper for *VerifyAnnotationItem() and *VerifyEncodedArrayItem(), which
 * verifies an encoded_array. */
static const u1* verifyEncodedArray(const CheckState* state,
        const u1* data, bool crossVerify) {
    bool okay = true;
    u4 size = readAndVerifyUnsignedLeb128(&data, state->fileEnd, &okay);

    if (!okay) {
        ALOGE("Bogus encoded_array size");
        return NULL;
    }

    while (size--) {
        data = verifyEncodedValue(state, data, crossVerify);
        if (data == NULL) {
            ALOGE("Bogus encoded_array value");
            return NULL;
        }
    }

    return data;
}

/* Helper for *VerifyAnnotationItem() and *VerifyEncodedArrayItem(), which
 * verifies an encoded_value. */
static const u1* verifyEncodedValue(const CheckState* state,
        const u1* data, bool crossVerify) {
    CHECK_PTR_RANGE(data, data + 1);

    u1 headerByte = *(data++);
    u4 valueType = headerByte & kDexAnnotationValueTypeMask;
    u4 valueArg = headerByte >> kDexAnnotationValueArgShift;

    switch (valueType) {
        case kDexAnnotationByte: {
            if (valueArg != 0) {
                ALOGE("Bogus byte size %#x", valueArg);
                return NULL;
            }
            data++;
            break;
        }
        case kDexAnnotationShort:
        case kDexAnnotationChar: {
            if (valueArg > 1) {
                ALOGE("Bogus char/short size %#x", valueArg);
                return NULL;
            }
            data += valueArg + 1;
            break;
        }
        case kDexAnnotationInt:
        case kDexAnnotationFloat: {
            if (valueArg > 3) {
                ALOGE("Bogus int/float size %#x", valueArg);
                return NULL;
            }
            data += valueArg + 1;
            break;
        }
        case kDexAnnotationLong:
        case kDexAnnotationDouble: {
            data += valueArg + 1;
            break;
        }
        case kDexAnnotationString: {
            if (valueArg > 3) {
                ALOGE("Bogus string size %#x", valueArg);
                return NULL;
            }
            u4 idx = readUnsignedLittleEndian(state, &data, valueArg + 1);
            CHECK_INDEX(idx, state->pHeader->stringIdsSize);
            break;
        }
        case kDexAnnotationType: {
            if (valueArg > 3) {
                ALOGE("Bogus type size %#x", valueArg);
                return NULL;
            }
            u4 idx = readUnsignedLittleEndian(state, &data, valueArg + 1);
            CHECK_INDEX(idx, state->pHeader->typeIdsSize);
            break;
        }
        case kDexAnnotationField:
        case kDexAnnotationEnum: {
            if (valueArg > 3) {
                ALOGE("Bogus field/enum size %#x", valueArg);
                return NULL;
            }
            u4 idx = readUnsignedLittleEndian(state, &data, valueArg + 1);
            CHECK_INDEX(idx, state->pHeader->fieldIdsSize);
            break;
        }
        case kDexAnnotationMethod: {
            if (valueArg > 3) {
                ALOGE("Bogus method size %#x", valueArg);
                return NULL;
            }
            u4 idx = readUnsignedLittleEndian(state, &data, valueArg + 1);
            CHECK_INDEX(idx, state->pHeader->methodIdsSize);
            break;
        }
        case kDexAnnotationArray: {
            if (valueArg != 0) {
                ALOGE("Bogus array value_arg %#x", valueArg);
                return NULL;
            }
            data = verifyEncodedArray(state, data, crossVerify);
            break;
        }
        case kDexAnnotationAnnotation: {
            if (valueArg != 0) {
                ALOGE("Bogus annotation value_arg %#x", valueArg);
                return NULL;
            }
            data = verifyEncodedAnnotation(state, data, crossVerify);
            break;
        }
        case kDexAnnotationNull: {
            if (valueArg != 0) {
                ALOGE("Bogus null value_arg %#x", valueArg);
                return NULL;
            }
            // Nothing else to do for this type.
            break;
        }
        case kDexAnnotationBoolean: {
            if (valueArg > 1) {
                ALOGE("Bogus boolean value_arg %#x", valueArg);
                return NULL;
            }
            // Nothing else to do for this type.
            break;
        }
        default: {
            ALOGE("Bogus value_type %#x", valueType);
            return NULL;
        }
    }

    return data;
}

/* Helper for *VerifyAnnotationItem() and *VerifyEncodedArrayItem(), which
 * verifies an encoded_annotation. */
static const u1* verifyEncodedAnnotation(const CheckState* state,
        const u1* data, bool crossVerify) {
    const u1* fileEnd = state->fileEnd;
    bool okay = true;
    u4 idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);

    if (!okay) {
        ALOGE("Bogus encoded_annotation type_idx");
        return NULL;
    }

    CHECK_INDEX(idx, state->pHeader->typeIdsSize);

    if (crossVerify) {
        const char* descriptor = dexStringByTypeIdx(state->pDexFile, idx);
        if (!dexIsClassDescriptor(descriptor)) {
            ALOGE("Bogus annotation type: '%s'", descriptor);
            return NULL;
        }
    }

    u4 size = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);
    u4 lastIdx = 0;
    bool first = true;

    if (!okay) {
        ALOGE("Bogus encoded_annotation size");
        return NULL;
    }

    while (size--) {
        idx = readAndVerifyUnsignedLeb128(&data, fileEnd, &okay);

        if (!okay) {
            ALOGE("Bogus encoded_annotation name_idx");
            return NULL;
        }

        CHECK_INDEX(idx, state->pHeader->stringIdsSize);

        if (crossVerify) {
            const char* name = dexStringById(state->pDexFile, idx);
            if (!dexIsValidMemberName(name)) {
                ALOGE("Bogus annotation member name: '%s'", name);
                return NULL;
            }
        }

        if (first) {
            first = false;
        } else if (lastIdx >= idx) {
            ALOGE("Out-of-order encoded_annotation name_idx: %#x then %#x",
                    lastIdx, idx);
            return NULL;
        }

        data = verifyEncodedValue(state, data, crossVerify);
        lastIdx = idx;

        if (data == NULL) {
            return NULL;
        }
    }

    return data;
}

/* Perform intra-item verification on encoded_array_item. */
static void* intraVerifyEncodedArrayItem(const CheckState* state, void* ptr) {
    return (void*) verifyEncodedArray(state, (const u1*) ptr, false);
}

/* Perform intra-item verification on annotation_item. */
static void* intraVerifyAnnotationItem(const CheckState* state, void* ptr) {
    const u1* data = (const u1*) ptr;

    CHECK_PTR_RANGE(data, data + 1);

    switch (*(data++)) {
        case kDexVisibilityBuild:
        case kDexVisibilityRuntime:
        case kDexVisibilitySystem: {
            break;
        }
        default: {
            ALOGE("Bogus annotation visibility: %#x", *data);
            return NULL;
        }
    }

    return (void*) verifyEncodedAnnotation(state, data, false);
}

/* Perform cross-item verification on annotation_item. */
static void* crossVerifyAnnotationItem(const CheckState* state, void* ptr) {
    const u1* data = (const u1*) ptr;

    // Skip the visibility byte.
    data++;

    return (void*) verifyEncodedAnnotation(state, data, true);
}




/*
 * Function to visit an individual top-level item type.
 */
typedef void* ItemVisitorFunction(const CheckState* state, void* ptr);

/*
 * Iterate over all the items in a section, optionally updating the
 * data map (done if mapType is passed as non-negative). The section
 * must consist of concatenated items of the same type.
 */
static bool iterateSectionWithOptionalUpdate(CheckState* state,
        u4 offset, u4 count, ItemVisitorFunction* func, u4 alignment,
        u4* nextOffset, int mapType) {
    u4 alignmentMask = alignment - 1;
    u4 i;

    state->previousItem = NULL;

    for (i = 0; i < count; i++) {
        u4 newOffset = (offset + alignmentMask) & ~alignmentMask;
        u1* ptr = (u1*) filePointer(state, newOffset);

        if (offset < newOffset) {
            ptr = (u1*) filePointer(state, offset);
            if (offset < newOffset) {
                CHECK_OFFSET_RANGE(offset, newOffset);
                while (offset < newOffset) {
                    if (*ptr != '\0') {
                        ALOGE("Non-zero padding 0x%02x @ %x", *ptr, offset);
                        return false;
                    }
                    ptr++;
                    offset++;
                }
            }
        }

        u1* newPtr = (u1*) func(state, ptr);
        newOffset = fileOffset(state, newPtr);

        if (newPtr == NULL) {
            ALOGE("Trouble with item %d @ offset %#x", i, offset);
            return false;
        }

        if (newOffset > state->fileLen) {
            ALOGE("Item %d @ offset %#x ends out of bounds", i, offset);
            return false;
        }

        if (mapType >= 0) {
            dexDataMapAdd(state->pDataMap, offset, mapType);
        }

        state->previousItem = ptr;
        offset = newOffset;
    }

    if (nextOffset != NULL) {
        *nextOffset = offset;
    }

    return true;
}

/*
 * Iterate over all the items in a section. The section must consist of
 * concatenated items of the same type. This variant will not update the data
 * map.
 */
static bool iterateSection(CheckState* state, u4 offset, u4 count,
        ItemVisitorFunction* func, u4 alignment, u4* nextOffset) {
    return iterateSectionWithOptionalUpdate(state, offset, count, func,
            alignment, nextOffset, -1);
}

/*
 * Like iterateSection(), but also check that the offset and count match
 * a given pair of expected values.
 */
static bool checkBoundsAndIterateSection(CheckState* state,
        u4 offset, u4 count, u4 expectedOffset, u4 expectedCount,
        ItemVisitorFunction* func, u4 alignment, u4* nextOffset) {
    if (offset != expectedOffset) {
        ALOGE("Bogus offset for section: got %#x; expected %#x",
                offset, expectedOffset);
        return false;
    }

    if (count != expectedCount) {
        ALOGE("Bogus size for section: got %#x; expected %#x",
                count, expectedCount);
        return false;
    }

    return iterateSection(state, offset, count, func, alignment, nextOffset);
}

/*
 * Like iterateSection(), but also update the data section map and
 * check that all the items fall within the data section.
 */
static bool iterateDataSection(CheckState* state, u4 offset, u4 count,
        ItemVisitorFunction* func, u4 alignment, u4* nextOffset, int mapType) {
    u4 dataStart = state->pHeader->dataOff;
    u4 dataEnd = dataStart + state->pHeader->dataSize;

    assert(nextOffset != NULL);

    if ((offset < dataStart) || (offset >= dataEnd)) {
        ALOGE("Bogus offset for data subsection: %#x", offset);
        return false;
    }

    if (!iterateSectionWithOptionalUpdate(state, offset, count, func,
                    alignment, nextOffset, mapType)) {
        return false;
    }

    if (*nextOffset > dataEnd) {
        ALOGE("Out-of-bounds end of data subsection: %#x", *nextOffset);
        return false;
    }

    return true;
}

/*
 * Byte-swap all items in the given map except the header and the map
 * itself, both of which should have already gotten swapped. This also
 * does all possible intra-item verification, that is, verification
 * that doesn't need to assume the sanctity of the contents of *other*
 * items. The intra-item limitation is because at the time an item is
 * asked to verify itself, it can't assume that the items it refers to
 * have been byte-swapped and verified.
 */
static bool swapEverythingButHeaderAndMap(CheckState* state,
        DexMapList* pMap) {
    const DexMapItem* item = pMap->list;
    u4 lastOffset = 0;
    u4 count = pMap->size;
    bool okay = true;

    while (okay && count--) {
        u4 sectionOffset = item->offset;
        u4 sectionCount = item->size;
        u2 type = item->type;

        if (lastOffset < sectionOffset) {
            CHECK_OFFSET_RANGE(lastOffset, sectionOffset);
            const u1* ptr = (const u1*) filePointer(state, lastOffset);
            while (lastOffset < sectionOffset) {
                if (*ptr != '\0') {
                    ALOGE("Non-zero padding 0x%02x before section start @ %x",
                            *ptr, lastOffset);
                    okay = false;
                    break;
                }
                ptr++;
                lastOffset++;
            }
        } else if (lastOffset > sectionOffset) {
            ALOGE("Section overlap or out-of-order map: %x, %x",
                    lastOffset, sectionOffset);
            okay = false;
        }

        if (!okay) {
            break;
        }

        switch (type) {
            case kDexTypeHeaderItem: {
                /*
                 * The header got swapped very early on, but do some
                 * additional sanity checking here.
                 */
                okay = checkHeaderSection(state, sectionOffset, sectionCount,
                        &lastOffset);
                break;
            }
            case kDexTypeStringIdItem: {
                okay = checkBoundsAndIterateSection(state, sectionOffset,
                        sectionCount, state->pHeader->stringIdsOff,
                        state->pHeader->stringIdsSize, swapStringIdItem,
                        sizeof(u4), &lastOffset);
                break;
            }
            case kDexTypeTypeIdItem: {
                okay = checkBoundsAndIterateSection(state, sectionOffset,
                        sectionCount, state->pHeader->typeIdsOff,
                        state->pHeader->typeIdsSize, swapTypeIdItem,
                        sizeof(u4), &lastOffset);
                break;
            }
            case kDexTypeProtoIdItem: {
                okay = checkBoundsAndIterateSection(state, sectionOffset,
                        sectionCount, state->pHeader->protoIdsOff,
                        state->pHeader->protoIdsSize, swapProtoIdItem,
                        sizeof(u4), &lastOffset);
                break;
            }
            case kDexTypeFieldIdItem: {
                okay = checkBoundsAndIterateSection(state, sectionOffset,
                        sectionCount, state->pHeader->fieldIdsOff,
                        state->pHeader->fieldIdsSize, swapFieldIdItem,
                        sizeof(u4), &lastOffset);
                break;
            }
            case kDexTypeMethodIdItem: {
                okay = checkBoundsAndIterateSection(state, sectionOffset,
                        sectionCount, state->pHeader->methodIdsOff,
                        state->pHeader->methodIdsSize, swapMethodIdItem,
                        sizeof(u4), &lastOffset);
                break;
            }
            case kDexTypeClassDefItem: {
                okay = checkBoundsAndIterateSection(state, sectionOffset,
                        sectionCount, state->pHeader->classDefsOff,
                        state->pHeader->classDefsSize, swapClassDefItem,
                        sizeof(u4), &lastOffset);
                break;
            }
            case kDexTypeMapList: {
                /*
                 * The map section was swapped early on, but do some
                 * additional sanity checking here.
                 */
                okay = checkMapSection(state, sectionOffset, sectionCount,
                        &lastOffset);
                break;
            }
            case kDexTypeTypeList: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        swapTypeList, sizeof(u4), &lastOffset, type);
                break;
            }
            case kDexTypeAnnotationSetRefList: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        swapAnnotationSetRefList, sizeof(u4), &lastOffset,
                        type);
                break;
            }
            case kDexTypeAnnotationSetItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        swapAnnotationSetItem, sizeof(u4), &lastOffset, type);
                break;
            }
            case kDexTypeClassDataItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        intraVerifyClassDataItem, sizeof(u1), &lastOffset,
                        type);
                break;
            }
            case kDexTypeCodeItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        swapCodeItem, sizeof(u4), &lastOffset, type);
                break;
            }
            case kDexTypeStringDataItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        intraVerifyStringDataItem, sizeof(u1), &lastOffset,
                        type);
                break;
            }
            case kDexTypeDebugInfoItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        intraVerifyDebugInfoItem, sizeof(u1), &lastOffset,
                        type);
                break;
            }
            case kDexTypeAnnotationItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        intraVerifyAnnotationItem, sizeof(u1), &lastOffset,
                        type);
                break;
            }
            case kDexTypeEncodedArrayItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        intraVerifyEncodedArrayItem, sizeof(u1), &lastOffset,
                        type);
                break;
            }
            case kDexTypeAnnotationsDirectoryItem: {
                okay = iterateDataSection(state, sectionOffset, sectionCount,
                        swapAnnotationsDirectoryItem, sizeof(u4), &lastOffset,
                        type);
                break;
            }
            default: {
                ALOGE("Unknown map item type %04x", type);
                return false;
            }
        }

        if (!okay) {
            ALOGE("Swap of section type %04x failed", type);
        }

        item++;
    }

    return okay;
}

/*
 * Perform cross-item verification on everything that needs it. This
 * pass is only called after all items are byte-swapped and
 * intra-verified (checked for internal consistency).
 */
static bool crossVerifyEverything(CheckState* state, DexMapList* pMap)
{
    const DexMapItem* item = pMap->list;
    u4 count = pMap->size;
    bool okay = true;

    while (okay && count--) {
        u4 sectionOffset = item->offset;
        u4 sectionCount = item->size;

        switch (item->type) {
            case kDexTypeHeaderItem:
            case kDexTypeMapList:
            case kDexTypeTypeList:
            case kDexTypeCodeItem:
            case kDexTypeStringDataItem:
            case kDexTypeDebugInfoItem:
            case kDexTypeAnnotationItem:
            case kDexTypeEncodedArrayItem: {
                // There is no need for cross-item verification for these.
                break;
            }
            case kDexTypeStringIdItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyStringIdItem, sizeof(u4), NULL);
                break;
            }
            case kDexTypeTypeIdItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyTypeIdItem, sizeof(u4), NULL);
                break;
            }
            case kDexTypeProtoIdItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyProtoIdItem, sizeof(u4), NULL);
                break;
            }
            case kDexTypeFieldIdItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyFieldIdItem, sizeof(u4), NULL);
                break;
            }
            case kDexTypeMethodIdItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyMethodIdItem, sizeof(u4), NULL);
                break;
            }
            case kDexTypeClassDefItem: {
                // Allocate (on the stack) the "observed class_def" bits.
                size_t arraySize = calcDefinedClassBitsSize(state);
                u4 definedClassBits[arraySize];
                memset(definedClassBits, 0, arraySize * sizeof(u4));
                state->pDefinedClassBits = definedClassBits;

                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyClassDefItem, sizeof(u4), NULL);

                state->pDefinedClassBits = NULL;
                break;
            }
            case kDexTypeAnnotationSetRefList: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyAnnotationSetRefList, sizeof(u4), NULL);
                break;
            }
            case kDexTypeAnnotationSetItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyAnnotationSetItem, sizeof(u4), NULL);
                break;
            }
            case kDexTypeClassDataItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyClassDataItem, sizeof(u1), NULL);
                break;
            }
            case kDexTypeAnnotationsDirectoryItem: {
                okay = iterateSection(state, sectionOffset, sectionCount,
                        crossVerifyAnnotationsDirectoryItem, sizeof(u4), NULL);
                break;
            }
            default: {
                ALOGE("Unknown map item type %04x", item->type);
                return false;
            }
        }

        if (!okay) {
            ALOGE("Cross-item verify of section type %04x failed",
                    item->type);
        }

        item++;
    }

    return okay;
}

/* (documented in header file) */
bool dexHasValidMagic(const DexHeader* pHeader)
{
    const u1* magic = pHeader->magic;
    const u1* version = &magic[4];

    if (memcmp(magic, DEX_MAGIC, 4) != 0) {
        ALOGE("ERROR: unrecognized magic number (%02x %02x %02x %02x)",
            magic[0], magic[1], magic[2], magic[3]);
        return false;
    }

    if ((memcmp(version, DEX_MAGIC_VERS, 4) != 0) &&
            (memcmp(version, DEX_MAGIC_VERS_API_13, 4) != 0)) {
        /*
         * Magic was correct, but this is an unsupported older or
         * newer format variant.
         */
        ALOGE("ERROR: unsupported dex version (%02x %02x %02x %02x)",
            version[0], version[1], version[2], version[3]);
        return false;
    }

    return true;
}

/*
 * Fix the byte ordering of all fields in the DEX file, and do
 * structural verification. This is only required for code that opens
 * "raw" DEX files, such as the DEX optimizer.
 *
 * Returns 0 on success, nonzero on failure.
 */
int dexSwapAndVerify(u1* addr, int len)
{
    DexHeader* pHeader;
    CheckState state;
    bool okay = true;

    memset(&state, 0, sizeof(state));
    ALOGV("+++ swapping and verifying");

    /*
     * Note: The caller must have verified that "len" is at least as
     * large as a dex file header.
     */
    pHeader = (DexHeader*) addr;

    if (!dexHasValidMagic(pHeader)) {
        okay = false;
    }

    if (okay) {
        int expectedLen = (int) SWAP4(pHeader->fileSize);
        if (len < expectedLen) {
            ALOGE("ERROR: Bad length: expected %d, got %d", expectedLen, len);
            okay = false;
        } else if (len != expectedLen) {
            ALOGW("WARNING: Odd length: expected %d, got %d", expectedLen,
                    len);
            // keep going
        }
    }

    if (okay) {
        /*
         * Compute the adler32 checksum and compare it to what's stored in
         * the file.  This isn't free, but chances are good that we just
         * unpacked this from a jar file and have all of the pages sitting
         * in memory, so it's pretty quick.
         *
         * This might be a big-endian system, so we need to do this before
         * we byte-swap the header.
         */
        uLong adler = adler32(0L, Z_NULL, 0);
        const int nonSum = sizeof(pHeader->magic) + sizeof(pHeader->checksum);
        u4 storedFileSize = SWAP4(pHeader->fileSize);
        u4 expectedChecksum = SWAP4(pHeader->checksum);

        adler = adler32(adler, ((const u1*) pHeader) + nonSum,
                    storedFileSize - nonSum);

        if (adler != expectedChecksum) {
            ALOGE("ERROR: bad checksum (%08lx, expected %08x)",
                adler, expectedChecksum);
            okay = false;
        }
    }

    if (okay) {
        state.fileStart = addr;
        state.fileEnd = addr + len;
        state.fileLen = len;
        state.pDexFile = NULL;
        state.pDataMap = NULL;
        state.pDefinedClassBits = NULL;
        state.previousItem = NULL;

        /*
         * Swap the header and check the contents.
         */
        okay = swapDexHeader(&state, pHeader);
    }

    if (okay) {
        state.pHeader = pHeader;

        if (pHeader->headerSize < sizeof(DexHeader)) {
            ALOGE("ERROR: Small header size %d, struct %d",
                    pHeader->headerSize, (int) sizeof(DexHeader));
            okay = false;
        } else if (pHeader->headerSize > sizeof(DexHeader)) {
            ALOGW("WARNING: Large header size %d, struct %d",
                    pHeader->headerSize, (int) sizeof(DexHeader));
            // keep going?
        }
    }

    if (okay) {
        /*
         * Look for the map. Swap it and then use it to find and swap
         * everything else.
         */
        if (pHeader->mapOff != 0) {
            DexFile dexFile;
            DexMapList* pDexMap = (DexMapList*) (addr + pHeader->mapOff);

            okay = okay && swapMap(&state, pDexMap);
            okay = okay && swapEverythingButHeaderAndMap(&state, pDexMap);

            dexFileSetupBasicPointers(&dexFile, addr);
            state.pDexFile = &dexFile;

            okay = okay && crossVerifyEverything(&state, pDexMap);
        } else {
            ALOGE("ERROR: No map found; impossible to byte-swap and verify");
            okay = false;
        }
    }

    if (!okay) {
        ALOGE("ERROR: Byte swap + verify failed");
    }

    if (state.pDataMap != NULL) {
        dexDataMapFree(state.pDataMap);
    }

    return !okay;       // 0 == success
}

/*
 * Detect the file type of the given memory buffer via magic number.
 * Call dexSwapAndVerify() on an unoptimized DEX file, do nothing
 * but return successfully on an optimized DEX file, and report an
 * error for all other cases.
 *
 * Returns 0 on success, nonzero on failure.
 */
int dexSwapAndVerifyIfNecessary(u1* addr, int len)
{
    if (memcmp(addr, DEX_OPT_MAGIC, 4) == 0) {
        // It is an optimized dex file.
        return 0;
    }

    if (memcmp(addr, DEX_MAGIC, 4) == 0) {
        // It is an unoptimized dex file.
        return dexSwapAndVerify(addr, len);
    }

    ALOGE("ERROR: Bad magic number (0x%02x %02x %02x %02x)",
             addr[0], addr[1], addr[2], addr[3]);

    return 1;
}
