/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "DlMalloc.h"

#include <stdint.h>
#include "Common.h"

/* Dalvik specific morecore implementation defined in HeapSource.cpp. */
#define MORECORE(x) dvmHeapSourceMorecore(m, x)
extern void* dvmHeapSourceMorecore(void* mspace, intptr_t increment);

/* Custom heap error handling. */
#define PROCEED_ON_ERROR 0
static void heap_error(const char* msg, const char* function, void* p);
#define CORRUPTION_ERROR_ACTION(m) \
    heap_error("HEAP MEMORY CORRUPTION", __FUNCTION__, NULL)
#define USAGE_ERROR_ACTION(m,p) \
    heap_error("ARGUMENT IS INVALID HEAP ADDRESS", __FUNCTION__, p)

/*
 * Ugly inclusion of C file so that Dalvik specific #defines configure
 * dlmalloc for our use for mspaces (regular dlmalloc is still declared
 * in bionic).
 */
#include "../../../bionic/libc/upstream-dlmalloc/malloc.c"


static void heap_error(const char* msg, const char* function, void* p) {
    ALOG(LOG_FATAL, LOG_TAG, "@@@ ABORTING: DALVIK: %s IN %s addr=%p", msg,
         function, p);
    /* So that we can get a memory dump around p */
    *((int **) 0xdeadbaad) = (int *) p;
}
