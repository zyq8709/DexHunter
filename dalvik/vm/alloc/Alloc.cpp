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
 * Garbage-collecting memory allocator.
 */
#include "Dalvik.h"
#include "Globals.h"
#include "alloc/Heap.h"
#include "alloc/HeapInternal.h"
#include "alloc/HeapSource.h"
#include "cutils/atomic.h"
#include "cutils/atomic-inline.h"

/*
 * Initialize the GC universe.
 *
 * We're currently using a memory-mapped arena to keep things off of the
 * main heap.  This needs to be replaced with something real.
 */
bool dvmGcStartup()
{
    dvmInitMutex(&gDvm.gcHeapLock);
    pthread_cond_init(&gDvm.gcHeapCond, NULL);
    return dvmHeapStartup();
}

/*
 * Post-zygote heap initialization, including starting
 * the HeapWorker thread.
 */
bool dvmGcStartupAfterZygote()
{
    return dvmHeapStartupAfterZygote();
}

/*
 * Shutdown the threads internal to the garbage collector.
 */
void dvmGcThreadShutdown()
{
    dvmHeapThreadShutdown();
}

/*
 * Shut the GC down.
 */
void dvmGcShutdown()
{
    //TODO: grab and destroy the lock
    dvmHeapShutdown();
}

/*
 * Do any last-minute preparation before we call fork() for the first time.
 */
bool dvmGcPreZygoteFork()
{
    return dvmHeapSourceStartupBeforeFork();
}

bool dvmGcStartupClasses()
{
    ClassObject *klass = dvmFindSystemClass("Ljava/lang/Daemons;");
    if (klass == NULL) {
        return false;
    }
    Method *method = dvmFindDirectMethodByDescriptor(klass, "start", "()V");
    if (method == NULL) {
        return false;
    }
    Thread *self = dvmThreadSelf();
    assert(self != NULL);
    JValue unusedResult;
    dvmCallMethod(self, method, NULL, &unusedResult);
    return true;
}

/*
 * Create a "stock instance" of an exception class.
 */
static Object* createStockException(const char* descriptor, const char* msg)
{
    Thread* self = dvmThreadSelf();
    StringObject* msgStr = NULL;
    ClassObject* clazz;
    Method* init;
    Object* obj;

    /* find class, initialize if necessary */
    clazz = dvmFindSystemClass(descriptor);
    if (clazz == NULL) {
        ALOGE("Unable to find %s", descriptor);
        return NULL;
    }

    init = dvmFindDirectMethodByDescriptor(clazz, "<init>",
            "(Ljava/lang/String;)V");
    if (init == NULL) {
        ALOGE("Unable to find String-arg constructor for %s", descriptor);
        return NULL;
    }

    obj = dvmAllocObject(clazz, ALLOC_DEFAULT);
    if (obj == NULL)
        return NULL;

    if (msg == NULL) {
        msgStr = NULL;
    } else {
        msgStr = dvmCreateStringFromCstr(msg);
        if (msgStr == NULL) {
            ALOGW("Could not allocate message string \"%s\"", msg);
            dvmReleaseTrackedAlloc(obj, self);
            return NULL;
        }
    }

    JValue unused;
    dvmCallMethod(self, init, obj, &unused, msgStr);
    if (dvmCheckException(self)) {
        dvmReleaseTrackedAlloc((Object*) msgStr, self);
        dvmReleaseTrackedAlloc(obj, self);
        return NULL;
    }

    dvmReleaseTrackedAlloc((Object*) msgStr, self);     // okay if msgStr NULL
    return obj;
}

/*
 * Create some "stock" exceptions.  These can be thrown when the system is
 * too screwed up to allocate and initialize anything, or when we don't
 * need a meaningful stack trace.
 *
 * We can't do this during the initial startup because we need to execute
 * the constructors.
 */
bool dvmCreateStockExceptions()
{
    /*
     * Pre-allocate some throwables.  These need to be explicitly added
     * to the GC's root set (see dvmHeapMarkRootSet()).
     */
    gDvm.outOfMemoryObj = createStockException("Ljava/lang/OutOfMemoryError;",
        "[memory exhausted]");
    dvmReleaseTrackedAlloc(gDvm.outOfMemoryObj, NULL);
    gDvm.internalErrorObj = createStockException("Ljava/lang/InternalError;",
        "[pre-allocated]");
    dvmReleaseTrackedAlloc(gDvm.internalErrorObj, NULL);
    gDvm.noClassDefFoundErrorObj =
        createStockException("Ljava/lang/NoClassDefFoundError;",
            "[generic]");
    dvmReleaseTrackedAlloc(gDvm.noClassDefFoundErrorObj, NULL);

    if (gDvm.outOfMemoryObj == NULL || gDvm.internalErrorObj == NULL ||
        gDvm.noClassDefFoundErrorObj == NULL)
    {
        ALOGW("Unable to create stock exceptions");
        return false;
    }

    return true;
}


/*
 * Create an instance of the specified class.
 *
 * Returns NULL and throws an exception on failure.
 */
Object* dvmAllocObject(ClassObject* clazz, int flags)
{
    Object* newObj;

    assert(clazz != NULL);
    assert(dvmIsClassInitialized(clazz) || dvmIsClassInitializing(clazz));

    /* allocate on GC heap; memory is zeroed out */
    newObj = (Object*)dvmMalloc(clazz->objectSize, flags);
    if (newObj != NULL) {
        DVM_OBJECT_INIT(newObj, clazz);
        dvmTrackAllocation(clazz, clazz->objectSize);   /* notify DDMS */
    }

    return newObj;
}

/*
 * Create a copy of an object, for Object.clone().
 *
 * We use the size actually allocated, rather than obj->clazz->objectSize,
 * because the latter doesn't work for array objects.
 */
Object* dvmCloneObject(Object* obj, int flags)
{
    assert(dvmIsValidObject(obj));
    ClassObject* clazz = obj->clazz;

    /* Class.java shouldn't let us get here (java.lang.Class is final
     * and does not implement Clonable), but make extra sure.
     * A memcpy() clone will wreak havoc on a ClassObject's "innards".
     */
    assert(!dvmIsTheClassClass(clazz));

    size_t size;
    if (IS_CLASS_FLAG_SET(clazz, CLASS_ISARRAY)) {
        size = dvmArrayObjectSize((ArrayObject *)obj);
    } else {
        size = clazz->objectSize;
    }

    Object* copy = (Object*)dvmMalloc(size, flags);
    if (copy == NULL)
        return NULL;

    DVM_OBJECT_INIT(copy, clazz);
    size_t offset = sizeof(Object);
    /* Copy instance data.  We assume memcpy copies by words. */
    memcpy((char*)copy + offset, (char*)obj + offset, size - offset);

    /* Mark the clone as finalizable if appropriate. */
    if (IS_CLASS_FLAG_SET(clazz, CLASS_ISFINALIZABLE)) {
        dvmSetFinalizable(copy);
    }

    dvmTrackAllocation(clazz, size);    /* notify DDMS */

    return copy;
}


/*
 * Track an object that was allocated internally and isn't yet part of the
 * VM root set.
 *
 * We could do this per-thread or globally.  If it's global we don't have
 * to do the thread lookup but we do have to synchronize access to the list.
 *
 * "obj" must not be NULL.
 *
 * NOTE: "obj" is not a fully-formed object; in particular, obj->clazz will
 * usually be NULL since we're being called from dvmMalloc().
 */
void dvmAddTrackedAlloc(Object* obj, Thread* self)
{
    if (self == NULL)
        self = dvmThreadSelf();

    assert(obj != NULL);
    assert(self != NULL);
    if (!dvmAddToReferenceTable(&self->internalLocalRefTable, obj)) {
        ALOGE("threadid=%d: unable to add %p to internal ref table",
            self->threadId, obj);
        dvmDumpThread(self, false);
        dvmAbort();
    }
}

/*
 * Stop tracking an object.
 *
 * We allow attempts to delete NULL "obj" so that callers don't have to wrap
 * calls with "if != NULL".
 */
void dvmReleaseTrackedAlloc(Object* obj, Thread* self)
{
    if (obj == NULL)
        return;

    if (self == NULL)
        self = dvmThreadSelf();
    assert(self != NULL);

    if (!dvmRemoveFromReferenceTable(&self->internalLocalRefTable,
            self->internalLocalRefTable.table, obj))
    {
        ALOGE("threadid=%d: failed to remove %p from internal ref table",
            self->threadId, obj);
        dvmAbort();
    }
}


/*
 * Explicitly initiate garbage collection.
 */
void dvmCollectGarbage()
{
    if (gDvm.disableExplicitGc) {
        return;
    }
    dvmLockHeap();
    dvmWaitForConcurrentGcToComplete();
    dvmCollectGarbageInternal(GC_EXPLICIT);
    dvmUnlockHeap();
}

/*
 * Run finalization.
 */
void dvmRunFinalization() {
  Thread *self = dvmThreadSelf();
  assert(self != NULL);
  JValue unusedResult;
  assert(gDvm.methJavaLangSystem_runFinalization != NULL);
  dvmCallMethod(self, gDvm.methJavaLangSystem_runFinalization, NULL, &unusedResult);
}

struct CountContext {
    const ClassObject *clazz;
    size_t count;
};

static void countInstancesOfClassCallback(Object *obj, void *arg)
{
    CountContext *ctx = (CountContext *)arg;
    assert(ctx != NULL);
    if (obj->clazz == ctx->clazz) {
        ctx->count += 1;
    }
}

size_t dvmCountInstancesOfClass(const ClassObject *clazz)
{
    CountContext ctx = { clazz, 0 };
    dvmLockHeap();
    HeapBitmap *bitmap = dvmHeapSourceGetLiveBits();
    dvmHeapBitmapWalk(bitmap, countInstancesOfClassCallback, &ctx);
    dvmUnlockHeap();
    return ctx.count;
}

static void countAssignableInstancesOfClassCallback(Object *obj, void *arg)
{
    CountContext *ctx = (CountContext *)arg;
    assert(ctx != NULL);
    if (obj->clazz != NULL && dvmInstanceof(obj->clazz, ctx->clazz)) {
        ctx->count += 1;
    }
}

size_t dvmCountAssignableInstancesOfClass(const ClassObject *clazz)
{
    CountContext ctx = { clazz, 0 };
    dvmLockHeap();
    HeapBitmap *bitmap = dvmHeapSourceGetLiveBits();
    dvmHeapBitmapWalk(bitmap, countAssignableInstancesOfClassCallback, &ctx);
    dvmUnlockHeap();
    return ctx.count;
}

bool dvmIsHeapAddress(void *address)
{
    return address != NULL && (((uintptr_t) address & (8-1)) == 0);
}

bool dvmIsNonMovingObject(const Object* object)
{
    return true;
}
