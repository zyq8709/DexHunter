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

#include "stringpiece.h"

#include <iostream>
#include <utility>

namespace art {

void StringPiece::CopyToString(std::string* target) const {
  target->assign(ptr_, length_);
}

int StringPiece::copy(char* buf, size_type n, size_type pos) const {
  int ret = std::min(length_ - pos, n);
  memcpy(buf, ptr_ + pos, ret);
  return ret;
}

StringPiece::size_type StringPiece::find(const StringPiece& s, size_type pos) const {
  if (length_ < 0 || pos > static_cast<size_type>(length_))
    return npos;

  const char* result = std::search(ptr_ + pos, ptr_ + length_,
                                   s.ptr_, s.ptr_ + s.length_);
  const size_type xpos = result - ptr_;
  return xpos + s.length_ <= static_cast<size_type>(length_) ? xpos : npos;
}

int StringPiece::compare(const StringPiece& x) const {
  int r = memcmp(ptr_, x.ptr_, std::min(length_, x.length_));
  if (r == 0) {
    if (length_ < x.length_) r = -1;
    else if (length_ > x.length_) r = +1;
  }
  return r;
}

StringPiece::size_type StringPiece::find(char c, size_type pos) const {
  if (length_ <= 0 || pos >= static_cast<size_type>(length_)) {
    return npos;
  }
  const char* result = std::find(ptr_ + pos, ptr_ + length_, c);
  return result != ptr_ + length_ ? result - ptr_ : npos;
}

StringPiece::size_type StringPiece::rfind(const StringPiece& s, size_type pos) const {
  if (length_ < s.length_) return npos;
  const size_t ulen = length_;
  if (s.length_ == 0) return std::min(ulen, pos);

  const char* last = ptr_ + std::min(ulen - s.length_, pos) + s.length_;
  const char* result = std::find_end(ptr_, last, s.ptr_, s.ptr_ + s.length_);
  return result != last ? result - ptr_ : npos;
}

StringPiece::size_type StringPiece::rfind(char c, size_type pos) const {
  if (length_ <= 0) return npos;
  for (int i = std::min(pos, static_cast<size_type>(length_ - 1));
       i >= 0; --i) {
    if (ptr_[i] == c) {
      return i;
    }
  }
  return npos;
}

StringPiece StringPiece::substr(size_type pos, size_type n) const {
  if (pos > static_cast<size_type>(length_)) pos = length_;
  if (n > length_ - pos) n = length_ - pos;
  return StringPiece(ptr_ + pos, n);
}

const StringPiece::size_type StringPiece::npos = size_type(-1);

std::ostream& operator<<(std::ostream& o, const StringPiece& piece) {
  o.write(piece.data(), piece.size());
  return o;
}

}  // namespace art
