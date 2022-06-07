// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_RTTI_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_RTTI_H_

#include <cstdint>
#include <utility>

namespace cel::internal {

class TypeInfo;

template <typename T>
TypeInfo TypeId();

// TypeInfo is an RTTI-like alternative for identifying a type at runtime. Its
// main benefit is it does not require RTTI being available, allowing CEL to
// work without RTTI.
//
// This is used to implement the runtime type system and conversion between CEL
// values and their native C++ counterparts.
class TypeInfo final {
 public:
  constexpr TypeInfo() = default;

  TypeInfo(const TypeInfo&) = default;

  TypeInfo& operator=(const TypeInfo&) = default;

  friend bool operator==(const TypeInfo& lhs, const TypeInfo& rhs) {
    return lhs.id_ == rhs.id_;
  }

  friend bool operator!=(const TypeInfo& lhs, const TypeInfo& rhs) {
    return !operator==(lhs, rhs);
  }

  template <typename H>
  friend H AbslHashValue(H state, const TypeInfo& type) {
    return H::combine(std::move(state), reinterpret_cast<uintptr_t>(type.id_));
  }

 private:
  template <typename T>
  friend TypeInfo TypeId();

  constexpr explicit TypeInfo(void* id) : id_(id) {}

  void* id_ = nullptr;
};

template <typename T>
TypeInfo TypeId() {
  // Adapted from Abseil and GTL. I believe this not being const is to ensure
  // the compiler does not merge multiple constants with the same value to share
  // the same address.
  static char id;
  return TypeInfo(&id);
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_RTTI_H_
