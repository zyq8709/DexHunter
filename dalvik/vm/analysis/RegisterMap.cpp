/*
 * Copyright (C) 2009 The Android Open Source Project
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
 * This code generate "register maps" for Dalvik bytecode.  In a stack-based
 * VM we might call these "stack maps".  They are used to increase the
 * precision in the garbage collector when scanning references in the
 * interpreter thread stacks.
 */
#include "Dalvik.h"
#include "UniquePtr.h"
#include "analysis/CodeVerify.h"
#include "analysis/RegisterMap.h"
#include "libdex/DexCatch.h"
#include "libdex/InstrUtils.h"
#include "libdex/Leb128.h"

#include <stddef.h>

/* double-check the compression */
#define REGISTER_MAP_VERIFY     false

/* verbose logging */
#define REGISTER_MAP_VERBOSE    false

//#define REGISTER_MAP_STATS

// fwd
static void outputTypeVector(const RegType* regs, int insnRegCount, u1* data);
static bool verifyMap(VerifierData* vdata, const RegisterMap* pMap);
static int compareMaps(const RegisterMap* pMap1, const RegisterMap* pMap2);

#ifdef REGISTER_MAP_STATS
static void computeMapStats(RegisterMap* pMap, const Method* method);
#endif
static RegisterMap* compressMapDifferential(const RegisterMap* pMap,\
    const Method* meth);
static RegisterMap* uncompressMapDifferential(const RegisterMap* pMap);

#ifdef REGISTER_MAP_STATS
/*
 * Generate some statistics on the register maps we create and use.
 */
#define kMaxGcPointGap      50
#define kUpdatePosnMinRegs  24
#define kNumUpdatePosns     8
#define kMaxDiffBits        20
struct MapStats {
    /*
     * Buckets measuring the distance between GC points.  This tells us how
     * many bits we need to encode the advancing program counter.  We ignore
     * some of the "long tail" entries.
     */
    int gcPointGap[kMaxGcPointGap];

    /*
     * Number of gaps.  Equal to (number of gcPoints - number of methods),
     * since the computation isn't including the initial gap.
     */
    int gcGapCount;

    /*
     * Number of gaps.
     */
    int totalGcPointCount;

    /*
     * For larger methods (>= 24 registers), measure in which octant register
     * updates occur.  This should help us understand whether register
     * changes tend to cluster in the low regs even for large methods.
     */
    int updatePosn[kNumUpdatePosns];

    /*
     * For all methods, count up the number of changes to registers < 16
     * and >= 16.
     */
    int updateLT16;
    int updateGE16;

    /*
     * Histogram of the number of bits that differ between adjacent entries.
     */
    int numDiffBits[kMaxDiffBits];


    /*
     * Track the number of expanded maps, and the heap space required to
     * hold them.
     */
    int numExpandedMaps;
    int totalExpandedMapSize;
};
#endif

/*
 * Prepare some things.
 */
bool dvmRegisterMapStartup()
{
#ifdef REGISTER_MAP_STATS
    MapStats* pStats = calloc(1, sizeof(MapStats));
    gDvm.registerMapStats = pStats;
#endif
    return true;
}

/*
 * Clean up.
 */
void dvmRegisterMapShutdown()
{
#ifdef REGISTER_MAP_STATS
    free(gDvm.registerMapStats);
#endif
}

/*
 * Write stats to log file.
 */
void dvmRegisterMapDumpStats()
{
#ifdef REGISTER_MAP_STATS
    MapStats* pStats = (MapStats*) gDvm.registerMapStats;
    int i, end;

    for (end = kMaxGcPointGap-1; end >= 0; end--) {
        if (pStats->gcPointGap[end] != 0)
            break;
    }

    ALOGI("Register Map gcPointGap stats (diff count=%d, total=%d):",
        pStats->gcGapCount, pStats->totalGcPointCount);
    assert(pStats->gcPointGap[0] == 0);
    for (i = 1; i <= end; i++) {
        ALOGI(" %2d %d", i, pStats->gcPointGap[i]);
    }


    for (end = kMaxDiffBits-1; end >= 0; end--) {
        if (pStats->numDiffBits[end] != 0)
            break;
    }

    ALOGI("Register Map bit difference stats:");
    for (i = 0; i <= end; i++) {
        ALOGI(" %2d %d", i, pStats->numDiffBits[i]);
    }


    ALOGI("Register Map update position stats (lt16=%d ge16=%d):",
        pStats->updateLT16, pStats->updateGE16);
    for (i = 0; i < kNumUpdatePosns; i++) {
        ALOGI(" %2d %d", i, pStats->updatePosn[i]);
    }
#endif
}


/*
 * ===========================================================================
 *      Map generation
 * ===========================================================================
 */

/*
 * Generate the register map for a method that has just been verified
 * (i.e. we're doing this as part of verification).
 *
 * For type-precise determination we have all the data we need, so we
 * just need to encode it in some clever fashion.
 *
 * Returns a pointer to a newly-allocated RegisterMap, or NULL on failure.
 */
RegisterMap* dvmGenerateRegisterMapV(VerifierData* vdata)
{
    static const int kHeaderSize = offsetof(RegisterMap, data);
    RegisterMap* pMap = NULL;
    RegisterMap* pResult = NULL;
    RegisterMapFormat format;
    u1 regWidth;
    u1* mapData;
    int i, bytesForAddr, gcPointCount;
    int bufSize;

    if (vdata->method->registersSize >= 2048) {
        ALOGE("ERROR: register map can't handle %d registers",
            vdata->method->registersSize);
        goto bail;
    }
    regWidth = (vdata->method->registersSize + 7) / 8;

    /*
     * Decide if we need 8 or 16 bits to hold the address.  Strictly speaking
     * we only need 16 bits if we actually encode an address >= 256 -- if
     * the method has a section at the end without GC points (e.g. array
     * data) we don't need to count it.  The situation is unusual, and
     * detecting it requires scanning the entire method, so we don't bother.
     */
    if (vdata->insnsSize < 256) {
        format = kRegMapFormatCompact8;
        bytesForAddr = 1;
    } else {
        format = kRegMapFormatCompact16;
        bytesForAddr = 2;
    }

    /*
     * Count up the number of GC point instructions.
     *
     * NOTE: this does not automatically include the first instruction,
     * since we don't count method entry as a GC point.
     */
    gcPointCount = 0;
    for (i = 0; i < (int) vdata->insnsSize; i++) {
        if (dvmInsnIsGcPoint(vdata->insnFlags, i))
            gcPointCount++;
    }
    if (gcPointCount >= 65536) {
        /* we could handle this, but in practice we don't get near this */
        ALOGE("ERROR: register map can't handle %d gc points in one method",
            gcPointCount);
        goto bail;
    }

    /*
     * Allocate a buffer to hold the map data.
     */
    bufSize = kHeaderSize + gcPointCount * (bytesForAddr + regWidth);

    ALOGV("+++ grm: %s.%s (adr=%d gpc=%d rwd=%d bsz=%d)",
        vdata->method->clazz->descriptor, vdata->method->name,
        bytesForAddr, gcPointCount, regWidth, bufSize);

    pMap = (RegisterMap*) malloc(bufSize);
    dvmRegisterMapSetFormat(pMap, format);
    dvmRegisterMapSetOnHeap(pMap, true);
    dvmRegisterMapSetRegWidth(pMap, regWidth);
    dvmRegisterMapSetNumEntries(pMap, gcPointCount);

    /*
     * Populate it.
     */
    mapData = pMap->data;
    for (i = 0; i < (int) vdata->insnsSize; i++) {
        if (dvmInsnIsGcPoint(vdata->insnFlags, i)) {
            assert(vdata->registerLines[i].regTypes != NULL);
            if (format == kRegMapFormatCompact8) {
                *mapData++ = i;
            } else /*kRegMapFormatCompact16*/ {
                *mapData++ = i & 0xff;
                *mapData++ = i >> 8;
            }
            outputTypeVector(vdata->registerLines[i].regTypes,
                vdata->insnRegCount, mapData);
            mapData += regWidth;
        }
    }

    ALOGV("mapData=%p pMap=%p bufSize=%d", mapData, pMap, bufSize);
    assert(mapData - (const u1*) pMap == bufSize);

    if (REGISTER_MAP_VERIFY && !verifyMap(vdata, pMap))
        goto bail;
#ifdef REGISTER_MAP_STATS
    computeMapStats(pMap, vdata->method);
#endif

    /*
     * Try to compress the map.
     */
    RegisterMap* pCompMap;

    pCompMap = compressMapDifferential(pMap, vdata->method);
    if (pCompMap != NULL) {
        if (REGISTER_MAP_VERIFY) {
            /*
             * Expand the compressed map we just created, and compare it
             * to the original.  Abort the VM if it doesn't match up.
             */
            RegisterMap* pUncompMap;
            pUncompMap = uncompressMapDifferential(pCompMap);
            if (pUncompMap == NULL) {
                ALOGE("Map failed to uncompress - %s.%s",
                    vdata->method->clazz->descriptor,
                    vdata->method->name);
                free(pCompMap);
                /* bad - compression is broken or we're out of memory */
                dvmAbort();
            } else {
                if (compareMaps(pMap, pUncompMap) != 0) {
                    ALOGE("Map comparison failed - %s.%s",
                        vdata->method->clazz->descriptor,
                        vdata->method->name);
                    free(pCompMap);
                    /* bad - compression is broken */
                    dvmAbort();
                }

                /* verify succeeded */
                delete pUncompMap;
            }
        }

        if (REGISTER_MAP_VERBOSE) {
            ALOGD("Good compress on %s.%s",
                vdata->method->clazz->descriptor,
                vdata->method->name);
        }
        free(pMap);
        pMap = pCompMap;
    } else {
        if (REGISTER_MAP_VERBOSE) {
            ALOGD("Unable to compress %s.%s (ent=%d rw=%d)",
                vdata->method->clazz->descriptor,
                vdata->method->name,
                dvmRegisterMapGetNumEntries(pMap),
                dvmRegisterMapGetRegWidth(pMap));
        }
    }

    pResult = pMap;

bail:
    return pResult;
}

/*
 * Release the storage held by a RegisterMap.
 */
void dvmFreeRegisterMap(RegisterMap* pMap)
{
    if (pMap == NULL)
        return;

    assert(dvmRegisterMapGetOnHeap(pMap));
    free(pMap);
}

/*
 * Determine if the RegType value is a reference type.
 *
 * Ordinarily we include kRegTypeZero in the "is it a reference"
 * check.  There's no value in doing so here, because we know
 * the register can't hold anything but zero.
 */
static inline bool isReferenceType(RegType type)
{
    return (type > kRegTypeMAX || type == kRegTypeUninit);
}

/*
 * Given a line of registers, output a bit vector that indicates whether
 * or not the register holds a reference type (which could be null).
 *
 * We use '1' to indicate it's a reference, '0' for anything else (numeric
 * value, uninitialized data, merge conflict).  Register 0 will be found
 * in the low bit of the first byte.
 */
static void outputTypeVector(const RegType* regs, int insnRegCount, u1* data)
{
    u1 val = 0;
    int i;

    for (i = 0; i < insnRegCount; i++) {
        RegType type = *regs++;
        val >>= 1;
        if (isReferenceType(type))
            val |= 0x80;        /* set hi bit */

        if ((i & 0x07) == 7)
            *data++ = val;
    }
    if ((i & 0x07) != 0) {
        /* flush bits from last byte */
        val >>= 8 - (i & 0x07);
        *data++ = val;
    }
}

/*
 * Print the map as a series of binary strings.
 *
 * Pass in method->registersSize if known, or -1 if not.
 */
static void dumpRegisterMap(const RegisterMap* pMap, int registersSize)
{
    const u1* rawMap = pMap->data;
    const RegisterMapFormat format = dvmRegisterMapGetFormat(pMap);
    const int numEntries = dvmRegisterMapGetNumEntries(pMap);
    const int regWidth = dvmRegisterMapGetRegWidth(pMap);
    int addrWidth;

    switch (format) {
    case kRegMapFormatCompact8:
        addrWidth = 1;
        break;
    case kRegMapFormatCompact16:
        addrWidth = 2;
        break;
    default:
        /* can't happen */
        ALOGE("Can only dump Compact8 / Compact16 maps (not %d)", format);
        return;
    }

    if (registersSize < 0)
        registersSize = 8 * regWidth;
    assert(registersSize <= regWidth * 8);

    int ent;
    for (ent = 0; ent < numEntries; ent++) {
        int i, addr;

        addr = *rawMap++;
        if (addrWidth > 1)
            addr |= (*rawMap++) << 8;

        const u1* dataStart = rawMap;
        u1 val = 0;

        /* create binary string */
        char outBuf[registersSize +1];
        for (i = 0; i < registersSize; i++) {
            val >>= 1;
            if ((i & 0x07) == 0)
                val = *rawMap++;

            outBuf[i] = '0' + (val & 0x01);
        }
        outBuf[i] = '\0';

        /* back up and create hex dump */
        char hexBuf[regWidth * 3 +1];
        char* cp = hexBuf;
        rawMap = dataStart;
        for (i = 0; i < regWidth; i++) {
            sprintf(cp, " %02x", *rawMap++);
            cp += 3;
        }
        hexBuf[i * 3] = '\0';

        ALOGD("  %04x %s %s", addr, outBuf, hexBuf);
    }
}

/*
 * Double-check the map.
 *
 * We run through all of the data in the map, and compare it to the original.
 * Only works on uncompressed data.
 */
static bool verifyMap(VerifierData* vdata, const RegisterMap* pMap)
{
    const u1* rawMap = pMap->data;
    const RegisterMapFormat format = dvmRegisterMapGetFormat(pMap);
    const int numEntries = dvmRegisterMapGetNumEntries(pMap);
    int ent;
    bool dumpMap = false;

    if (false) {
        const char* cd = "Landroid/net/http/Request;";
        const char* mn = "readResponse";
        if (strcmp(vdata->method->clazz->descriptor, cd) == 0 &&
            strcmp(vdata->method->name, mn) == 0)
        {
            char* desc;
            desc = dexProtoCopyMethodDescriptor(&vdata->method->prototype);
            ALOGI("Map for %s.%s %s", vdata->method->clazz->descriptor,
                vdata->method->name, desc);
            free(desc);

            dumpMap = true;
        }
    }

    if ((vdata->method->registersSize + 7) / 8 != pMap->regWidth) {
        ALOGE("GLITCH: registersSize=%d, regWidth=%d",
            vdata->method->registersSize, pMap->regWidth);
        return false;
    }

    for (ent = 0; ent < numEntries; ent++) {
        int addr;

        switch (format) {
        case kRegMapFormatCompact8:
            addr = *rawMap++;
            break;
        case kRegMapFormatCompact16:
            addr = *rawMap++;
            addr |= (*rawMap++) << 8;
            break;
        default:
            /* shouldn't happen */
            ALOGE("GLITCH: bad format (%d)", format);
            dvmAbort();
            /* Make compiler happy */
            addr = 0;
        }

        const RegType* regs = vdata->registerLines[addr].regTypes;
        if (regs == NULL) {
            ALOGE("GLITCH: addr %d has no data", addr);
            return false;
        }

        u1 val = 0;
        int i;

        for (i = 0; i < vdata->method->registersSize; i++) {
            bool bitIsRef, regIsRef;

            val >>= 1;
            if ((i & 0x07) == 0) {
                /* load next byte of data */
                val = *rawMap++;
            }

            bitIsRef = val & 0x01;

            RegType type = regs[i];
            regIsRef = isReferenceType(type);

            if (bitIsRef != regIsRef) {
                ALOGE("GLITCH: addr %d reg %d: bit=%d reg=%d(%d)",
                    addr, i, bitIsRef, regIsRef, type);
                return false;
            }
        }

        /* rawMap now points to the address field of the next entry */
    }

    if (dumpMap)
        dumpRegisterMap(pMap, vdata->method->registersSize);

    return true;
}


/*
 * ===========================================================================
 *      DEX generation & parsing
 * ===========================================================================
 */

/*
 * Advance "ptr" to ensure 32-bit alignment.
 */
static inline u1* align32(u1* ptr)
{
    return (u1*) (((int) ptr + 3) & ~0x03);
}

/*
 * Compute the size, in bytes, of a register map.
 */
static size_t computeRegisterMapSize(const RegisterMap* pMap)
{
    static const int kHeaderSize = offsetof(RegisterMap, data);
    u1 format = dvmRegisterMapGetFormat(pMap);
    u2 numEntries = dvmRegisterMapGetNumEntries(pMap);

    assert(pMap != NULL);

    switch (format) {
    case kRegMapFormatNone:
        return 1;
    case kRegMapFormatCompact8:
        return kHeaderSize + (1 + pMap->regWidth) * numEntries;
    case kRegMapFormatCompact16:
        return kHeaderSize + (2 + pMap->regWidth) * numEntries;
    case kRegMapFormatDifferential:
        {
            /* kHeaderSize + decoded ULEB128 length */
            const u1* ptr = pMap->data;
            int len = readUnsignedLeb128(&ptr);
            return len + (ptr - (u1*) pMap);
        }
    default:
        ALOGE("Bad register map format %d", format);
        dvmAbort();
        return 0;
    }
}

/*
 * Output the map for a single method, if it has one.
 *
 * Abstract and native methods have no map.  All others are expected to
 * have one, since we know the class verified successfully.
 *
 * This strips the "allocated on heap" flag from the format byte, so that
 * direct-mapped maps are correctly identified as such.
 */
static bool writeMapForMethod(const Method* meth, u1** pPtr)
{
    if (meth->registerMap == NULL) {
        if (!dvmIsAbstractMethod(meth) && !dvmIsNativeMethod(meth)) {
            ALOGW("Warning: no map available for %s.%s",
                meth->clazz->descriptor, meth->name);
            /* weird, but keep going */
        }
        *(*pPtr)++ = kRegMapFormatNone;
        return true;
    }

    /* serialize map into the buffer */
    size_t mapSize = computeRegisterMapSize(meth->registerMap);
    memcpy(*pPtr, meth->registerMap, mapSize);

    /* strip the "on heap" flag out of the format byte, which is always first */
    assert(**pPtr == meth->registerMap->format);
    **pPtr &= ~(kRegMapFormatOnHeap);

    *pPtr += mapSize;

    return true;
}

/*
 * Write maps for all methods in the specified class to the buffer, which
 * can hold at most "length" bytes.  "*pPtr" will be advanced past the end
 * of the data we write.
 */
static bool writeMapsAllMethods(DvmDex* pDvmDex, const ClassObject* clazz,
    u1** pPtr, size_t length)
{
    RegisterMapMethodPool* pMethodPool;
    u1* ptr = *pPtr;
    int i, methodCount;

    /* artificial limit */
    if (clazz->virtualMethodCount + clazz->directMethodCount >= 65536) {
        ALOGE("Too many methods in %s", clazz->descriptor);
        return false;
    }

    pMethodPool = (RegisterMapMethodPool*) ptr;
    ptr += offsetof(RegisterMapMethodPool, methodData);
    methodCount = 0;

    /*
     * Run through all methods, direct then virtual.  The class loader will
     * traverse them in the same order.  (We could split them into two
     * distinct pieces, but there doesn't appear to be any value in doing
     * so other than that it makes class loading slightly less fragile.)
     *
     * The class loader won't know about miranda methods at the point
     * where it parses this, so we omit those.
     *
     * TODO: consider omitting all native/abstract definitions.  Should be
     * safe, though we lose the ability to sanity-check against the
     * method counts in the DEX file.
     */
    for (i = 0; i < clazz->directMethodCount; i++) {
        const Method* meth = &clazz->directMethods[i];
        if (dvmIsMirandaMethod(meth))
            continue;
        if (!writeMapForMethod(&clazz->directMethods[i], &ptr)) {
            return false;
        }
        methodCount++;
        //ptr = align32(ptr);
    }

    for (i = 0; i < clazz->virtualMethodCount; i++) {
        const Method* meth = &clazz->virtualMethods[i];
        if (dvmIsMirandaMethod(meth))
            continue;
        if (!writeMapForMethod(&clazz->virtualMethods[i], &ptr)) {
            return false;
        }
        methodCount++;
        //ptr = align32(ptr);
    }

    pMethodPool->methodCount = methodCount;

    *pPtr = ptr;
    return true;
}

/*
 * Write maps for all classes to the specified buffer, which can hold at
 * most "length" bytes.
 *
 * Returns the actual length used, or 0 on failure.
 */
static size_t writeMapsAllClasses(DvmDex* pDvmDex, u1* basePtr, size_t length)
{
    DexFile* pDexFile = pDvmDex->pDexFile;
    u4 count = pDexFile->pHeader->classDefsSize;
    RegisterMapClassPool* pClassPool;
    u4* offsetTable;
    u1* ptr = basePtr;
    u4 idx;

    assert(gDvm.optimizing);

    pClassPool = (RegisterMapClassPool*) ptr;
    ptr += offsetof(RegisterMapClassPool, classDataOffset);
    offsetTable = (u4*) ptr;
    ptr += count * sizeof(u4);

    pClassPool->numClasses = count;

    /*
     * We want an entry for every class, loaded or not.
     */
    for (idx = 0; idx < count; idx++) {
        const DexClassDef* pClassDef;
        const char* classDescriptor;
        ClassObject* clazz;

        pClassDef = dexGetClassDef(pDexFile, idx);
        classDescriptor = dexStringByTypeIdx(pDexFile, pClassDef->classIdx);

        /*
         * All classes have been loaded into the bootstrap class loader.
         * If we can find it, and it was successfully pre-verified, we
         * run through its methods and add the register maps.
         *
         * If it wasn't pre-verified then we know it can't have any
         * register maps.  Classes that can't be loaded or failed
         * verification get an empty slot in the index.
         */
        clazz = NULL;
        if ((pClassDef->accessFlags & CLASS_ISPREVERIFIED) != 0)
            clazz = dvmLookupClass(classDescriptor, NULL, false);

        if (clazz != NULL) {
            offsetTable[idx] = ptr - basePtr;
            LOGVV("%d -> offset %d (%p-%p)",
                idx, offsetTable[idx], ptr, basePtr);

            if (!writeMapsAllMethods(pDvmDex, clazz, &ptr,
                    length - (ptr - basePtr)))
            {
                return 0;
            }

            ptr = align32(ptr);
            LOGVV("Size %s (%d+%d methods): %d", clazz->descriptor,
                clazz->directMethodCount, clazz->virtualMethodCount,
                (ptr - basePtr) - offsetTable[idx]);
        } else {
            ALOGV("%4d NOT mapadding '%s'", idx, classDescriptor);
            assert(offsetTable[idx] == 0);
        }
    }

    if (ptr - basePtr >= (int)length) {
        /* a bit late */
        ALOGE("Buffer overrun");
        dvmAbort();
    }

    return ptr - basePtr;
}

/*
 * Generate a register map set for all verified classes in "pDvmDex".
 */
RegisterMapBuilder* dvmGenerateRegisterMaps(DvmDex* pDvmDex)
{
    RegisterMapBuilder* pBuilder;

    pBuilder = (RegisterMapBuilder*) calloc(1, sizeof(RegisterMapBuilder));
    if (pBuilder == NULL)
        return NULL;

    /*
     * We have a couple of options here:
     *  (1) Compute the size of the output, and malloc a buffer.
     *  (2) Create a "large-enough" anonymous mmap region.
     *
     * The nice thing about option #2 is that we don't have to traverse
     * all of the classes and methods twice.  The risk is that we might
     * not make the region large enough.  Since the pages aren't mapped
     * until used we can allocate a semi-absurd amount of memory without
     * worrying about the effect on the rest of the system.
     *
     * The basic encoding on the largest jar file requires about 1MB of
     * storage.  We map out 4MB here.  (TODO: guarantee that the last
     * page of the mapping is marked invalid, so we reliably fail if
     * we overrun.)
     */
    if (sysCreatePrivateMap(4 * 1024 * 1024, &pBuilder->memMap) != 0) {
        free(pBuilder);
        return NULL;
    }

    /*
     * Create the maps.
     */
    size_t actual = writeMapsAllClasses(pDvmDex, (u1*)pBuilder->memMap.addr,
                                        pBuilder->memMap.length);
    if (actual == 0) {
        dvmFreeRegisterMapBuilder(pBuilder);
        return NULL;
    }

    ALOGV("TOTAL size of register maps: %d", actual);

    pBuilder->data = pBuilder->memMap.addr;
    pBuilder->size = actual;
    return pBuilder;
}

/*
 * Free the builder.
 */
void dvmFreeRegisterMapBuilder(RegisterMapBuilder* pBuilder)
{
    if (pBuilder == NULL)
        return;

    sysReleaseShmem(&pBuilder->memMap);
    free(pBuilder);
}


/*
 * Find the data for the specified class.
 *
 * If there's no register map data, or none for this class, we return NULL.
 */
const void* dvmRegisterMapGetClassData(const DexFile* pDexFile, u4 classIdx,
    u4* pNumMaps)
{
    const RegisterMapClassPool* pClassPool;
    const RegisterMapMethodPool* pMethodPool;

    pClassPool = (const RegisterMapClassPool*) pDexFile->pRegisterMapPool;
    if (pClassPool == NULL)
        return NULL;

    if (classIdx >= pClassPool->numClasses) {
        ALOGE("bad class index (%d vs %d)", classIdx, pClassPool->numClasses);
        dvmAbort();
    }

    u4 classOffset = pClassPool->classDataOffset[classIdx];
    if (classOffset == 0) {
        ALOGV("+++ no map for classIdx=%d", classIdx);
        return NULL;
    }

    pMethodPool =
        (const RegisterMapMethodPool*) (((u1*) pClassPool) + classOffset);
    if (pNumMaps != NULL)
        *pNumMaps = pMethodPool->methodCount;
    return pMethodPool->methodData;
}

/*
 * This advances "*pPtr" and returns its original value.
 */
const RegisterMap* dvmRegisterMapGetNext(const void** pPtr)
{
    const RegisterMap* pMap = (const RegisterMap*) *pPtr;

    *pPtr = /*align32*/(((u1*) pMap) + computeRegisterMapSize(pMap));
    LOGVV("getNext: %p -> %p (f=%#x w=%d e=%d)",
        pMap, *pPtr, pMap->format, pMap->regWidth,
        dvmRegisterMapGetNumEntries(pMap));
    return pMap;
}


/*
 * ===========================================================================
 *      Utility functions
 * ===========================================================================
 */

/*
 * Return the data for the specified address, or NULL if not found.
 *
 * The result must be released with dvmReleaseRegisterMapLine().
 */
const u1* dvmRegisterMapGetLine(const RegisterMap* pMap, int addr)
{
    int addrWidth, lineWidth;
    u1 format = dvmRegisterMapGetFormat(pMap);
    u2 numEntries = dvmRegisterMapGetNumEntries(pMap);

    assert(numEntries > 0);

    switch (format) {
    case kRegMapFormatNone:
        return NULL;
    case kRegMapFormatCompact8:
        addrWidth = 1;
        break;
    case kRegMapFormatCompact16:
        addrWidth = 2;
        break;
    default:
        ALOGE("Unknown format %d", format);
        dvmAbort();
        return NULL;
    }

    lineWidth = addrWidth + pMap->regWidth;

    /*
     * Find the appropriate entry.  Many maps are very small, some are very
     * large.
     */
    static const int kSearchThreshold = 8;
    const u1* data = NULL;
    int lineAddr;

    if (numEntries < kSearchThreshold) {
        int i;
        data = pMap->data;
        for (i = numEntries; i > 0; i--) {
            lineAddr = data[0];
            if (addrWidth > 1)
                lineAddr |= data[1] << 8;
            if (lineAddr == addr)
                return data + addrWidth;

            data += lineWidth;
        }
        assert(data == pMap->data + lineWidth * numEntries);
    } else {
        int hi, lo, mid;

        lo = 0;
        hi = numEntries -1;

        while (hi >= lo) {
            mid = (hi + lo) / 2;
            data = pMap->data + lineWidth * mid;

            lineAddr = data[0];
            if (addrWidth > 1)
                lineAddr |= data[1] << 8;

            if (addr > lineAddr) {
                lo = mid + 1;
            } else if (addr < lineAddr) {
                hi = mid - 1;
            } else {
                return data + addrWidth;
            }
        }
    }

    return NULL;
}

/*
 * Compare two register maps.
 *
 * Returns 0 if they're equal, nonzero if not.
 */
static int compareMaps(const RegisterMap* pMap1, const RegisterMap* pMap2)
{
    size_t size1, size2;

    size1 = computeRegisterMapSize(pMap1);
    size2 = computeRegisterMapSize(pMap2);
    if (size1 != size2) {
        ALOGI("compareMaps: size mismatch (%zd vs %zd)", size1, size2);
        return -1;
    }

    if (memcmp(pMap1, pMap2, size1) != 0) {
        ALOGI("compareMaps: content mismatch");
        return -1;
    }

    return 0;
}


/*
 * Get the expanded form of the register map associated with the method.
 *
 * If the map is already in one of the uncompressed formats, we return
 * immediately.  Otherwise, we expand the map and replace method's register
 * map pointer, freeing it if it was allocated on the heap.
 *
 * NOTE: this function is not synchronized; external locking is mandatory
 * (unless we're in the zygote, where single-threaded access is guaranteed).
 */
const RegisterMap* dvmGetExpandedRegisterMap0(Method* method)
{
    const RegisterMap* curMap = method->registerMap;
    RegisterMap* newMap;

    if (curMap == NULL)
        return NULL;

    /* sanity check to ensure this isn't called w/o external locking */
    /* (if we use this at a time other than during GC, fix/remove this test) */
    if (true) {
        if (!gDvm.zygote && dvmTryLockMutex(&gDvm.gcHeapLock) == 0) {
            ALOGE("GLITCH: dvmGetExpandedRegisterMap not called at GC time");
            dvmAbort();
        }
    }

    RegisterMapFormat format = dvmRegisterMapGetFormat(curMap);
    switch (format) {
    case kRegMapFormatCompact8:
    case kRegMapFormatCompact16:
        if (REGISTER_MAP_VERBOSE) {
            if (dvmRegisterMapGetOnHeap(curMap)) {
                ALOGD("RegMap: already expanded: %s.%s",
                    method->clazz->descriptor, method->name);
            } else {
                ALOGD("RegMap: stored w/o compression: %s.%s",
                    method->clazz->descriptor, method->name);
            }
        }
        return curMap;
    case kRegMapFormatDifferential:
        newMap = uncompressMapDifferential(curMap);
        break;
    default:
        ALOGE("Unknown format %d in dvmGetExpandedRegisterMap", format);
        dvmAbort();
        newMap = NULL;      // make gcc happy
    }

    if (newMap == NULL) {
        ALOGE("Map failed to uncompress (fmt=%d) %s.%s",
            format, method->clazz->descriptor, method->name);
        return NULL;
    }

#ifdef REGISTER_MAP_STATS
    /*
     * Gather and display some stats.
     */
    {
        MapStats* pStats = (MapStats*) gDvm.registerMapStats;
        pStats->numExpandedMaps++;
        pStats->totalExpandedMapSize += computeRegisterMapSize(newMap);
        ALOGD("RMAP: count=%d size=%d",
            pStats->numExpandedMaps, pStats->totalExpandedMapSize);
    }
#endif

    IF_ALOGV() {
        char* desc = dexProtoCopyMethodDescriptor(&method->prototype);
        ALOGV("Expanding map -> %s.%s:%s",
            method->clazz->descriptor, method->name, desc);
        free(desc);
    }

    /*
     * Update method, and free compressed map if it was sitting on the heap.
     */
    dvmSetRegisterMap(method, newMap);

    if (dvmRegisterMapGetOnHeap(curMap))
        dvmFreeRegisterMap((RegisterMap*) curMap);

    return newMap;
}


/*
 * ===========================================================================
 *      Map compression
 * ===========================================================================
 */

/*
Notes on map compression

The idea is to create a compressed form that will be uncompressed before
use, with the output possibly saved in a cache.  This means we can use an
approach that is unsuited for random access if we choose.

In the event that a map simply does not work with our compression scheme,
it's reasonable to store the map without compression.  In the future we
may want to have more than one compression scheme, and try each in turn,
retaining the best.  (We certainly want to keep the uncompressed form if it
turns out to be smaller or even slightly larger than the compressed form.)

Each entry consists of an address and a bit vector.  Adjacent entries are
strongly correlated, suggesting differential encoding.


Ideally we would avoid outputting adjacent entries with identical
bit vectors.  However, the register values at a given address do not
imply anything about the set of valid registers at subsequent addresses.
We therefore cannot omit an entry.

  If the thread stack has a PC at an address without a corresponding
  entry in the register map, we must conservatively scan the registers in
  that thread.  This can happen when single-stepping in the debugger,
  because the debugger is allowed to invoke arbitrary methods when
  a thread is stopped at a breakpoint.  If we can guarantee that a GC
  thread scan will never happen while the debugger has that thread stopped,
  then we can lift this restriction and simply omit entries that don't
  change the bit vector from its previous state.

Each entry advances the address value by at least 1 (measured in 16-bit
"code units").  Looking at the bootclasspath entries, advancing by 2 units
is most common.  Advances by 1 unit are far less common than advances by
2 units, but more common than 5, and things fall off rapidly.  Gaps of
up to 220 code units appear in some computationally intensive bits of code,
but are exceedingly rare.

If we sum up the number of transitions in a couple of ranges in framework.jar:
  [1,4]: 188998 of 218922 gaps (86.3%)
  [1,7]: 211647 of 218922 gaps (96.7%)
Using a 3-bit delta, with one value reserved as an escape code, should
yield good results for the address.

These results would change dramatically if we reduced the set of GC
points by e.g. removing instructions like integer divide that are only
present because they can throw and cause an allocation.

We also need to include an "initial gap", because the first few instructions
in a method may not be GC points.


By observation, many entries simply repeat the previous bit vector, or
change only one or two bits.  (This is with type-precise information;
the rate of change of bits will be different if live-precise information
is factored in).

Looking again at adjacent entries in framework.jar:
  0 bits changed: 63.0%
  1 bit changed: 32.2%
After that it falls off rapidly, e.g. the number of entries with 2 bits
changed is usually less than 1/10th of the number of entries with 1 bit
changed.  A solution that allows us to encode 0- or 1- bit changes
efficiently will do well.

We still need to handle cases where a large number of bits change.  We
probably want a way to drop in a full copy of the bit vector when it's
smaller than the representation of multiple bit changes.


The bit-change information can be encoded as an index that tells the
decoder to toggle the state.  We want to encode the index in as few bits
as possible, but we need to allow for fairly wide vectors (e.g. we have a
method with 175 registers).  We can deal with this in a couple of ways:
(1) use an encoding that assumes few registers and has an escape code
for larger numbers of registers; or (2) use different encodings based
on how many total registers the method has.  The choice depends to some
extent on whether methods with large numbers of registers tend to modify
the first 16 regs more often than the others.

The last N registers hold method arguments.  If the bytecode is expected
to be examined in a debugger, "dx" ensures that the contents of these
registers won't change.  Depending upon the encoding format, we may be
able to take advantage of this.  We still have to encode the initial
state, but we know we'll never have to output a bit change for the last
N registers.

Considering only methods with 16 or more registers, the "target octant"
for register changes looks like this:
  [ 43.1%, 16.4%, 6.5%, 6.2%, 7.4%, 8.8%, 9.7%, 1.8% ]
As expected, there are fewer changes at the end of the list where the
arguments are kept, and more changes at the start of the list because
register values smaller than 16 can be used in compact Dalvik instructions
and hence are favored for frequently-used values.  In general, the first
octant is considerably more active than later entries, the last octant
is much less active, and the rest are all about the same.

Looking at all bit changes in all methods, 94% are to registers 0-15.  The
encoding will benefit greatly by favoring the low-numbered registers.


Some of the smaller methods have identical maps, and space could be
saved by simply including a pointer to an earlier definition.  This would
be best accomplished by specifying a "pointer" format value, followed by
a 3-byte (or ULEB128) offset.  Implementing this would probably involve
generating a hash value for each register map and maintaining a hash table.

In some cases there are repeating patterns in the bit vector that aren't
adjacent.  These could benefit from a dictionary encoding.  This doesn't
really become useful until the methods reach a certain size though,
and managing the dictionary may incur more overhead than we want.

Large maps can be compressed significantly.  The trouble is that, when
we need to use them, we have to uncompress them onto the heap.  We may
get a better trade-off between storage size and heap usage by refusing to
compress large maps, so that they can be memory mapped and used directly.
(OTOH, only about 2% of the maps will ever actually be used.)


----- differential format -----

// common header
+00 1B format
+01 1B regWidth
+02 2B numEntries (little-endian)
+04 nB length in bytes of the data that follows, in ULEB128 format
       (not strictly necessary; allows determination of size w/o full parse)
+05+ 1B initial address (0-127), high bit set if max addr >= 256
+06+ nB initial value for bit vector

// for each entry
+00: CCCCBAAA

  AAA: address difference.  Values from 0 to 6 indicate an increment of 1
  to 7.  A value of 7 indicates that the address difference is large,
  and the next byte is a ULEB128-encoded difference value.

  B: determines the meaning of CCCC.

  CCCC: if B is 0, this is the number of the bit to toggle (0-15).
  If B is 1, this is a count of the number of changed bits (1-14).  A value
  of 0 means that no bits were changed, and a value of 15 indicates
  that enough bits were changed that it required less space to output
  the entire bit vector.

+01: (optional) ULEB128-encoded address difference

+01+: (optional) one or more ULEB128-encoded bit numbers, OR the entire
  bit vector.

The most common situation is an entry whose address has changed by 2-4
code units, has no changes or just a single bit change, and the changed
register is less than 16.  We should therefore be able to encode a large
number of entries with a single byte, which is half the size of the
Compact8 encoding method.
*/

/*
 * Compute some stats on an uncompressed register map.
 */
#ifdef REGISTER_MAP_STATS
static void computeMapStats(RegisterMap* pMap, const Method* method)
{
    MapStats* pStats = (MapStats*) gDvm.registerMapStats;
    const u1 format = dvmRegisterMapGetFormat(pMap);
    const u2 numEntries = dvmRegisterMapGetNumEntries(pMap);
    const u1* rawMap = pMap->data;
    const u1* prevData = NULL;
    int ent, addr, prevAddr = -1;

    for (ent = 0; ent < numEntries; ent++) {
        switch (format) {
        case kRegMapFormatCompact8:
            addr = *rawMap++;
            break;
        case kRegMapFormatCompact16:
            addr = *rawMap++;
            addr |= (*rawMap++) << 8;
            break;
        default:
            /* shouldn't happen */
            ALOGE("GLITCH: bad format (%d)", format);
            dvmAbort();
        }

        const u1* dataStart = rawMap;

        pStats->totalGcPointCount++;

        /*
         * Gather "gap size" stats, i.e. the difference in addresses between
         * successive GC points.
         */
        if (prevData != NULL) {
            assert(prevAddr >= 0);
            int addrDiff = addr - prevAddr;

            if (addrDiff < 0) {
                ALOGE("GLITCH: address went backward (0x%04x->0x%04x, %s.%s)",
                    prevAddr, addr, method->clazz->descriptor, method->name);
            } else if (addrDiff > kMaxGcPointGap) {
                if (REGISTER_MAP_VERBOSE) {
                    ALOGI("HEY: addrDiff is %d, max %d (0x%04x->0x%04x %s.%s)",
                        addrDiff, kMaxGcPointGap, prevAddr, addr,
                        method->clazz->descriptor, method->name);
                }
                /* skip this one */
            } else {
                pStats->gcPointGap[addrDiff]++;
            }
            pStats->gcGapCount++;


            /*
             * Compare bit vectors in adjacent entries.  We want to count
             * up the number of bits that differ (to see if we frequently
             * change 0 or 1 bits) and get a sense for which part of the
             * vector changes the most often (near the start, middle, end).
             *
             * We only do the vector position quantization if we have at
             * least 16 registers in the method.
             */
            int numDiff = 0;
            float div = (float) kNumUpdatePosns / method->registersSize;
            int regByte;
            for (regByte = 0; regByte < pMap->regWidth; regByte++) {
                int prev, cur, bit;

                prev = prevData[regByte];
                cur = dataStart[regByte];

                for (bit = 0; bit < 8; bit++) {
                    if (((prev >> bit) & 1) != ((cur >> bit) & 1)) {
                        numDiff++;

                        int bitNum = regByte * 8 + bit;

                        if (bitNum < 16)
                            pStats->updateLT16++;
                        else
                            pStats->updateGE16++;

                        if (method->registersSize < 16)
                            continue;

                        if (bitNum >= method->registersSize) {
                            /* stuff off the end should be zero in both */
                            ALOGE("WEIRD: bit=%d (%d/%d), prev=%02x cur=%02x",
                                bit, regByte, method->registersSize,
                                prev, cur);
                            assert(false);
                        }
                        int idx = (int) (bitNum * div);
                        if (!(idx >= 0 && idx < kNumUpdatePosns)) {
                            ALOGE("FAIL: bitNum=%d (of %d) div=%.3f idx=%d",
                                bitNum, method->registersSize, div, idx);
                            assert(false);
                        }
                        pStats->updatePosn[idx]++;
                    }
                }
            }

            if (numDiff > kMaxDiffBits) {
                if (REGISTER_MAP_VERBOSE) {
                    ALOGI("WOW: numDiff is %d, max %d", numDiff, kMaxDiffBits);
                }
            } else {
                pStats->numDiffBits[numDiff]++;
            }
        }

        /* advance to start of next line */
        rawMap += pMap->regWidth;

        prevAddr = addr;
        prevData = dataStart;
    }
}
#endif

/*
 * Compute the difference between two bit vectors.
 *
 * If "lebOutBuf" is non-NULL, we output the bit indices in ULEB128 format
 * as we go.  Otherwise, we just generate the various counts.
 *
 * The bit vectors are compared byte-by-byte, so any unused bits at the
 * end must be zero.
 *
 * Returns the number of bytes required to hold the ULEB128 output.
 *
 * If "pFirstBitChanged" or "pNumBitsChanged" are non-NULL, they will
 * receive the index of the first changed bit and the number of changed
 * bits, respectively.
 */
static int computeBitDiff(const u1* bits1, const u1* bits2, int byteWidth,
    int* pFirstBitChanged, int* pNumBitsChanged, u1* lebOutBuf)
{
    int numBitsChanged = 0;
    int firstBitChanged = -1;
    int lebSize = 0;
    int byteNum;

    /*
     * Run through the vectors, first comparing them at the byte level.  This
     * will yield a fairly quick result if nothing has changed between them.
     */
    for (byteNum = 0; byteNum < byteWidth; byteNum++) {
        u1 byte1 = *bits1++;
        u1 byte2 = *bits2++;
        if (byte1 != byte2) {
            /*
             * Walk through the byte, identifying the changed bits.
             */
            int bitNum;
            for (bitNum = 0; bitNum < 8; bitNum++) {
                if (((byte1 >> bitNum) & 0x01) != ((byte2 >> bitNum) & 0x01)) {
                    int bitOffset = (byteNum << 3) + bitNum;

                    if (firstBitChanged < 0)
                        firstBitChanged = bitOffset;
                    numBitsChanged++;

                    if (lebOutBuf == NULL) {
                        lebSize += unsignedLeb128Size(bitOffset);
                    } else {
                        u1* curBuf = lebOutBuf;
                        lebOutBuf = writeUnsignedLeb128(lebOutBuf, bitOffset);
                        lebSize += lebOutBuf - curBuf;
                    }
                }
            }
        }
    }

    if (numBitsChanged > 0)
        assert(firstBitChanged >= 0);

    if (pFirstBitChanged != NULL)
        *pFirstBitChanged = firstBitChanged;
    if (pNumBitsChanged != NULL)
        *pNumBitsChanged = numBitsChanged;

    return lebSize;
}

/*
 * Compress the register map with differential encoding.
 *
 * "meth" is only needed for debug output.
 *
 * On success, returns a newly-allocated RegisterMap.  If the map is not
 * compatible for some reason, or fails to get smaller, this will return NULL.
 */
static RegisterMap* compressMapDifferential(const RegisterMap* pMap,
    const Method* meth)
{
    RegisterMap* pNewMap = NULL;
    int origSize = computeRegisterMapSize(pMap);
    u1* tmpPtr;
    int addrWidth, regWidth, numEntries;
    bool debug = false;

    if (false &&
        strcmp(meth->clazz->descriptor, "Landroid/text/StaticLayout;") == 0 &&
        strcmp(meth->name, "generate") == 0)
    {
        debug = true;
    }

    u1 format = dvmRegisterMapGetFormat(pMap);
    switch (format) {
    case kRegMapFormatCompact8:
        addrWidth = 1;
        break;
    case kRegMapFormatCompact16:
        addrWidth = 2;
        break;
    default:
        ALOGE("ERROR: can't compress map with format=%d", format);
        return NULL;
    }

    regWidth = dvmRegisterMapGetRegWidth(pMap);
    numEntries = dvmRegisterMapGetNumEntries(pMap);

    if (debug) {
        ALOGI("COMPRESS: %s.%s aw=%d rw=%d ne=%d",
            meth->clazz->descriptor, meth->name,
            addrWidth, regWidth, numEntries);
        dumpRegisterMap(pMap, -1);
    }

    if (numEntries <= 1) {
        ALOGV("Can't compress map with 0 or 1 entries");
        return NULL;
    }

    /*
     * We don't know how large the compressed data will be.  It's possible
     * for it to expand and become larger than the original.  The header
     * itself is variable-sized, so we generate everything into a temporary
     * buffer and then copy it to form-fitting storage once we know how big
     * it will be (and that it's smaller than the original).
     *
     * If we use a size that is equal to the size of the input map plus
     * a value longer than a single entry can possibly expand to, we need
     * only check for overflow at the end of each entry.  The worst case
     * for a single line is (1 + <ULEB8 address> + <full copy of vector>).
     * Addresses are 16 bits, so that's (1 + 3 + regWidth).
     *
     * The initial address offset and bit vector will take up less than
     * or equal to the amount of space required when uncompressed -- large
     * initial offsets are rejected.
     */
    UniquePtr<u1[]> tmpBuf(new u1[origSize + (1 + 3 + regWidth)]);
    if (tmpBuf.get() == NULL)
        return NULL;

    tmpPtr = tmpBuf.get();

    const u1* mapData = pMap->data;
    const u1* prevBits;
    u2 addr, prevAddr;

    addr = *mapData++;
    if (addrWidth > 1)
        addr |= (*mapData++) << 8;

    if (addr >= 128) {
        ALOGV("Can't compress map with starting address >= 128");
        return NULL;
    }

    /*
     * Start by writing the initial address and bit vector data.  The high
     * bit of the initial address is used to indicate the required address
     * width (which the decoder can't otherwise determine without parsing
     * the compressed data).
     */
    *tmpPtr++ = addr | (addrWidth > 1 ? 0x80 : 0x00);
    memcpy(tmpPtr, mapData, regWidth);

    prevBits = mapData;
    prevAddr = addr;

    tmpPtr += regWidth;
    mapData += regWidth;

    /*
     * Loop over all following entries.
     */
    for (int entry = 1; entry < numEntries; entry++) {
        int addrDiff;
        u1 key;

        /*
         * Pull out the address and figure out how to encode it.
         */
        addr = *mapData++;
        if (addrWidth > 1)
            addr |= (*mapData++) << 8;

        if (debug)
            ALOGI(" addr=0x%04x ent=%d (aw=%d)", addr, entry, addrWidth);

        addrDiff = addr - prevAddr;
        assert(addrDiff > 0);
        if (addrDiff < 8) {
            /* small difference, encode in 3 bits */
            key = addrDiff -1;          /* set 00000AAA */
            if (debug)
                ALOGI(" : small %d, key=0x%02x", addrDiff, key);
        } else {
            /* large difference, output escape code */
            key = 0x07;                 /* escape code for AAA */
            if (debug)
                ALOGI(" : large %d, key=0x%02x", addrDiff, key);
        }

        int numBitsChanged, firstBitChanged, lebSize;

        lebSize = computeBitDiff(prevBits, mapData, regWidth,
            &firstBitChanged, &numBitsChanged, NULL);

        if (debug) {
            ALOGI(" : diff fbc=%d nbc=%d ls=%d (rw=%d)",
                firstBitChanged, numBitsChanged, lebSize, regWidth);
        }

        if (numBitsChanged == 0) {
            /* set B to 1 and CCCC to zero to indicate no bits were changed */
            key |= 0x08;
            if (debug) ALOGI(" : no bits changed");
        } else if (numBitsChanged == 1 && firstBitChanged < 16) {
            /* set B to 0 and CCCC to the index of the changed bit */
            key |= firstBitChanged << 4;
            if (debug) ALOGI(" : 1 low bit changed");
        } else if (numBitsChanged < 15 && lebSize < regWidth) {
            /* set B to 1 and CCCC to the number of bits */
            key |= 0x08 | (numBitsChanged << 4);
            if (debug) ALOGI(" : some bits changed");
        } else {
            /* set B to 1 and CCCC to 0x0f so we store the entire vector */
            key |= 0x08 | 0xf0;
            if (debug) ALOGI(" : encode original");
        }

        /*
         * Encode output.  Start with the key, follow with the address
         * diff (if it didn't fit in 3 bits), then the changed bit info.
         */
        *tmpPtr++ = key;
        if ((key & 0x07) == 0x07)
            tmpPtr = writeUnsignedLeb128(tmpPtr, addrDiff);

        if ((key & 0x08) != 0) {
            int bitCount = key >> 4;
            if (bitCount == 0) {
                /* nothing changed, no additional output required */
            } else if (bitCount == 15) {
                /* full vector is most compact representation */
                memcpy(tmpPtr, mapData, regWidth);
                tmpPtr += regWidth;
            } else {
                /* write bit indices in LEB128 format */
                (void) computeBitDiff(prevBits, mapData, regWidth,
                    NULL, NULL, tmpPtr);
                tmpPtr += lebSize;
            }
        } else {
            /* single-bit changed, value encoded in key byte */
        }

        prevBits = mapData;
        prevAddr = addr;
        mapData += regWidth;

        /*
         * See if we've run past the original size.
         */
        if (tmpPtr - tmpBuf.get() >= origSize) {
            if (debug) {
                ALOGD("Compressed size >= original (%d vs %d): %s.%s",
                    tmpPtr - tmpBuf.get(), origSize,
                    meth->clazz->descriptor, meth->name);
            }
            return NULL;
        }
    }

    /*
     * Create a RegisterMap with the contents.
     *
     * TODO: consider using a threshold other than merely ">=".  We would
     * get poorer compression but potentially use less native heap space.
     */
    static const int kHeaderSize = offsetof(RegisterMap, data);
    int newDataSize = tmpPtr - tmpBuf.get();
    int newMapSize;

    newMapSize = kHeaderSize + unsignedLeb128Size(newDataSize) + newDataSize;
    if (newMapSize >= origSize) {
        if (debug) {
            ALOGD("Final comp size >= original (%d vs %d): %s.%s",
                newMapSize, origSize, meth->clazz->descriptor, meth->name);
        }
        return NULL;
    }

    pNewMap = (RegisterMap*) malloc(newMapSize);
    if (pNewMap == NULL)
        return NULL;
    dvmRegisterMapSetFormat(pNewMap, kRegMapFormatDifferential);
    dvmRegisterMapSetOnHeap(pNewMap, true);
    dvmRegisterMapSetRegWidth(pNewMap, regWidth);
    dvmRegisterMapSetNumEntries(pNewMap, numEntries);

    tmpPtr = pNewMap->data;
    tmpPtr = writeUnsignedLeb128(tmpPtr, newDataSize);
    memcpy(tmpPtr, tmpBuf.get(), newDataSize);

    if (REGISTER_MAP_VERBOSE) {
        ALOGD("Compression successful (%d -> %d) from aw=%d rw=%d ne=%d",
            computeRegisterMapSize(pMap), computeRegisterMapSize(pNewMap),
            addrWidth, regWidth, numEntries);
    }

    return pNewMap;
}

/*
 * Toggle the value of the "idx"th bit in "ptr".
 */
static inline void toggleBit(u1* ptr, int idx)
{
    ptr[idx >> 3] ^= 1 << (idx & 0x07);
}

/*
 * Expand a compressed map to an uncompressed form.
 *
 * Returns a newly-allocated RegisterMap on success, or NULL on failure.
 *
 * TODO: consider using the linear allocator or a custom allocator with
 * LRU replacement for these instead of the native heap.
 */
static RegisterMap* uncompressMapDifferential(const RegisterMap* pMap)
{
    static const int kHeaderSize = offsetof(RegisterMap, data);
    u1 format = dvmRegisterMapGetFormat(pMap);
    RegisterMapFormat newFormat;
    int regWidth, numEntries, newAddrWidth, newMapSize;

    if (format != kRegMapFormatDifferential) {
        ALOGE("Not differential (%d)", format);
        return NULL;
    }

    regWidth = dvmRegisterMapGetRegWidth(pMap);
    numEntries = dvmRegisterMapGetNumEntries(pMap);

    /* get the data size; we can check this at the end */
    const u1* srcPtr = pMap->data;
    int expectedSrcLen = readUnsignedLeb128(&srcPtr);
    const u1* srcStart = srcPtr;

    /* get the initial address and the 16-bit address flag */
    int addr = *srcPtr & 0x7f;
    if ((*srcPtr & 0x80) == 0) {
        newFormat = kRegMapFormatCompact8;
        newAddrWidth = 1;
    } else {
        newFormat = kRegMapFormatCompact16;
        newAddrWidth = 2;
    }
    srcPtr++;

    /* now we know enough to allocate the new map */
    if (REGISTER_MAP_VERBOSE) {
        ALOGI("Expanding to map aw=%d rw=%d ne=%d",
            newAddrWidth, regWidth, numEntries);
    }
    newMapSize = kHeaderSize + (newAddrWidth + regWidth) * numEntries;
    RegisterMap* pNewMap = (RegisterMap*) malloc(newMapSize);

    if (pNewMap == NULL)
      return NULL;

    dvmRegisterMapSetFormat(pNewMap, newFormat);
    dvmRegisterMapSetOnHeap(pNewMap, true);
    dvmRegisterMapSetRegWidth(pNewMap, regWidth);
    dvmRegisterMapSetNumEntries(pNewMap, numEntries);

    /*
     * Write the start address and initial bits to the new map.
     */
    u1* dstPtr = pNewMap->data;

    *dstPtr++ = addr & 0xff;
    if (newAddrWidth > 1)
        *dstPtr++ = (u1) (addr >> 8);

    memcpy(dstPtr, srcPtr, regWidth);

    int prevAddr = addr;
    const u1* prevBits = dstPtr;    /* point at uncompressed data */

    dstPtr += regWidth;
    srcPtr += regWidth;

    /*
     * Walk through, uncompressing one line at a time.
     */
    int entry;
    for (entry = 1; entry < numEntries; entry++) {
        int addrDiff;
        u1 key;

        key = *srcPtr++;

        /* get the address */
        if ((key & 0x07) == 7) {
            /* address diff follows in ULEB128 */
            addrDiff = readUnsignedLeb128(&srcPtr);
        } else {
            addrDiff = (key & 0x07) +1;
        }

        addr = prevAddr + addrDiff;
        *dstPtr++ = addr & 0xff;
        if (newAddrWidth > 1)
            *dstPtr++ = (u1) (addr >> 8);

        /* unpack the bits */
        if ((key & 0x08) != 0) {
            int bitCount = (key >> 4);
            if (bitCount == 0) {
                /* no bits changed, just copy previous */
                memcpy(dstPtr, prevBits, regWidth);
            } else if (bitCount == 15) {
                /* full copy of bit vector is present; ignore prevBits */
                memcpy(dstPtr, srcPtr, regWidth);
                srcPtr += regWidth;
            } else {
                /* copy previous bits and modify listed indices */
                memcpy(dstPtr, prevBits, regWidth);
                while (bitCount--) {
                    int bitIndex = readUnsignedLeb128(&srcPtr);
                    toggleBit(dstPtr, bitIndex);
                }
            }
        } else {
            /* copy previous bits and modify the specified one */
            memcpy(dstPtr, prevBits, regWidth);

            /* one bit, from 0-15 inclusive, was changed */
            toggleBit(dstPtr, key >> 4);
        }

        prevAddr = addr;
        prevBits = dstPtr;
        dstPtr += regWidth;
    }

    if (dstPtr - (u1*) pNewMap != newMapSize) {
        ALOGE("ERROR: output %d bytes, expected %d",
            dstPtr - (u1*) pNewMap, newMapSize);
        free(pNewMap);
        return NULL;
    }

    if (srcPtr - srcStart != expectedSrcLen) {
        ALOGE("ERROR: consumed %d bytes, expected %d",
            srcPtr - srcStart, expectedSrcLen);
        free(pNewMap);
        return NULL;
    }

    if (REGISTER_MAP_VERBOSE) {
        ALOGD("Expansion successful (%d -> %d)",
            computeRegisterMapSize(pMap), computeRegisterMapSize(pNewMap));
    }

    return pNewMap;
}
