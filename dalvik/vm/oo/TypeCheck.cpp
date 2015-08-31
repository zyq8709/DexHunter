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
 * instanceof, checkcast, etc.
 */
#include "Dalvik.h"

#include <stdlib.h>

/*
 * I think modern C mandates that the results of a boolean expression are
 * 0 or 1.  If not, or we suddenly turn into C++ and bool != int, use this.
 */
#define BOOL_TO_INT(x)  (x)
//#define BOOL_TO_INT(x)  ((x) ? 1 : 0)

/*
 * Number of entries in instanceof cache.  MUST be a power of 2.
 */
#define INSTANCEOF_CACHE_SIZE   1024


/*
 * Allocate cache.
 */
bool dvmInstanceofStartup()
{
    gDvm.instanceofCache = dvmAllocAtomicCache(INSTANCEOF_CACHE_SIZE);
    if (gDvm.instanceofCache == NULL)
        return false;
    return true;
}

/*
 * Discard the cache.
 */
void dvmInstanceofShutdown()
{
    dvmFreeAtomicCache(gDvm.instanceofCache);
}


/*
 * Determine whether "sub" is an instance of "clazz", where both of these
 * are array classes.
 *
 * Consider an array class, e.g. Y[][], where Y is a subclass of X.
 *   Y[][] instanceof Y[][]        --> true (identity)
 *   Y[][] instanceof X[][]        --> true (element superclass)
 *   Y[][] instanceof Y            --> false
 *   Y[][] instanceof Y[]          --> false
 *   Y[][] instanceof Object       --> true (everything is an object)
 *   Y[][] instanceof Object[]     --> true
 *   Y[][] instanceof Object[][]   --> true
 *   Y[][] instanceof Object[][][] --> false (too many []s)
 *   Y[][] instanceof Serializable     --> true (all arrays are Serializable)
 *   Y[][] instanceof Serializable[]   --> true
 *   Y[][] instanceof Serializable[][] --> false (unless Y is Serializable)
 *
 * Don't forget about primitive types.
 *   int[] instanceof Object[]     --> false
 *
 * "subElemClass" is sub->elementClass.
 *
 * "subDim" is usually just sub->dim, but for some kinds of checks we want
 * to pass in a non-array class and pretend that it's an array.
 */
static int isArrayInstanceOfArray(const ClassObject* subElemClass, int subDim,
    const ClassObject* clazz)
{
    //assert(dvmIsArrayClass(sub));
    assert(dvmIsArrayClass(clazz));

    /* "If T is an array type TC[]... one of the following must be true:
     *   TC and SC are the same primitive type.
     *   TC and SC are reference types and type SC can be cast to TC [...]."
     *
     * We need the class objects for the array elements.  For speed we
     * tucked them into the class object.
     */
    assert(subDim > 0 && clazz->arrayDim > 0);
    if (subDim == clazz->arrayDim) {
        /*
         * See if "sub" is an instance of "clazz".  This handles the
         * interfaces, java.lang.Object, superclassing, etc.
         */
        return dvmInstanceof(subElemClass, clazz->elementClass);
    } else if (subDim > clazz->arrayDim) {
        /*
         * The thing we might be an instance of has fewer dimensions.  It
         * must be an Object or array of Object, or a standard array
         * interface or array of standard array interfaces (the standard
         * interfaces being java/lang/Cloneable and java/io/Serializable).
         */
        if (dvmIsInterfaceClass(clazz->elementClass)) {
            /*
             * See if the class implements its base element.  We know the
             * base element is an interface; if the array class implements
             * it, we know it's a standard array interface.
             */
            return dvmImplements(clazz, clazz->elementClass);
        } else {
            /*
             * See if this is an array of Object, Object[], etc.  We know
             * that the superclass of an array is always Object, so we
             * just compare the element type to that.
             */
            return (clazz->elementClass == clazz->super);
        }
    } else {
        /*
         * Too many []s.
         */
        return false;
    }
}

/*
 * Determine whether "sub" is a sub-class of "clazz", where "sub" is an
 * array class.
 *
 * "clazz" could be an array class, interface, or simple class.
 */
static int isArrayInstanceOf(const ClassObject* sub, const ClassObject* clazz)
{
    assert(dvmIsArrayClass(sub));

    /* "If T is an interface type, T must be one of the interfaces
     * implemented by arrays."
     *
     * I'm not checking that here, because dvmInstanceof tests for
     * interfaces first, and the generic dvmImplements stuff should
     * work correctly.
     */
    assert(!dvmIsInterfaceClass(clazz));     /* make sure */

    /* "If T is a class type, then T must be Object."
     *
     * The superclass of an array is always java.lang.Object, so just
     * compare against that.
     */
    if (!dvmIsArrayClass(clazz))
        return BOOL_TO_INT(clazz == sub->super);

    /*
     * If T is an array type TC[] ...
     */
    return isArrayInstanceOfArray(sub->elementClass, sub->arrayDim, clazz);
}


/*
 * Returns 1 (true) if "clazz" is an implementation of "interface".
 *
 * "clazz" could be a class or an interface.
 */
int dvmImplements(const ClassObject* clazz, const ClassObject* interface)
{
    int i;

    assert(dvmIsInterfaceClass(interface));

    /*
     * All interfaces implemented directly and by our superclass, and
     * recursively all super-interfaces of those interfaces, are listed
     * in "iftable", so we can just do a linear scan through that.
     */
    for (i = 0; i < clazz->iftableCount; i++) {
        if (clazz->iftable[i].clazz == interface)
            return 1;
    }

    return 0;
}

/*
 * Determine whether or not we can put an object into an array, based on
 * the class hierarchy.  The object might itself by an array, which means
 * we have to pay attention to the array instanceof rules.
 *
 * Note that "objectClass" could be an array, but objectClass->elementClass
 * is always a non-array type.
 */
bool dvmCanPutArrayElement(const ClassObject* objectClass,
    const ClassObject* arrayClass)
{
    if (dvmIsArrayClass(objectClass)) {
        /*
         * We're stuffing an array into an array.  We want to see if the
         * elements of "arrayClass" are compatible with "objectClass".
         * We bump up the number of dimensions in "objectClass" so that we
         * can compare the two directly.
         */
        return isArrayInstanceOfArray(objectClass->elementClass,
                    objectClass->arrayDim + 1, arrayClass);
    } else {
        /*
         * We're putting a non-array element into an array.  We need to
         * test to see if the elements are compatible.  The easiest way
         * to do that is to "arrayify" it and use the standard array
         * compatibility check.
         */
        return isArrayInstanceOfArray(objectClass, 1, arrayClass);
    }
}


/*
 * Perform the instanceof calculation.
 */
static inline int isInstanceof(const ClassObject* instance,
    const ClassObject* clazz)
{
    if (dvmIsInterfaceClass(clazz)) {
        return dvmImplements(instance, clazz);
    } else if (dvmIsArrayClass(instance)) {
        return isArrayInstanceOf(instance, clazz);
    } else {
        return dvmIsSubClass(instance, clazz);
    }
}


/*
 * Do the instanceof calculation, pulling the result from the cache if
 * possible.
 */
int dvmInstanceofNonTrivial(const ClassObject* instance,
    const ClassObject* clazz)
{
#define ATOMIC_CACHE_CALC isInstanceof(instance, clazz)
#define ATOMIC_CACHE_NULL_ALLOWED true
    return ATOMIC_CACHE_LOOKUP(gDvm.instanceofCache,
                INSTANCEOF_CACHE_SIZE, instance, clazz);
#undef ATOMIC_CACHE_CALC
}
