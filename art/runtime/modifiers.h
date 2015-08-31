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

#ifndef ART_RUNTIME_MODIFIERS_H_
#define ART_RUNTIME_MODIFIERS_H_

#include <stdint.h>

static const uint32_t kAccPublic = 0x0001;  // class, field, method, ic
static const uint32_t kAccPrivate = 0x0002;  // field, method, ic
static const uint32_t kAccProtected = 0x0004;  // field, method, ic
static const uint32_t kAccStatic = 0x0008;  // field, method, ic
static const uint32_t kAccFinal = 0x0010;  // class, field, method, ic
static const uint32_t kAccSynchronized = 0x0020;  // method (only allowed on natives)
static const uint32_t kAccSuper = 0x0020;  // class (not used in dex)
static const uint32_t kAccVolatile = 0x0040;  // field
static const uint32_t kAccBridge = 0x0040;  // method (1.5)
static const uint32_t kAccTransient = 0x0080;  // field
static const uint32_t kAccVarargs = 0x0080;  // method (1.5)
static const uint32_t kAccNative = 0x0100;  // method
static const uint32_t kAccInterface = 0x0200;  // class, ic
static const uint32_t kAccAbstract = 0x0400;  // class, method, ic
static const uint32_t kAccStrict = 0x0800;  // method
static const uint32_t kAccSynthetic = 0x1000;  // field, method, ic
static const uint32_t kAccAnnotation = 0x2000;  // class, ic (1.5)
static const uint32_t kAccEnum = 0x4000;  // class, field, ic (1.5)

static const uint32_t kAccMiranda = 0x8000;  // method

static const uint32_t kAccJavaFlagsMask = 0xffff;  // bits set from Java sources (low 16)

static const uint32_t kAccConstructor = 0x00010000;  // method (dex only) <init> and <clinit>
static const uint32_t kAccDeclaredSynchronized = 0x00020000;  // method (dex only)
static const uint32_t kAccClassIsProxy = 0x00040000;  // class (dex only)
static const uint32_t kAccPreverified = 0x00080000;  // method (dex only)

// Special runtime-only flags.
// Note: if only kAccClassIsReference is set, we have a soft reference.
static const uint32_t kAccClassIsFinalizable        = 0x80000000;  // class/ancestor overrides finalize()
static const uint32_t kAccClassIsReference          = 0x08000000;  // class is a soft/weak/phantom ref
static const uint32_t kAccClassIsWeakReference      = 0x04000000;  // class is a weak reference
static const uint32_t kAccClassIsFinalizerReference = 0x02000000;  // class is a finalizer reference
static const uint32_t kAccClassIsPhantomReference   = 0x01000000;  // class is a phantom reference

static const uint32_t kAccReferenceFlagsMask = (kAccClassIsReference
                                                | kAccClassIsWeakReference
                                                | kAccClassIsFinalizerReference
                                                | kAccClassIsPhantomReference);

#endif  // ART_RUNTIME_MODIFIERS_H_

