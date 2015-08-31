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
 * VM-specific state associated with a DEX file.
 */
#include "Dalvik.h"
#include <sys/mman.h>

/*
 * Create auxillary data structures.
 *
 * We need a 4-byte pointer for every reference to a class, method, field,
 * or string constant.  Summed up over all loaded DEX files (including the
 * whoppers in the boostrap class path), this adds up to be quite a bit
 * of native memory.
 *
 * For more traditional VMs these values could be stuffed into the loaded
 * class file constant pool area, but we don't have that luxury since our
 * classes are memory-mapped read-only.
 *
 * The DEX optimizer will remove the need for some of these (e.g. we won't
 * use the entry for virtual methods that are only called through
 * invoke-virtual-quick), creating the possibility of some space reduction
 * at dexopt time.
 */

static DvmDex* allocateAuxStructures(DexFile* pDexFile)
{
    DvmDex* pDvmDex;
    const DexHeader* pHeader;
    u4 stringSize, classSize, methodSize, fieldSize;

    pHeader = pDexFile->pHeader;

    stringSize = pHeader->stringIdsSize * sizeof(struct StringObject*);
    classSize  = pHeader->typeIdsSize * sizeof(struct ClassObject*);
    methodSize = pHeader->methodIdsSize * sizeof(struct Method*);
    fieldSize  = pHeader->fieldIdsSize * sizeof(struct Field*);

    u4 totalSize = sizeof(DvmDex) +
                   stringSize + classSize + methodSize + fieldSize;

    u1 *blob = (u1 *)dvmAllocRegion(totalSize,
                              PROT_READ | PROT_WRITE, "dalvik-aux-structure");
    if ((void *)blob == MAP_FAILED)
        return NULL;

    pDvmDex = (DvmDex*)blob;
    blob += sizeof(DvmDex);

    pDvmDex->pDexFile = pDexFile;
    pDvmDex->pHeader = pHeader;

    pDvmDex->pResStrings = (struct StringObject**)blob;
    blob += stringSize;
    pDvmDex->pResClasses = (struct ClassObject**)blob;
    blob += classSize;
    pDvmDex->pResMethods = (struct Method**)blob;
    blob += methodSize;
    pDvmDex->pResFields = (struct Field**)blob;

    ALOGV("+++ DEX %p: allocateAux (%d+%d+%d+%d)*4 = %d bytes",
        pDvmDex, stringSize/4, classSize/4, methodSize/4, fieldSize/4,
        stringSize + classSize + methodSize + fieldSize);

    pDvmDex->pInterfaceCache = dvmAllocAtomicCache(DEX_INTERFACE_CACHE_SIZE);

    dvmInitMutex(&pDvmDex->modLock);

    return pDvmDex;
}

/*
 * Given an open optimized DEX file, map it into read-only shared memory and
 * parse the contents.
 *
 * Returns nonzero on error.
 */
int dvmDexFileOpenFromFd(int fd, DvmDex** ppDvmDex)
{
    DvmDex* pDvmDex;
    DexFile* pDexFile;
    MemMapping memMap;
    int parseFlags = kDexParseDefault;
    int result = -1;

    if (gDvm.verifyDexChecksum)
        parseFlags |= kDexParseVerifyChecksum;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        ALOGE("lseek rewind failed");
        goto bail;
    }

    if (sysMapFileInShmemWritableReadOnly(fd, &memMap) != 0) {
        ALOGE("Unable to map file");
        goto bail;
    }

    pDexFile = dexFileParse((u1*)memMap.addr, memMap.length, parseFlags);
    if (pDexFile == NULL) {
        ALOGE("DEX parse failed");
        sysReleaseShmem(&memMap);
        goto bail;
    }

    pDvmDex = allocateAuxStructures(pDexFile);
    if (pDvmDex == NULL) {
        dexFileFree(pDexFile);
        sysReleaseShmem(&memMap);
        goto bail;
    }

    /* tuck this into the DexFile so it gets released later */
    sysCopyMap(&pDvmDex->memMap, &memMap);
    pDvmDex->isMappedReadOnly = true;
    *ppDvmDex = pDvmDex;
    result = 0;

bail:
    return result;
}

/*
 * Create a DexFile structure for a "partial" DEX.  This is one that is in
 * the process of being optimized.  The optimization header isn't finished
 * and we won't have any of the auxillary data tables, so we have to do
 * the initialization slightly differently.
 *
 * Returns nonzero on error.
 */
int dvmDexFileOpenPartial(const void* addr, int len, DvmDex** ppDvmDex)
{
    DvmDex* pDvmDex;
    DexFile* pDexFile;
    int parseFlags = kDexParseDefault;
    int result = -1;

    /* -- file is incomplete, new checksum has not yet been calculated
    if (gDvm.verifyDexChecksum)
        parseFlags |= kDexParseVerifyChecksum;
    */

    pDexFile = dexFileParse((u1*)addr, len, parseFlags);
    if (pDexFile == NULL) {
        ALOGE("DEX parse failed");
        goto bail;
    }
    pDvmDex = allocateAuxStructures(pDexFile);
    if (pDvmDex == NULL) {
        dexFileFree(pDexFile);
        goto bail;
    }

    pDvmDex->isMappedReadOnly = false;
    *ppDvmDex = pDvmDex;
    result = 0;

bail:
    return result;
}

/*
 * Free up the DexFile and any associated data structures.
 *
 * Note we may be called with a partially-initialized DvmDex.
 */
void dvmDexFileFree(DvmDex* pDvmDex)
{
    u4 totalSize;

    if (pDvmDex == NULL)
        return;

    dvmDestroyMutex(&pDvmDex->modLock);

    totalSize  = pDvmDex->pHeader->stringIdsSize * sizeof(struct StringObject*);
    totalSize += pDvmDex->pHeader->typeIdsSize * sizeof(struct ClassObject*);
    totalSize += pDvmDex->pHeader->methodIdsSize * sizeof(struct Method*);
    totalSize += pDvmDex->pHeader->fieldIdsSize * sizeof(struct Field*);
    totalSize += sizeof(DvmDex);

    dexFileFree(pDvmDex->pDexFile);

    ALOGV("+++ DEX %p: freeing aux structs", pDvmDex);
    dvmFreeAtomicCache(pDvmDex->pInterfaceCache);
    sysReleaseShmem(&pDvmDex->memMap);
    munmap(pDvmDex, totalSize);
}


/*
 * Change the byte at the specified address to a new value.  If the location
 * already has the new value, do nothing.
 *
 * This requires changing the access permissions to read-write, updating
 * the value, and then resetting the permissions.
 *
 * We need to ensure mutual exclusion at a page granularity to avoid a race
 * where one threads sets read-write, another thread sets read-only, and
 * then the first thread does a write.  Since we don't do a lot of updates,
 * and the window is small, we just use a lock across the entire DvmDex.
 * We're only trying to make the page state change atomic; it's up to the
 * caller to ensure that multiple threads aren't stomping on the same
 * location (e.g. breakpoints and verifier/optimizer changes happening
 * simultaneously).
 *
 * TODO: if we're back to the original state of the page, use
 * madvise(MADV_DONTNEED) to release the private/dirty copy.
 *
 * Returns "true" on success.
 */
bool dvmDexChangeDex1(DvmDex* pDvmDex, u1* addr, u1 newVal)
{
    if (*addr == newVal) {
        ALOGV("+++ byte at %p is already 0x%02x", addr, newVal);
        return true;
    }

    /*
     * We're not holding this for long, so we don't bother with switching
     * to VMWAIT.
     */
    dvmLockMutex(&pDvmDex->modLock);

    ALOGV("+++ change byte at %p from 0x%02x to 0x%02x", addr, *addr, newVal);
    if (sysChangeMapAccess(addr, 1, true, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RW) failed");
        /* expected on files mounted from FAT; keep going (may crash) */
    }

    *addr = newVal;

    if (sysChangeMapAccess(addr, 1, false, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RO) failed");
        /* expected on files mounted from FAT; keep going */
    }

    dvmUnlockMutex(&pDvmDex->modLock);

    return true;
}

/*
 * Change the 2-byte value at the specified address to a new value.  If the
 * location already has the new value, do nothing.
 *
 * Otherwise works like dvmDexChangeDex1.
 */
bool dvmDexChangeDex2(DvmDex* pDvmDex, u2* addr, u2 newVal)
{
    if (*addr == newVal) {
        ALOGV("+++ value at %p is already 0x%04x", addr, newVal);
        return true;
    }

    /*
     * We're not holding this for long, so we don't bother with switching
     * to VMWAIT.
     */
    dvmLockMutex(&pDvmDex->modLock);

    ALOGV("+++ change 2byte at %p from 0x%04x to 0x%04x", addr, *addr, newVal);
    if (sysChangeMapAccess(addr, 2, true, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RW) failed");
        /* expected on files mounted from FAT; keep going (may crash) */
    }

    *addr = newVal;

    if (sysChangeMapAccess(addr, 2, false, &pDvmDex->memMap) != 0) {
        ALOGD("NOTE: DEX page access change (->RO) failed");
        /* expected on files mounted from FAT; keep going */
    }

    dvmUnlockMutex(&pDvmDex->modLock);

    return true;
}
