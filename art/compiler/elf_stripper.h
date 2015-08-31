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

#ifndef ART_COMPILER_ELF_STRIPPER_H_
#define ART_COMPILER_ELF_STRIPPER_H_

#include "base/macros.h"
#include "os.h"

namespace art {

class ElfStripper {
 public:
  // Strip an ELF file of unneeded debugging information.
  // Returns true on success, false on failure.
  static bool Strip(File* file);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ElfStripper);
};

}  // namespace art

#endif  // ART_COMPILER_ELF_STRIPPER_H_
