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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_REFERENCE_COUNTED_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_REFERENCE_COUNTED_H_

#include <atomic>
#include <cstddef>
#include <type_traits>

#include "absl/base/macros.h"

namespace cel::internal {

class ReferenceCounted;

void Ref(const ReferenceCounted& refcnt);
void Unref(const ReferenceCounted& refcnt);

// To make life easier, we return the passed pointer so it can be used inline in
// places like constructors. To ensure this is only be used as intended, we use
// SFINAE.
template <typename T>
std::enable_if_t<std::is_base_of_v<ReferenceCounted, T>, T*> Ref(T* refcnt);

void Unref(const ReferenceCounted* refcnt);

class ReferenceCounted {
 public:
  ReferenceCounted(const ReferenceCounted&) = delete;

  ReferenceCounted(ReferenceCounted&&) = delete;

  virtual ~ReferenceCounted() = default;

  ReferenceCounted& operator=(const ReferenceCounted&) = delete;

  ReferenceCounted& operator=(ReferenceCounted&&) = delete;

 protected:
  constexpr ReferenceCounted() : refs_(1) {}

 private:
  friend void Ref(const ReferenceCounted& refcnt);
  friend void Unref(const ReferenceCounted& refcnt);
  template <typename T>
  friend std::enable_if_t<std::is_base_of_v<ReferenceCounted, T>, T*> Ref(
      T* refcnt);
  friend void Unref(const ReferenceCounted* refcnt);

  void Ref() const {
    const auto refs = refs_.fetch_add(1, std::memory_order_relaxed);
    ABSL_ASSERT(refs >= 1);
  }

  void Unref() const {
    const auto refs = refs_.fetch_sub(1, std::memory_order_acq_rel);
    ABSL_ASSERT(refs >= 1);
    if (refs == 1) {
      delete this;
    }
  }

  mutable std::atomic<long> refs_;  // NOLINT
};

inline void Ref(const ReferenceCounted& refcnt) { refcnt.Ref(); }

inline void Unref(const ReferenceCounted& refcnt) { refcnt.Unref(); }

template <typename T>
inline std::enable_if_t<std::is_base_of_v<ReferenceCounted, T>, T*> Ref(
    T* refcnt) {
  if (refcnt != nullptr) {
    (Ref)(*refcnt);
  }
  return refcnt;
}

inline void Unref(const ReferenceCounted* refcnt) {
  if (refcnt != nullptr) {
    (Unref)(*refcnt);
  }
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_REFERENCE_COUNTED_H_
