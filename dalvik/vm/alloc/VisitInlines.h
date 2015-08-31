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

#ifndef DALVIK_ALLOC_VISITINLINES_H_
#define DALVIK_ALLOC_VISITINLINES_H_

/*
 * Visits the instance fields of a class or data object.
 */
static void visitFields(Visitor *visitor, Object *obj, void *arg)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    if (obj->clazz->refOffsets != CLASS_WALK_SUPER) {
        size_t refOffsets = obj->clazz->refOffsets;
        while (refOffsets != 0) {
            size_t rshift = CLZ(refOffsets);
            size_t offset = CLASS_OFFSET_FROM_CLZ(rshift);
            Object **ref = (Object **)BYTE_OFFSET(obj, offset);
            (*visitor)(ref, arg);
            refOffsets &= ~(CLASS_HIGH_BIT >> rshift);
        }
    } else {
        for (ClassObject *clazz = obj->clazz;
             clazz != NULL;
             clazz = clazz->super) {
            InstField *field = clazz->ifields;
            for (int i = 0; i < clazz->ifieldRefCount; ++i, ++field) {
                size_t offset = field->byteOffset;
                Object **ref = (Object **)BYTE_OFFSET(obj, offset);
                (*visitor)(ref, arg);
            }
        }
    }
}

/*
 * Visits the static fields of a class object.
 */
static void visitStaticFields(Visitor *visitor, ClassObject *clazz,
                              void *arg)
{
    assert(visitor != NULL);
    assert(clazz != NULL);
    for (int i = 0; i < clazz->sfieldCount; ++i) {
        char ch = clazz->sfields[i].signature[0];
        if (ch == '[' || ch == 'L') {
            (*visitor)(&clazz->sfields[i].value.l, arg);
        }
    }
}

/*
 * Visit the interfaces of a class object.
 */
static void visitInterfaces(Visitor *visitor, ClassObject *clazz,
                            void *arg)
{
    assert(visitor != NULL);
    assert(clazz != NULL);
    for (int i = 0; i < clazz->interfaceCount; ++i) {
        (*visitor)(&clazz->interfaces[i], arg);
    }
}

/*
 * Visits all the references stored in a class object instance.
 */
static void visitClassObject(Visitor *visitor, Object *obj, void *arg)
{
    ClassObject *asClass;

    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    assert(!strcmp(obj->clazz->descriptor, "Ljava/lang/Class;"));
    (*visitor)(&obj->clazz, arg);
    asClass = (ClassObject *)obj;
    if (IS_CLASS_FLAG_SET(asClass, CLASS_ISARRAY)) {
        (*visitor)(&asClass->elementClass, arg);
    }
    if (asClass->status > CLASS_IDX) {
        (*visitor)(&asClass->super, arg);
    }
    (*visitor)(&asClass->classLoader, arg);
    visitFields(visitor, obj, arg);
    visitStaticFields(visitor, asClass, arg);
    if (asClass->status > CLASS_IDX) {
      visitInterfaces(visitor, asClass, arg);
    }
}

/*
 * Visits the class object and, if the array is typed as an object
 * array, all of the array elements.
 */
static void visitArrayObject(Visitor *visitor, Object *obj, void *arg)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    (*visitor)(&obj->clazz, arg);
    if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISOBJECTARRAY)) {
        ArrayObject *array = (ArrayObject *)obj;
        Object **contents = (Object **)(void *)array->contents;
        for (size_t i = 0; i < array->length; ++i) {
            (*visitor)(&contents[i], arg);
        }
    }
}

/*
 * Visits the class object and reference typed instance fields of a
 * data object.
 */
static void visitDataObject(Visitor *visitor, Object *obj, void *arg)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    (*visitor)(&obj->clazz, arg);
    visitFields(visitor, obj, arg);
}

/*
 * Like visitDataObject, but visits the hidden referent field that
 * belongings to the subclasses of java.lang.Reference.
 */
static void visitReferenceObject(Visitor *visitor, Object *obj, void *arg)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    visitDataObject(visitor, obj, arg);
    size_t offset = gDvm.offJavaLangRefReference_referent;
    Object **ref = (Object **)BYTE_OFFSET(obj, offset);
    (*visitor)(ref, arg);
}

/*
 * Visits all of the reference stored in an object.
 */
static void visitObject(Visitor *visitor, Object *obj, void *arg)
{
    assert(visitor != NULL);
    assert(obj != NULL);
    assert(obj->clazz != NULL);
    if (dvmIsClassObject(obj)) {
        visitClassObject(visitor, obj, arg);
    } else if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISARRAY)) {
        visitArrayObject(visitor, obj, arg);
    } else if (IS_CLASS_FLAG_SET(obj->clazz, CLASS_ISREFERENCE)) {
        visitReferenceObject(visitor, obj, arg);
    } else {
        visitDataObject(visitor, obj, arg);
    }
}

#endif  // DALVIK_ALLOC_VISITINLINES_H_
