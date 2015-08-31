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
 * Internal heap functions
 */
#ifndef DALVIK_ALLOC_HEAP_H_
#define DALVIK_ALLOC_HEAP_H_

struct GcSpec {
  /* If true, only the application heap is threatened. */
  bool isPartial;
  /* If true, the trace is run concurrently with the mutator. */
  bool isConcurrent;
  /* Toggles for the soft reference clearing policy. */
  bool doPreserve;
  /* A name for this garbage collection mode. */
  const char *reason;
};

/* Not enough space for an "ordinary" Object to be allocated. */
extern const GcSpec *GC_FOR_MALLOC;

/* Automatic GC triggered by exceeding a heap occupancy threshold. */
extern const GcSpec *GC_CONCURRENT;

/* Explicit GC via Runtime.gc(), VMRuntime.gc(), or SIGUSR1. */
extern const GcSpec *GC_EXPLICIT;

/* Final attempt to reclaim memory before throwing an OOM. */
extern const GcSpec *GC_BEFORE_OOM;

/*
 * Initialize the GC heap.
 *
 * Returns true if successful, false otherwise.
 */
bool dvmHeapStartup(void);

/*
 * Initialization that needs to wait until after leaving zygote mode.
 * This needs to be called before the first allocation or GC that
 * happens after forking.
 */
bool dvmHeapStartupAfterZygote(void);

/*
 * Tear down the GC heap.
 *
 * Frees all memory allocated via dvmMalloc() as
 * a side-effect.
 */
void dvmHeapShutdown(void);

/*
 * Stops any threads internal to the garbage collector.  Called before
 * the heap itself is shutdown.
 */
void dvmHeapThreadShutdown(void);

#if 0       // needs to be in Alloc.h so debug code can find it.
/*
 * Returns a number of bytes greater than or
 * equal to the size of the named object in the heap.
 *
 * Specifically, it returns the size of the heap
 * chunk which contains the object.
 */
size_t dvmObjectSizeInHeap(const Object *obj);
#endif

/*
 * Run the garbage collector without doing any locking.
 */
void dvmCollectGarbageInternal(const GcSpec *spec);

/*
 * Blocks the calling thread until the garbage collector is inactive.
 * The caller must hold the heap lock as this call releases and
 * re-acquires the heap lock.  After returning, no garbage collection
 * will be in progress and the heap lock will be held by the caller.
 */
bool dvmWaitForConcurrentGcToComplete(void);

/*
 * Returns true iff <obj> points to a valid allocated object.
 */
bool dvmIsValidObject(const Object* obj);

#endif  // DALVIK_ALLOC_HEAP_H_
