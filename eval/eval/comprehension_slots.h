// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_SLOTS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_SLOTS_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "internal/new.h"

namespace google::api::expr::runtime {

class ComprehensionSlot final {
 public:
  ComprehensionSlot() = default;
  ComprehensionSlot(const ComprehensionSlot&) = delete;
  ComprehensionSlot(ComprehensionSlot&&) = delete;
  ComprehensionSlot& operator=(const ComprehensionSlot&) = delete;
  ComprehensionSlot& operator=(ComprehensionSlot&&) = delete;

  const cel::Value& value() const {
    ABSL_DCHECK(Has());

    return value_;
  }

  absl::Nonnull<cel::Value*> mutable_value() {
    ABSL_DCHECK(Has());

    return &value_;
  }

  const AttributeTrail& attribute() const {
    ABSL_DCHECK(Has());

    return attribute_;
  }

  absl::Nonnull<AttributeTrail*> mutable_attribute() {
    ABSL_DCHECK(Has());

    return &attribute_;
  }

  bool Has() const { return has_; }

  void Set() { Set(cel::NullValue(), absl::nullopt); }

  template <typename V>
  void Set(V&& value) {
    Set(std::forward<V>(value), absl::nullopt);
  }

  template <typename V, typename A>
  void Set(V&& value, A&& attribute) {
    value_ = std::forward<V>(value);
    attribute_ = std::forward<A>(attribute);
    has_ = true;
  }

  void Clear() {
    if (has_) {
      value_ = cel::NullValue();
      attribute_ = absl::nullopt;
      has_ = false;
    }
  }

 private:
  cel::Value value_;
  AttributeTrail attribute_;
  bool has_ = false;
};

// Simple manager for comprehension variables.
//
// At plan time, each comprehension variable is assigned a slot by index.
// This is used instead of looking up the variable identifier by name in a
// runtime stack.
//
// Callers must handle range checking.
class ComprehensionSlots final {
 public:
  using Slot = ComprehensionSlot;

  // Trivial instance if no slots are needed.
  // Trivially thread safe since no effective state.
  static ComprehensionSlots& GetEmptyInstance() {
    static absl::NoDestructor<ComprehensionSlots> instance(0);
    return *instance;
  }

  explicit ComprehensionSlots(size_t size)
      : slots_(reinterpret_cast<ComprehensionSlot*>(
            cel::internal::New(sizeof(ComprehensionSlot) * size))),
        size_(size) {
    std::uninitialized_value_construct_n(slots_, size_);
  }

  ComprehensionSlots(const ComprehensionSlots&) = delete;
  ComprehensionSlots& operator=(const ComprehensionSlots&) = delete;

  ~ComprehensionSlots() {
    std::destroy_n(slots_, size_);
    cel::internal::SizedDelete(slots_, sizeof(ComprehensionSlot) * size_);
  }

  ComprehensionSlots(ComprehensionSlots&&) = delete;
  ComprehensionSlots& operator=(ComprehensionSlots&&) = delete;

  // Return ptr to slot at index.
  // If not set, returns nullptr.
  absl::Nonnull<Slot*> Get(size_t index) {
    ABSL_DCHECK_LT(index, size_);

    return &slots_[index];
  }

  void Reset() {
    for (size_t i = 0; i < size_; ++i) {
      slots_[i].Clear();
    }
  }

  void ClearSlot(size_t index) { Get(index)->Clear(); }

  template <typename V>
  void Set(size_t index, V&& value) {
    Set(index, std::forward<V>(value), absl::nullopt);
  }

  template <typename V, typename A>
  void Set(size_t index, V&& value, A&& attribute) {
    Get(index)->Set(std::forward<V>(value), std::forward<A>(attribute));
  }

  size_t size() const { return size_; }

 private:
  absl::NullabilityUnknown<ComprehensionSlot*> slots_ = nullptr;
  size_t size_ = 0;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_SLOTS_H_
