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

#ifndef DALVIK_VM_ALLOC_DLMALLOC_H_
#define DALVIK_VM_ALLOC_DLMALLOC_H_

/* Configure dlmalloc for mspaces. */
#define HAVE_MMAP 0
#define HAVE_MREMAP 0
#define HAVE_MORECORE 1
#define MSPACES 1
#define NO_MALLINFO 1
#define ONLY_MSPACES 1
#define MALLOC_INSPECT_ALL 1

/* Include the proper definitions. */
#include "../../../bionic/libc/upstream-dlmalloc/malloc.h"

/*
 * Define dlmalloc routines from bionic that cannot be included
 * directly because of redefining symbols from the include above.
 */
extern "C" void dlmalloc_inspect_all(void(*handler)(void*, void *, size_t, void*),
                                     void* arg);
extern "C" int  dlmalloc_trim(size_t);
extern "C" void* dlmem2chunk(void* mem);

#endif  // DALVIK_VM_ALLOC_DLMALLOC_H_
