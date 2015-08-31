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
 * List all methods in all concrete classes in one or more DEX files.
 */

#include "libdex/DexFile.h"

#include "libdex/CmdUtils.h"
#include "libdex/DexClass.h"
#include "libdex/DexDebugInfo.h"
#include "libdex/DexProto.h"
#include "libdex/SysUtil.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

static const char* gProgName = "dexlist";

/* command-line args */
static struct {
    char*       argCopy;
    const char* classToFind;
    const char* methodToFind;
} gParms;


/*
 * Return a newly-allocated string for the "dot version" of the class
 * name for the given type descriptor. That is, The initial "L" and
 * final ";" (if any) have been removed and all occurrences of '/'
 * have been changed to '.'.
 */
static char* descriptorToDot(const char* str)
{
    size_t at = strlen(str);
    char* newStr;

    if (str[0] == 'L') {
        assert(str[at - 1] == ';');
        at -= 2; /* Two fewer chars to copy. */
        str++; /* Skip the 'L'. */
    }

    newStr = (char*)malloc(at + 1); /* Add one for the '\0'. */
    newStr[at] = '\0';

    while (at > 0) {
        at--;
        newStr[at] = (str[at] == '/') ? '.' : str[at];
    }

    return newStr;
}

/*
 * Position table callback; we just want to catch the number of the
 * first line in the method, which *should* correspond to the first
 * entry from the table.  (Could also use "min" here.)
 */
static int positionsCallback(void* cnxt, u4 address, u4 lineNum)
{
    int* pFirstLine = (int*) cnxt;
    if (*pFirstLine == -1)
        *pFirstLine = lineNum;
    return 0;
}


/*
 * Dump a method.
 */
void dumpMethod(DexFile* pDexFile, const char* fileName,
    const DexMethod* pDexMethod, int i)
{
    const DexMethodId* pMethodId;
    const DexCode* pCode;
    const char* classDescriptor;
    const char* methodName;
    int firstLine;

    /* abstract and native methods don't get listed */
    if (pDexMethod->codeOff == 0)
        return;

    pMethodId = dexGetMethodId(pDexFile, pDexMethod->methodIdx);
    methodName = dexStringById(pDexFile, pMethodId->nameIdx);

    classDescriptor = dexStringByTypeIdx(pDexFile, pMethodId->classIdx);

    pCode = dexGetCode(pDexFile, pDexMethod);
    assert(pCode != NULL);

    /*
     * If the filename is empty, then set it to something printable
     * so that it is easier to parse.
     *
     * TODO: A method may override its class's default source file by
     * specifying a different one in its debug info. This possibility
     * should be handled here.
     */
    if (fileName == NULL || fileName[0] == 0) {
        fileName = "(none)";
    }

    firstLine = -1;
    dexDecodeDebugInfo(pDexFile, pCode, classDescriptor, pMethodId->protoIdx,
        pDexMethod->accessFlags, positionsCallback, NULL, &firstLine);

    char* className = descriptorToDot(classDescriptor);
    char* desc = dexCopyDescriptorFromMethodId(pDexFile, pMethodId);
    u4 insnsOff = pDexMethod->codeOff + offsetof(DexCode, insns);

    if (gParms.methodToFind != NULL &&
        (strcmp(gParms.classToFind, className) != 0 ||
         strcmp(gParms.methodToFind, methodName) != 0))
    {
        goto skip;
    }

    printf("0x%08x %d %s %s %s %s %d\n",
        insnsOff, pCode->insnsSize * 2,
        className, methodName, desc,
        fileName, firstLine);

skip:
    free(desc);
    free(className);
}

/*
 * Run through all direct and virtual methods in the class.
 */
void dumpClass(DexFile* pDexFile, int idx)
{
    const DexClassDef* pClassDef;
    DexClassData* pClassData;
    const u1* pEncodedData;
    const char* fileName;
    int i;

    pClassDef = dexGetClassDef(pDexFile, idx);
    pEncodedData = dexGetClassData(pDexFile, pClassDef);
    pClassData = dexReadAndVerifyClassData(&pEncodedData, NULL);

    if (pClassData == NULL) {
        fprintf(stderr, "Trouble reading class data\n");
        return;
    }

    if (pClassDef->sourceFileIdx == 0xffffffff) {
        fileName = NULL;
    } else {
        fileName = dexStringById(pDexFile, pClassDef->sourceFileIdx);
    }

    /*
     * TODO: Each class def points at a sourceFile, so maybe that
     * should be printed out. However, this needs to be coordinated
     * with the tools that parse this output.
     */

    for (i = 0; i < (int) pClassData->header.directMethodsSize; i++) {
        dumpMethod(pDexFile, fileName, &pClassData->directMethods[i], i);
    }

    for (i = 0; i < (int) pClassData->header.virtualMethodsSize; i++) {
        dumpMethod(pDexFile, fileName, &pClassData->virtualMethods[i], i);
    }

    free(pClassData);
}

/*
 * Process a file.
 *
 * Returns 0 on success.
 */
int process(const char* fileName)
{
    DexFile* pDexFile = NULL;
    MemMapping map;
    bool mapped = false;
    int result = -1;
    UnzipToFileResult utfr;

    utfr = dexOpenAndMap(fileName, NULL, &map, true);
    if (utfr != kUTFRSuccess) {
        if (utfr == kUTFRNoClassesDex) {
            /* no classes.dex in the APK; pretend we succeeded */
            result = 0;
            goto bail;
        }
        fprintf(stderr, "Unable to process '%s'\n", fileName);
        goto bail;
    }
    mapped = true;

    pDexFile = dexFileParse((u1*)map.addr, map.length, kDexParseDefault);
    if (pDexFile == NULL) {
        fprintf(stderr, "Warning: DEX parse failed for '%s'\n", fileName);
        goto bail;
    }

    printf("#%s\n", fileName);

    int i;
    for (i = 0; i < (int) pDexFile->pHeader->classDefsSize; i++) {
        dumpClass(pDexFile, i);
    }

    result = 0;

bail:
    if (mapped)
        sysReleaseShmem(&map);
    if (pDexFile != NULL)
        dexFileFree(pDexFile);
    return result;
}


/*
 * Show usage.
 */
void usage(void)
{
    fprintf(stderr, "Copyright (C) 2007 The Android Open Source Project\n\n");
    fprintf(stderr, "%s: dexfile [dexfile2 ...]\n", gProgName);
    fprintf(stderr, "\n");
}

/*
 * Parse args.
 */
int main(int argc, char* const argv[])
{
    int result = 0;
    int i;

    /*
     * Find all instances of the fully-qualified method name.  This isn't
     * really what dexlist is for, but it's easy to do it here.
     */
    if (argc > 3 && strcmp(argv[1], "--method") == 0) {
        gParms.argCopy = strdup(argv[2]);
        char* meth = strrchr(gParms.argCopy, '.');
        if (meth == NULL) {
            fprintf(stderr, "Expected package.Class.method\n");
            free(gParms.argCopy);
            return 2;
        }
        *meth = '\0';
        gParms.classToFind = gParms.argCopy;
        gParms.methodToFind = meth+1;
        argv += 2;
        argc -= 2;
    }

    if (argc < 2) {
        fprintf(stderr, "%s: no file specified\n", gProgName);
        usage();
        return 2;
    }

    /*
     * Run through the list of files.  If one of them fails we contine on,
     * only returning a failure at the end.
     */
    for (i = 1; i < argc; i++)
        result |= process(argv[i]);

    free(gParms.argCopy);
    return result;
}
