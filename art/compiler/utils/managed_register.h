/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_COMPILER_UTILS_MANAGED_REGISTER_H_
#define ART_COMPILER_UTILS_MANAGED_REGISTER_H_

namespace art {

namespace arm {
class ArmManagedRegister;
}
namespace mips {
class MipsManagedRegister;
}
namespace x86 {
class X86ManagedRegister;
}

class ManagedRegister {
 public:
  // ManagedRegister is a value class. There exists no method to change the
  // internal state. We therefore allow a copy constructor and an
  // assignment-operator.
  ManagedRegister(const ManagedRegister& other) : id_(other.id_) { }

  ManagedRegister& operator=(const ManagedRegister& other) {
    id_ = other.id_;
    return *this;
  }

  arm::ArmManagedRegister AsArm() const;
  mips::MipsManagedRegister AsMips() const;
  x86::X86ManagedRegister AsX86() const;

  // It is valid to invoke Equals on and with a NoRegister.
  bool Equals(const ManagedRegister& other) const {
    return id_ == other.id_;
  }

  bool IsNoRegister() const {
    return id_ == kNoRegister;
  }

  static ManagedRegister NoRegister() {
    return ManagedRegister();
  }

 protected:
  static const int kNoRegister = -1;

  ManagedRegister() : id_(kNoRegister) { }
  explicit ManagedRegister(int reg_id) : id_(reg_id) { }

  int id_;
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_MANAGED_REGISTER_H_
