/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_BACKEND_H_
#define ART_COMPILER_DEX_BACKEND_H_

#include "compiled_method.h"
#include "arena_allocator.h"

namespace art {

class Backend {
  public:
    virtual ~Backend() {}
    virtual void Materialize() = 0;
    virtual CompiledMethod* GetCompiledMethod() = 0;

  protected:
    explicit Backend(ArenaAllocator* arena) : arena_(arena) {}
    ArenaAllocator* const arena_;
};  // Class Backend

}  // namespace art

#endif  // ART_COMPILER_DEX_BACKEND_H_
