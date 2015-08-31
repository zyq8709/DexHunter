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
#ifndef DALVIK_ALLOC_MARK_SWEEP_H_
#define DALVIK_ALLOC_MARK_SWEEP_H_

#include "alloc/HeapBitmap.h"
#include "alloc/HeapSource.h"

struct GcMarkStack {
    /* Highest address (exclusive)
     */
    const Object **limit;

    /* Current top of the stack (exclusive)
     */
    const Object **top;

    /* Lowest address (inclusive)
     */
    const Object **base;

    /* Maximum stack size, in bytes.
     */
    size_t length;
};

/* This is declared publicly so that it can be included in gDvm.gcHeap.
 */
struct GcMarkContext {
    HeapBitmap *bitmap;
    GcMarkStack stack;
    const char *immuneLimit;
    const void *finger;   // only used while scanning/recursing.
};

bool dvmHeapBeginMarkStep(bool isPartial);
void dvmHeapMarkRootSet(void);
void dvmHeapReMarkRootSet(void);
void dvmHeapScanMarkedObjects(void);
void dvmHeapReScanMarkedObjects(void);
void dvmHeapProcessReferences(Object **softReferences, bool clearSoftRefs,
                              Object **weakReferences,
                              Object **finalizerReferences,
                              Object **phantomReferences);
void dvmHeapFinishMarkStep(void);
void dvmHeapSweepSystemWeaks(void);
void dvmHeapSweepUnmarkedObjects(bool isPartial, bool isConcurrent,
                                 size_t *numObjects, size_t *numBytes);
void dvmEnqueueClearedReferences(Object **references);

#endif  // DALVIK_ALLOC_MARK_SWEEP_H_
