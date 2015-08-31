/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include "Dalvik.h"
#include "alloc/HeapBitmap.h"
#include "alloc/HeapSource.h"
#include "alloc/Verify.h"
#include "alloc/Visit.h"

/*
 * Visitor applied to each reference field when searching for things
 * that point to an object.  Sets the argument to NULL when a match is
 * found.
 */
static void dumpReferencesVisitor(void *pObj, void *arg)
{
    Object *obj = *(Object **)pObj;
    Object *lookingFor = *(Object **)arg;
    if (lookingFor != NULL && lookingFor == obj) {
        *(Object **)arg = NULL;
    }
}

/*
 * Visitor applied to each bitmap element to search for things that
 * point to an object.  Logs a message when a match is found.
 */
static void dumpReferencesCallback(Object *obj, void *arg)
{
    if (obj == (Object *)arg) {
        return;
    }
    dvmVisitObject(dumpReferencesVisitor, obj, &arg);
    if (arg == NULL) {
        ALOGD("Found %p in the heap @ %p", arg, obj);
        dvmDumpObject(obj);
    }
}

/*
 * Visitor applied to each root to search for things that point to an
 * object.  Logs a message when a match is found.
 */
static void dumpReferencesRootVisitor(void *ptr, u4 threadId,
                                      RootType type, void *arg)
{
    Object *obj = *(Object **)ptr;
    Object *lookingFor = *(Object **)arg;
    if (obj == lookingFor) {
        ALOGD("Found %p in a root @ %p", arg, ptr);
    }
}

/*
 * Searches the roots and heap for object references.
 */
static void dumpReferences(const Object *obj)
{
    HeapBitmap *bitmap = dvmHeapSourceGetLiveBits();
    void *arg = (void *)obj;
    dvmVisitRoots(dumpReferencesRootVisitor, arg);
    dvmHeapBitmapWalk(bitmap, dumpReferencesCallback, arg);
}

/*
 * Checks that the given reference points to a valid object.
 */
static void verifyReference(void *addr, void *arg)
{
    Object *obj;
    bool isValid;

    assert(addr != NULL);
    obj = *(Object **)addr;
    if (obj == NULL) {
        isValid = true;
    } else {
        isValid = dvmIsValidObject(obj);
    }
    if (!isValid) {
        Object **parent = (Object **)arg;
        if (*parent != NULL) {
            ALOGE("Verify of object %p failed", *parent);
            dvmDumpObject(*parent);
            *parent = NULL;
        }
        ALOGE("Verify of reference %p @ %p failed", obj, addr);
        dvmDumpObject(obj);
    }
}

/*
 * Verifies an object reference.
 */
void dvmVerifyObject(const Object *obj)
{
    Object *arg = const_cast<Object*>(obj);
    dvmVisitObject(verifyReference, arg, &arg);
    if (arg == NULL) {
        dumpReferences(obj);
        dvmAbort();
    }
}

/*
 * Helper function to call dvmVerifyObject from a bitmap walker.
 */
static void verifyBitmapCallback(Object *obj, void *arg)
{
    dvmVerifyObject(obj);
}

/*
 * Verifies the object references in a heap bitmap. Assumes the VM is
 * suspended.
 */
void dvmVerifyBitmap(const HeapBitmap *bitmap)
{
    dvmHeapBitmapWalk(bitmap, verifyBitmapCallback, NULL);
}

/*
 * Helper function to call verifyReference from the root verifier.
 */
static void verifyRootReference(void *addr, u4 threadId,
                                RootType type, void *arg)
{
    verifyReference(addr, arg);
}

/*
 * Verifies references in the roots.
 */
void dvmVerifyRoots()
{
    dvmVisitRoots(verifyRootReference, NULL);
}
