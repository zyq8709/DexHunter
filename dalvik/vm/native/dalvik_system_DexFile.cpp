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
 * dalvik.system.DexFile
 */
#include "Dalvik.h"
#include "native/InternalNativePriv.h"

/*
 * Return true if the given name ends with ".dex".
 */
static bool hasDexExtension(const char* name) {
    size_t len = strlen(name);

    return (len >= 5)
        && (name[len - 5] != '/')
        && (strcmp(&name[len - 4], ".dex") == 0);
}

/*
 * Internal struct for managing DexFile.
 */
struct DexOrJar {
    char*       fileName;
    bool        isDex;
    bool        okayToFree;
    RawDexFile* pRawDexFile;
    JarFile*    pJarFile;
    u1*         pDexMemory; // malloc()ed memory, if any
};

/*
 * (This is a dvmHashTableFree callback.)
 */
void dvmFreeDexOrJar(void* vptr)
{
    DexOrJar* pDexOrJar = (DexOrJar*) vptr;

    ALOGV("Freeing DexOrJar '%s'", pDexOrJar->fileName);

    if (pDexOrJar->isDex)
        dvmRawDexFileFree(pDexOrJar->pRawDexFile);
    else
        dvmJarFileFree(pDexOrJar->pJarFile);
    free(pDexOrJar->fileName);
    free(pDexOrJar->pDexMemory);
    free(pDexOrJar);
}

/*
 * (This is a dvmHashTableLookup compare func.)
 *
 * Args are DexOrJar*.
 */
static int hashcmpDexOrJar(const void* tableVal, const void* newVal)
{
    return (int) newVal - (int) tableVal;
}

/*
 * Verify that the "cookie" is a DEX file we opened.
 *
 * Expects that the hash table will be *unlocked* here.
 *
 * If the cookie is invalid, we throw an exception and return "false".
 */
static bool validateCookie(int cookie)
{
    DexOrJar* pDexOrJar = (DexOrJar*) cookie;

    LOGVV("+++ dex verifying cookie %p", pDexOrJar);

    if (pDexOrJar == NULL)
        return false;

    u4 hash = cookie;
    dvmHashTableLock(gDvm.userDexFiles);
    void* result = dvmHashTableLookup(gDvm.userDexFiles, hash, pDexOrJar,
                hashcmpDexOrJar, false);
    dvmHashTableUnlock(gDvm.userDexFiles);
    if (result == NULL) {
        dvmThrowRuntimeException("invalid DexFile cookie");
        return false;
    }

    return true;
}


/*
 * Add given DexOrJar to the hash table of user-loaded dex files.
 */
static void addToDexFileTable(DexOrJar* pDexOrJar) {
    /*
     * Later on, we will receive this pointer as an argument and need
     * to find it in the hash table without knowing if it's valid or
     * not, which means we can't compute a hash value from anything
     * inside DexOrJar. We don't share DexOrJar structs when the same
     * file is opened multiple times, so we can just use the low 32
     * bits of the pointer as the hash.
     */
    u4 hash = (u4) pDexOrJar;
    void* result;

    dvmHashTableLock(gDvm.userDexFiles);
    result = dvmHashTableLookup(gDvm.userDexFiles, hash, pDexOrJar,
            hashcmpDexOrJar, true);
    dvmHashTableUnlock(gDvm.userDexFiles);

    if (result != pDexOrJar) {
        ALOGE("Pointer has already been added?");
        dvmAbort();
    }

    pDexOrJar->okayToFree = true;
}

/*
 * private static int openDexFileNative(String sourceName, String outputName,
 *     int flags) throws IOException
 *
 * Open a DEX file, returning a pointer to our internal data structure.
 *
 * "sourceName" should point to the "source" jar or DEX file.
 *
 * If "outputName" is NULL, the DEX code will automatically find the
 * "optimized" version in the cache directory, creating it if necessary.
 * If it's non-NULL, the specified file will be used instead.
 *
 * TODO: at present we will happily open the same file more than once.
 * To optimize this away we could search for existing entries in the hash
 * table and refCount them.  Requires atomic ops or adding "synchronized"
 * to the non-native code that calls here.
 *
 * TODO: should be using "long" for a pointer.
 */
static void Dalvik_dalvik_system_DexFile_openDexFileNative(const u4* args,
    JValue* pResult)
{
    StringObject* sourceNameObj = (StringObject*) args[0];
    StringObject* outputNameObj = (StringObject*) args[1];
    DexOrJar* pDexOrJar = NULL;
    JarFile* pJarFile;
    RawDexFile* pRawDexFile;
    char* sourceName;
    char* outputName;

    if (sourceNameObj == NULL) {
        dvmThrowNullPointerException("sourceName == null");
        RETURN_VOID();
    }

    sourceName = dvmCreateCstrFromString(sourceNameObj);
    if (outputNameObj != NULL)
        outputName = dvmCreateCstrFromString(outputNameObj);
    else
        outputName = NULL;

    /*
     * We have to deal with the possibility that somebody might try to
     * open one of our bootstrap class DEX files.  The set of dependencies
     * will be different, and hence the results of optimization might be
     * different, which means we'd actually need to have two versions of
     * the optimized DEX: one that only knows about part of the boot class
     * path, and one that knows about everything in it.  The latter might
     * optimize field/method accesses based on a class that appeared later
     * in the class path.
     *
     * We can't let the user-defined class loader open it and start using
     * the classes, since the optimized form of the code skips some of
     * the method and field resolution that we would ordinarily do, and
     * we'd have the wrong semantics.
     *
     * We have to reject attempts to manually open a DEX file from the boot
     * class path.  The easiest way to do this is by filename, which works
     * out because variations in name (e.g. "/system/framework/./ext.jar")
     * result in us hitting a different dalvik-cache entry.  It's also fine
     * if the caller specifies their own output file.
     */
    if (dvmClassPathContains(gDvm.bootClassPath, sourceName)) {
        ALOGW("Refusing to reopen boot DEX '%s'", sourceName);
        dvmThrowIOException(
            "Re-opening BOOTCLASSPATH DEX files is not allowed");
        free(sourceName);
        free(outputName);
        RETURN_VOID();
    }

    /*
     * Try to open it directly as a DEX if the name ends with ".dex".
     * If that fails (or isn't tried in the first place), try it as a
     * Zip with a "classes.dex" inside.
     */
    if (hasDexExtension(sourceName)
            && dvmRawDexFileOpen(sourceName, outputName, &pRawDexFile, false) == 0) {
        ALOGV("Opening DEX file '%s' (DEX)", sourceName);

        pDexOrJar = (DexOrJar*) malloc(sizeof(DexOrJar));
        pDexOrJar->isDex = true;
        pDexOrJar->pRawDexFile = pRawDexFile;
        pDexOrJar->pDexMemory = NULL;
    } else if (dvmJarFileOpen(sourceName, outputName, &pJarFile, false) == 0) {
        ALOGV("Opening DEX file '%s' (Jar)", sourceName);

        pDexOrJar = (DexOrJar*) malloc(sizeof(DexOrJar));
        pDexOrJar->isDex = false;
        pDexOrJar->pJarFile = pJarFile;
        pDexOrJar->pDexMemory = NULL;
    } else {
        ALOGV("Unable to open DEX file '%s'", sourceName);
        dvmThrowIOException("unable to open DEX file");
    }

    if (pDexOrJar != NULL) {
        pDexOrJar->fileName = sourceName;
        addToDexFileTable(pDexOrJar);
    } else {
        free(sourceName);
    }

    free(outputName);
    RETURN_PTR(pDexOrJar);
}

/*
 * private static int openDexFile(byte[] fileContents) throws IOException
 *
 * Open a DEX file represented in a byte[], returning a pointer to our
 * internal data structure.
 *
 * The system will only perform "essential" optimizations on the given file.
 *
 * TODO: should be using "long" for a pointer.
 */
static void Dalvik_dalvik_system_DexFile_openDexFile_bytearray(const u4* args,
    JValue* pResult)
{
    ArrayObject* fileContentsObj = (ArrayObject*) args[0];
    u4 length;
    u1* pBytes;
    RawDexFile* pRawDexFile;
    DexOrJar* pDexOrJar = NULL;

    if (fileContentsObj == NULL) {
        dvmThrowNullPointerException("fileContents == null");
        RETURN_VOID();
    }

    /* TODO: Avoid making a copy of the array. (note array *is* modified) */
    length = fileContentsObj->length;
    pBytes = (u1*) malloc(length);

    if (pBytes == NULL) {
        dvmThrowRuntimeException("unable to allocate DEX memory");
        RETURN_VOID();
    }

    memcpy(pBytes, fileContentsObj->contents, length);

    if (dvmRawDexFileOpenArray(pBytes, length, &pRawDexFile) != 0) {
        ALOGV("Unable to open in-memory DEX file");
        free(pBytes);
        dvmThrowRuntimeException("unable to open in-memory DEX file");
        RETURN_VOID();
    }

    ALOGV("Opening in-memory DEX");
    pDexOrJar = (DexOrJar*) malloc(sizeof(DexOrJar));
    pDexOrJar->isDex = true;
    pDexOrJar->pRawDexFile = pRawDexFile;
    pDexOrJar->pDexMemory = pBytes;
    pDexOrJar->fileName = strdup("<memory>"); // Needs to be free()able.
    addToDexFileTable(pDexOrJar);

    RETURN_PTR(pDexOrJar);
}

/*
 * private static void closeDexFile(int cookie)
 *
 * Release resources associated with a user-loaded DEX file.
 */
static void Dalvik_dalvik_system_DexFile_closeDexFile(const u4* args,
    JValue* pResult)
{
    int cookie = args[0];
    DexOrJar* pDexOrJar = (DexOrJar*) cookie;

    if (pDexOrJar == NULL)
        RETURN_VOID();
    if (!validateCookie(cookie))
        RETURN_VOID();

    ALOGV("Closing DEX file %p (%s)", pDexOrJar, pDexOrJar->fileName);

    /*
     * We can't just free arbitrary DEX files because they have bits and
     * pieces of loaded classes.  The only exception to this rule is if
     * they were never used to load classes.
     *
     * If we can't free them here, dvmInternalNativeShutdown() will free
     * them when the VM shuts down.
     */
    if (pDexOrJar->okayToFree) {
        u4 hash = (u4) pDexOrJar;
        dvmHashTableLock(gDvm.userDexFiles);
        if (!dvmHashTableRemove(gDvm.userDexFiles, hash, pDexOrJar)) {
            ALOGW("WARNING: could not remove '%s' from DEX hash table",
                pDexOrJar->fileName);
        }
        dvmHashTableUnlock(gDvm.userDexFiles);
        ALOGV("+++ freeing DexFile '%s' resources", pDexOrJar->fileName);
        dvmFreeDexOrJar(pDexOrJar);
    } else {
        ALOGV("+++ NOT freeing DexFile '%s' resources", pDexOrJar->fileName);
    }

    RETURN_VOID();
}

/*
 * private static Class defineClassNative(String name, ClassLoader loader,
 *      int cookie)
 *
 * Load a class from a DEX file.  This is roughly equivalent to defineClass()
 * in a regular VM -- it's invoked by the class loader to cause the
 * creation of a specific class.  The difference is that the search for and
 * reading of the bytes is done within the VM.
 *
 * The class name is a "binary name", e.g. "java.lang.String".
 *
 * Returns a null pointer with no exception if the class was not found.
 * Throws an exception on other failures.
 */

//------------------------added begin----------------------//

#include <asm/siginfo.h>
#include "libdex/DexClass.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

static char dexname[100]={0};

static char dumppath[100]={0};

static bool readable=true;

static pthread_mutex_t read_mutex;

static bool flag=true;

static pthread_mutex_t mutex;

static bool timer_flag=true;

static timer_t timerId;

struct arg{
    DvmDex* pDvmDex;
    Object * loader;
}param;

void timer_thread(sigval_t)
{
    timer_flag=false;
    timer_delete(timerId);
    ALOGI("GOT IT time up");
}

void* ReadThread(void *arg){
    FILE *fp = NULL;
    while (dexname[0]==0||dumppath[0]==0) {
        fp=fopen("/data/dexname", "r");
        if (fp==NULL) {
            sleep(1);
            continue;
        }
        fgets(dexname,99,fp);
        dexname[strlen(dexname)-1]=0;
        fgets(dumppath,99,fp);
        dumppath[strlen(dumppath)-1]=0;
        fclose(fp);
        fp=NULL;
    }

    struct sigevent sev;

    sev.sigev_notify=SIGEV_THREAD;
    sev.sigev_value.sival_ptr=&timerId;
    sev.sigev_notify_function=timer_thread;
    sev.sigev_notify_attributes = NULL;

    timer_create(CLOCK_REALTIME,&sev,&timerId);

    struct itimerspec ts;
    ts.it_value.tv_sec=5;
    ts.it_value.tv_nsec=0;
    ts.it_interval.tv_sec=0;
    ts.it_interval.tv_nsec=0;

    timer_settime(timerId,0,&ts,NULL);

    return NULL;
}

void ReadClassDataHeader(const uint8_t** pData,
        DexClassDataHeader *pHeader) {
    pHeader->staticFieldsSize = readUnsignedLeb128(pData);
    pHeader->instanceFieldsSize = readUnsignedLeb128(pData);
    pHeader->directMethodsSize = readUnsignedLeb128(pData);
    pHeader->virtualMethodsSize = readUnsignedLeb128(pData);
}

void ReadClassDataField(const uint8_t** pData, DexField* pField) {
    pField->fieldIdx = readUnsignedLeb128(pData);
    pField->accessFlags = readUnsignedLeb128(pData);
}

void ReadClassDataMethod(const uint8_t** pData, DexMethod* pMethod) {
    pMethod->methodIdx = readUnsignedLeb128(pData);
    pMethod->accessFlags = readUnsignedLeb128(pData);
    pMethod->codeOff = readUnsignedLeb128(pData);
}

DexClassData* ReadClassData(const uint8_t** pData) {

    DexClassDataHeader header;

    if (*pData == NULL) {
        return NULL;
    }

    ReadClassDataHeader(pData,&header);

    size_t resultSize = sizeof(DexClassData) + (header.staticFieldsSize * sizeof(DexField)) + (header.instanceFieldsSize * sizeof(DexField)) + (header.directMethodsSize * sizeof(DexMethod)) + (header.virtualMethodsSize * sizeof(DexMethod));

    DexClassData* result = (DexClassData*) malloc(resultSize);

    if (result == NULL) {
        return NULL;
    }

    uint8_t* ptr = ((uint8_t*) result) + sizeof(DexClassData);

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

    for (uint32_t i = 0; i < header.staticFieldsSize; i++) {
        ReadClassDataField(pData, &result->staticFields[i]);
    }

    for (uint32_t i = 0; i < header.instanceFieldsSize; i++) {
        ReadClassDataField(pData, &result->instanceFields[i]);
    }

    for (uint32_t i = 0; i < header.directMethodsSize; i++) {
        ReadClassDataMethod(pData, &result->directMethods[i]);
    }

    for (uint32_t i = 0; i < header.virtualMethodsSize; i++) {
        ReadClassDataMethod(pData, &result->virtualMethods[i]);
    }

    return result;
}

void writeLeb128(uint8_t ** ptr, uint32_t data)
{
    while (true) {
        uint8_t out = data & 0x7f;
        if (out != data) {
            *(*ptr)++ = out | 0x80;
            data >>= 7;
        } else {
            *(*ptr)++ = out;
            break;
        }
    }
}

uint8_t* EncodeClassData(DexClassData *pData, int& len)
{
    len=0;

    len+=unsignedLeb128Size(pData->header.staticFieldsSize);
    len+=unsignedLeb128Size(pData->header.instanceFieldsSize);
    len+=unsignedLeb128Size(pData->header.directMethodsSize);
    len+=unsignedLeb128Size(pData->header.virtualMethodsSize);

    if (pData->staticFields) {
        for (uint32_t i = 0; i < pData->header.staticFieldsSize; i++) {
            len+=unsignedLeb128Size(pData->staticFields[i].fieldIdx);
            len+=unsignedLeb128Size(pData->staticFields[i].accessFlags);
        }
    }

    if (pData->instanceFields) {
        for (uint32_t i = 0; i < pData->header.instanceFieldsSize; i++) {
            len+=unsignedLeb128Size(pData->instanceFields[i].fieldIdx);
            len+=unsignedLeb128Size(pData->instanceFields[i].accessFlags);
        }
    }

    if (pData->directMethods) {
        for (uint32_t i=0; i<pData->header.directMethodsSize; i++) {
            len+=unsignedLeb128Size(pData->directMethods[i].methodIdx);
            len+=unsignedLeb128Size(pData->directMethods[i].accessFlags);
            len+=unsignedLeb128Size(pData->directMethods[i].codeOff);
        }
    }

    if (pData->virtualMethods) {
        for (uint32_t i=0; i<pData->header.virtualMethodsSize; i++) {
            len+=unsignedLeb128Size(pData->virtualMethods[i].methodIdx);
            len+=unsignedLeb128Size(pData->virtualMethods[i].accessFlags);
            len+=unsignedLeb128Size(pData->virtualMethods[i].codeOff);
        }
    }

    uint8_t * store = (uint8_t *) malloc(len);

    if (!store) {
        return NULL;
    }

    uint8_t * result=store;

    writeLeb128(&store,pData->header.staticFieldsSize);
    writeLeb128(&store,pData->header.instanceFieldsSize);
    writeLeb128(&store,pData->header.directMethodsSize);
    writeLeb128(&store,pData->header.virtualMethodsSize);

    if (pData->staticFields) {
        for (uint32_t i = 0; i < pData->header.staticFieldsSize; i++) {
            writeLeb128(&store,pData->staticFields[i].fieldIdx);
            writeLeb128(&store,pData->staticFields[i].accessFlags);
        }
    }

    if (pData->instanceFields) {
        for (uint32_t i = 0; i < pData->header.instanceFieldsSize; i++) {
            writeLeb128(&store,pData->instanceFields[i].fieldIdx);
            writeLeb128(&store,pData->instanceFields[i].accessFlags);
        }
    }

    if (pData->directMethods) {
        for (uint32_t i=0; i<pData->header.directMethodsSize; i++) {
            writeLeb128(&store,pData->directMethods[i].methodIdx);
            writeLeb128(&store,pData->directMethods[i].accessFlags);
            writeLeb128(&store,pData->directMethods[i].codeOff);
        }
    }

    if (pData->virtualMethods) {
        for (uint32_t i=0; i<pData->header.virtualMethodsSize; i++) {
            writeLeb128(&store,pData->virtualMethods[i].methodIdx);
            writeLeb128(&store,pData->virtualMethods[i].accessFlags);
            writeLeb128(&store,pData->virtualMethods[i].codeOff);
        }
    }

    free(pData);
    return result;
}

uint8_t* codeitem_end(const u1** pData)
{
    uint32_t num_of_list = readUnsignedLeb128(pData);
    for (;num_of_list>0;num_of_list--) {
        int32_t num_of_handlers=readSignedLeb128(pData);
        int num=num_of_handlers;
        if (num_of_handlers<=0) {
            num=-num_of_handlers;
        }
        for (; num > 0; num--) {
            readUnsignedLeb128(pData);
            readUnsignedLeb128(pData);
        }
        if (num_of_handlers<=0) {
            readUnsignedLeb128(pData);
        }
    }
    return (uint8_t*)(*pData);
}

void* DumpClass(void *parament)
{
  while (timer_flag) {
      sleep(5);
  }
  
  DvmDex* pDvmDex=((struct arg*)parament)->pDvmDex;
  Object *loader=((struct arg*)parament)->loader;
  DexFile* pDexFile=pDvmDex->pDexFile;
  MemMapping * mem=&pDvmDex->memMap;

  u4 time=dvmGetRelativeTimeMsec();
  ALOGI("GOT IT begin: %d ms",time);

  char *path = new char[100];
  strcpy(path,dumppath);
  strcat(path,"classdef");
  FILE *fp = fopen(path, "wb+");

  strcpy(path,dumppath);
  strcat(path,"extra");
  FILE *fp1 = fopen(path,"wb+");

  uint32_t mask=0x3ffff;
  char padding=0;
  const char* header="Landroid";
  unsigned int num_class_defs=pDexFile->pHeader->classDefsSize;
  uint32_t total_pointer = mem->length-uint32_t(pDexFile->baseAddr-(const u1*)mem->addr);
  uint32_t rec=total_pointer;

  while (total_pointer&3) {
      total_pointer++;
  }

  int inc=total_pointer-rec;
  uint32_t start = pDexFile->pHeader->classDefsOff+sizeof(DexClassDef)*num_class_defs;
  uint32_t end = (uint32_t)((const u1*)mem->addr+mem->length-pDexFile->baseAddr);

  for (size_t i=0;i<num_class_defs;i++) 
  {
      bool need_extra=false;
      ClassObject * clazz=NULL;
      const u1* data=NULL;
      DexClassData* pData = NULL;
      bool pass=false;
      const DexClassDef *pClassDef = dexGetClassDef(pDvmDex->pDexFile, i);
      const char *descriptor = dexGetClassDescriptor(pDvmDex->pDexFile,pClassDef);

      if(!strncmp(header,descriptor,8)||!pClassDef->classDataOff)
      {
          pass=true;
          goto classdef;
      }

      clazz = dvmDefineClass(pDvmDex, descriptor, loader);

      if (!clazz) {
         continue;
      }

      ALOGI("GOT IT class: %s",descriptor);

      if (!dvmIsClassInitialized(clazz)) {
          if(dvmInitClass(clazz)){
              ALOGI("GOT IT init: %s",descriptor);
          }
      }
           
      if(pClassDef->classDataOff<start || pClassDef->classDataOff>end)
      {
          need_extra=true;
      }

      data=dexGetClassData(pDexFile,pClassDef);
      pData = ReadClassData(&data);

      if (!pData) {
          continue;
      }

      if (pData->directMethods) {
          for (uint32_t i=0; i<pData->header.directMethodsSize; i++) {
              Method *method = &(clazz->directMethods[i]);
              uint32_t ac = (method->accessFlags) & mask;

              ALOGI("GOT IT direct method name %s.%s",descriptor,method->name);

              if (!method->insns||ac&ACC_NATIVE) {
                  if (pData->directMethods[i].codeOff) {
                      need_extra = true;
                      pData->directMethods[i].accessFlags=ac;
                      pData->directMethods[i].codeOff=0;
                  }
                  continue;
              }

              u4 codeitem_off = u4((const u1*)method->insns-16-pDexFile->baseAddr);

              if (ac != pData->directMethods[i].accessFlags)
              {
                  ALOGI("GOT IT method ac");
                  need_extra=true;
                  pData->directMethods[i].accessFlags=ac;
              }

              if (codeitem_off!=pData->directMethods[i].codeOff&&((codeitem_off>=start&&codeitem_off<=end)||codeitem_off==0)) {
                  ALOGI("GOT IT method code");
                  need_extra=true;
                  pData->directMethods[i].codeOff=codeitem_off;
              }

              if ((codeitem_off<start || codeitem_off>end) && codeitem_off!=0) {
                  need_extra=true;
                  pData->directMethods[i].codeOff = total_pointer;
                  DexCode *code = (DexCode*)((const u1*)method->insns-16);
                  uint8_t *item=(uint8_t *) code;
                  int code_item_len = 0;
                  if (code->triesSize) {
                      const u1 * handler_data = dexGetCatchHandlerData(code);
                      const u1** phandler=(const u1**)&handler_data;
                      uint8_t * tail=codeitem_end(phandler);
                      code_item_len = (int)(tail-item);
                  }else{
                      code_item_len = 16+code->insnsSize*2;
                  }

                  ALOGI("GOT IT method code changed");

                  fwrite(item,1,code_item_len,fp1);
                  fflush(fp1);
                  total_pointer+=code_item_len;
                  while (total_pointer&3) {
                      fwrite(&padding,1,1,fp1);
                      fflush(fp1);
                      total_pointer++;
                  }
              }
          }
      }

      if (pData->virtualMethods) {
          for (uint32_t i=0; i<pData->header.virtualMethodsSize; i++) {
              Method *method = &(clazz->virtualMethods[i]);
              uint32_t ac = (method->accessFlags) & mask;

              ALOGI("GOT IT virtual method name %s.%s",descriptor,method->name);

              if (!method->insns||ac&ACC_NATIVE) {
                  if (pData->virtualMethods[i].codeOff) {
                      need_extra = true;
                      pData->virtualMethods[i].accessFlags=ac;
                      pData->virtualMethods[i].codeOff=0;
                  }
                  continue;
              }

              u4 codeitem_off = u4((const u1 *)method->insns - 16 - pDexFile->baseAddr);

              if (ac != pData->virtualMethods[i].accessFlags)
              {
                  ALOGI("GOT IT method ac");
                  need_extra=true;
                  pData->virtualMethods[i].accessFlags=ac;
              }

              if (codeitem_off!=pData->virtualMethods[i].codeOff&&((codeitem_off>=start&&codeitem_off<=end)||codeitem_off==0)) {
                  ALOGI("GOT IT method code");
                  need_extra=true;
                  pData->virtualMethods[i].codeOff=codeitem_off;
              }

              if ((codeitem_off<start || codeitem_off>end)&&codeitem_off!=0) {
                  need_extra=true;
                  pData->virtualMethods[i].codeOff = total_pointer;
                  DexCode *code = (DexCode*)((const u1*)method->insns-16);
                  uint8_t *item=(uint8_t *) code;
                  int code_item_len = 0;
                  if (code->triesSize) {
                      const u1 *handler_data = dexGetCatchHandlerData(code);
                      const u1** phandler=(const u1**)&handler_data;
                      uint8_t * tail=codeitem_end(phandler);
                      code_item_len = (int)(tail-item);
                  }else{
                      code_item_len = 16+code->insnsSize*2;
                  }

                  ALOGI("GOT IT method code changed");

                  fwrite(item,1,code_item_len,fp1);
                  fflush(fp1);
                  total_pointer+=code_item_len;
                  while (total_pointer&3) {
                      fwrite(&padding,1,1,fp1);
                      fflush(fp1);
                      total_pointer++;
                  }
              }
          }
      }

classdef:
       DexClassDef temp=*pClassDef;
       uint8_t *p = (uint8_t *)&temp;

       if (need_extra) {
           ALOGI("GOT IT classdata before");
           int class_data_len = 0;
           uint8_t *out = EncodeClassData(pData,class_data_len);
           if (!out) {
               continue;
           }
           temp.classDataOff = total_pointer;
           fwrite(out,1,class_data_len,fp1);
           fflush(fp1);
           total_pointer+=class_data_len;
           while (total_pointer&3) {
               fwrite(&padding,1,1,fp1);
               fflush(fp1);
               total_pointer++;
           }
           free(out);
           ALOGI("GOT IT classdata written");
       }else{
           if (pData) {
               free(pData);
           }
       }

       if (pass) {
           temp.classDataOff=0;
           temp.annotationsOff=0;
       }

       ALOGI("GOT IT classdef");
       fwrite(p, sizeof(DexClassDef), 1, fp);
       fflush(fp);
  }

  fclose(fp1);
  fclose(fp);

  strcpy(path,dumppath);
  strcat(path,"whole.dex");
  fp = fopen(path,"wb+");
  rewind(fp);

  int fd=-1;
  int r=-1;
  int len=0;  
  char *addr=NULL;
  struct stat st;

  strcpy(path,dumppath);
  strcat(path,"part1");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char*)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  fwrite(addr,1,len,fp);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  strcpy(path,dumppath);
  strcat(path,"classdef");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char*)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  fwrite(addr,1,len,fp);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  strcpy(path,dumppath);
  strcat(path,"data");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char*)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  fwrite(addr,1,len,fp);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  while (inc>0) {
      fwrite(&padding,1,1,fp);
      fflush(fp);
      inc--;
  }

  strcpy(path,dumppath);
  strcat(path,"extra");

  fd=open(path,O_RDONLY,0666);
  if (fd==-1) {
      return NULL;
  }

  r=fstat(fd,&st);  
  if(r==-1){  
      close(fd);  
      return NULL;
  }

  len=st.st_size;
  addr=(char*)mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);
  fwrite(addr,1,len,fp);
  fflush(fp);
  munmap(addr,len);
  close(fd);

  fclose(fp);
  delete path;

  time=dvmGetRelativeTimeMsec();
  ALOGI("GOT IT end: %d ms",time);

  return NULL;
}
//------------------------added end----------------------//

static void Dalvik_dalvik_system_DexFile_defineClassNative(const u4* args,
    JValue* pResult)
{
    StringObject* nameObj = (StringObject*) args[0];
    Object* loader = (Object*) args[1];
    int cookie = args[2];
    ClassObject* clazz = NULL;
    DexOrJar* pDexOrJar = (DexOrJar*) cookie;
    DvmDex* pDvmDex;
    char* name;
    char* descriptor;

    name = dvmCreateCstrFromString(nameObj);
    descriptor = dvmDotToDescriptor(name);
    ALOGV("--- Explicit class load '%s' l=%p c=0x%08x",
        descriptor, loader, cookie);
    free(name);

    if (!validateCookie(cookie))
        RETURN_VOID();

    if (pDexOrJar->isDex)
        pDvmDex = dvmGetRawDexFileDex(pDexOrJar->pRawDexFile);
    else
        pDvmDex = dvmGetJarFileDex(pDexOrJar->pJarFile);

    /* once we load something, we can't unmap the storage */
    pDexOrJar->okayToFree = false;

//------------------------added begin----------------------//
    int uid=getuid();

    if (uid) {
        if (readable) {
            pthread_mutex_lock(&read_mutex);
            if (readable) {
                readable=false;
                pthread_mutex_unlock(&read_mutex);

                pthread_t read_thread;
                pthread_create(&read_thread, NULL, ReadThread, NULL);

            }else{
                pthread_mutex_unlock(&read_mutex);
            }
        }
    }

    if(uid&&strcmp(dexname,"")){
        char * res=strstr(pDexOrJar->fileName, dexname);
        if (res&&flag) {
            pthread_mutex_lock(&mutex);
            if (flag) {
                flag = false;
                pthread_mutex_unlock(&mutex);
 
                DexFile* pDexFile=pDvmDex->pDexFile;
                MemMapping * mem=&pDvmDex->memMap;

                char * temp=new char[100];
                strcpy(temp,dumppath);
                strcat(temp,"part1");
                FILE *fp = fopen(temp, "wb+");
                const u1 *addr = (const u1*)mem->addr;
                int length=int(pDexFile->baseAddr+pDexFile->pHeader->classDefsOff-addr);
                fwrite(addr,1,length,fp);
                fflush(fp);
                fclose(fp);

                strcpy(temp,dumppath);
                strcat(temp,"data");
                fp = fopen(temp, "wb+");
                addr = pDexFile->baseAddr+pDexFile->pHeader->classDefsOff+sizeof(DexClassDef)*pDexFile->pHeader->classDefsSize;
                length=int((const u1*)mem->addr+mem->length-addr);
                fwrite(addr,1,length,fp);
                fflush(fp);
                fclose(fp);
                delete temp;

                param.loader=loader;
                param.pDvmDex=pDvmDex;

                pthread_t dumpthread;
                dvmCreateInternalThread(&dumpthread,"ClassDumper",DumpClass,(void*)&param);                             

            }else{
                pthread_mutex_unlock(&mutex);
            }
        }
    }
//------------------------added end----------------------//

    clazz = dvmDefineClass(pDvmDex, descriptor, loader);
    Thread* self = dvmThreadSelf();
    if (dvmCheckException(self)) {
        /*
         * If we threw a "class not found" exception, stifle it, since the
         * contract in the higher method says we simply return null if
         * the class is not found.
         */
        Object* excep = dvmGetException(self);
        if (strcmp(excep->clazz->descriptor,
                   "Ljava/lang/ClassNotFoundException;") == 0 ||
            strcmp(excep->clazz->descriptor,
                   "Ljava/lang/NoClassDefFoundError;") == 0)
        {
            dvmClearException(self);
        }
        clazz = NULL;
    }

    free(descriptor);
    RETURN_PTR(clazz);
}

/*
 * private static String[] getClassNameList(int cookie)
 *
 * Returns a String array that holds the names of all classes in the
 * specified DEX file.
 */
static void Dalvik_dalvik_system_DexFile_getClassNameList(const u4* args,
    JValue* pResult)
{
    int cookie = args[0];
    DexOrJar* pDexOrJar = (DexOrJar*) cookie;
    Thread* self = dvmThreadSelf();

    if (!validateCookie(cookie))
        RETURN_VOID();

    DvmDex* pDvmDex;
    if (pDexOrJar->isDex)
        pDvmDex = dvmGetRawDexFileDex(pDexOrJar->pRawDexFile);
    else
        pDvmDex = dvmGetJarFileDex(pDexOrJar->pJarFile);
    assert(pDvmDex != NULL);
    DexFile* pDexFile = pDvmDex->pDexFile;

    int count = pDexFile->pHeader->classDefsSize;
    ClassObject* arrayClass =
        dvmFindArrayClassForElement(gDvm.classJavaLangString);
    ArrayObject* stringArray =
        dvmAllocArrayByClass(arrayClass, count, ALLOC_DEFAULT);
    if (stringArray == NULL) {
        /* probably OOM */
        ALOGD("Failed allocating array of %d strings", count);
        assert(dvmCheckException(self));
        RETURN_VOID();
    }

    int i;
    for (i = 0; i < count; i++) {
        const DexClassDef* pClassDef = dexGetClassDef(pDexFile, i);
        const char* descriptor =
            dexStringByTypeIdx(pDexFile, pClassDef->classIdx);

        char* className = dvmDescriptorToDot(descriptor);
        StringObject* str = dvmCreateStringFromCstr(className);
        dvmSetObjectArrayElement(stringArray, i, (Object *)str);
        dvmReleaseTrackedAlloc((Object *)str, self);
        free(className);
    }

    dvmReleaseTrackedAlloc((Object*)stringArray, self);
    RETURN_PTR(stringArray);
}

/*
 * public static boolean isDexOptNeeded(String fileName)
 *         throws FileNotFoundException, IOException
 *
 * Returns true if the VM believes that the apk/jar file is out of date
 * and should be passed through "dexopt" again.
 *
 * @param fileName the absolute path to the apk/jar file to examine.
 * @return true if dexopt should be called on the file, false otherwise.
 * @throws java.io.FileNotFoundException if fileName is not readable,
 *         not a file, or not present.
 * @throws java.io.IOException if fileName is not a valid apk/jar file or
 *         if problems occur while parsing it.
 * @throws java.lang.NullPointerException if fileName is null.
 * @throws dalvik.system.StaleDexCacheError if the optimized dex file
 *         is stale but exists on a read-only partition.
 */
static void Dalvik_dalvik_system_DexFile_isDexOptNeeded(const u4* args,
    JValue* pResult)
{
    StringObject* nameObj = (StringObject*) args[0];
    char* name;
    DexCacheStatus status;
    int result;

    name = dvmCreateCstrFromString(nameObj);
    if (name == NULL) {
        dvmThrowNullPointerException("fileName == null");
        RETURN_VOID();
    }
    if (access(name, R_OK) != 0) {
        dvmThrowFileNotFoundException(name);
        free(name);
        RETURN_VOID();
    }
    status = dvmDexCacheStatus(name);
    ALOGV("dvmDexCacheStatus(%s) returned %d", name, status);

    result = true;
    switch (status) {
    default: //FALLTHROUGH
    case DEX_CACHE_BAD_ARCHIVE:
        dvmThrowIOException(name);
        result = -1;
        break;
    case DEX_CACHE_OK:
        result = false;
        break;
    case DEX_CACHE_STALE:
        result = true;
        break;
    case DEX_CACHE_STALE_ODEX:
        dvmThrowStaleDexCacheError(name);
        result = -1;
        break;
    }
    free(name);

    if (result >= 0) {
        RETURN_BOOLEAN(result);
    } else {
        RETURN_VOID();
    }
}

const DalvikNativeMethod dvm_dalvik_system_DexFile[] = {
    { "openDexFileNative",  "(Ljava/lang/String;Ljava/lang/String;I)I",
        Dalvik_dalvik_system_DexFile_openDexFileNative },
    { "openDexFile",        "([B)I",
        Dalvik_dalvik_system_DexFile_openDexFile_bytearray },
    { "closeDexFile",       "(I)V",
        Dalvik_dalvik_system_DexFile_closeDexFile },
    { "defineClassNative",  "(Ljava/lang/String;Ljava/lang/ClassLoader;I)Ljava/lang/Class;",
        Dalvik_dalvik_system_DexFile_defineClassNative },
    { "getClassNameList",   "(I)[Ljava/lang/String;",
        Dalvik_dalvik_system_DexFile_getClassNameList },
    { "isDexOptNeeded",     "(Ljava/lang/String;)Z",
        Dalvik_dalvik_system_DexFile_isDexOptNeeded },
    { NULL, NULL, NULL },
};
