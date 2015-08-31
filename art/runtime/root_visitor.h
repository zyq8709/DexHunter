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

#ifndef ART_RUNTIME_ROOT_VISITOR_H_
#define ART_RUNTIME_ROOT_VISITOR_H_

namespace art {
namespace mirror {
class Object;
}  // namespace mirror
class StackVisitor;

typedef void (RootVisitor)(const mirror::Object* root, void* arg);
typedef void (VerifyRootVisitor)(const mirror::Object* root, void* arg, size_t vreg,
                                 const StackVisitor* visitor);
typedef bool (IsMarkedTester)(const mirror::Object* object, void* arg);

}  // namespace art

#endif  // ART_RUNTIME_ROOT_VISITOR_H_
