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

#include "space.h"

#include "base/logging.h"

namespace art {
namespace gc {
namespace space {

Space::Space(const std::string& name, GcRetentionPolicy gc_retention_policy)
    : name_(name), gc_retention_policy_(gc_retention_policy) { }

void Space::Dump(std::ostream& os) const {
  os << GetName() << ":" << GetGcRetentionPolicy();
}

std::ostream& operator<<(std::ostream& os, const Space& space) {
  space.Dump(os);
  return os;
}


DiscontinuousSpace::DiscontinuousSpace(const std::string& name,
                                       GcRetentionPolicy gc_retention_policy) :
    Space(name, gc_retention_policy),
    live_objects_(new accounting::SpaceSetMap("large live objects")),
    mark_objects_(new accounting::SpaceSetMap("large marked objects")) {
}

}  // namespace space
}  // namespace gc
}  // namespace art
