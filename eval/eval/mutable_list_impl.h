/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONCAT_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONCAT_LIST_IMPL_H_

#include <string>
#include <vector>

#include "base/handle.h"
#include "base/memory.h"
#include "base/types/opaque_type.h"
#include "base/values/list_value_builder.h"
#include "base/values/opaque_value.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

constexpr char kMutableListTypeName[] = "#cel.MutableList";

// Runtime internal type representing a list that is built from a comprehension.
//
// This is implemented as an Opaque since it should be used solely to
// optimize comprehensions that build list values (map, and filter) -- these
// values should never be accessed as a CEL list directly. When the
// comprehension completes, the evaluator should call Build on the value type
// and return the resulting immutable list.
class MutableListType : public cel::OpaqueType {
 public:
  static bool Is(const cel::Type& type);

  using OpaqueType::Is;

  static const MutableListType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.DebugString()
                          << " to MutableList";
    return static_cast<const MutableListType&>(type);
  }

  absl::string_view name() const override { return kMutableListTypeName; }

  std::string DebugString() const override { return std::string(name()); }

  absl::Span<const cel::Handle<cel::Type>> parameters() const override {
    return {};
  }

 private:
  // Called by Is() to perform type checking
  cel::internal::TypeInfo TypeId() const override;
};

// Runtime internal value type representing a list that is built from a
// comprehension.
// This should only be used as an optimization for the builtin comprehensions
// map and filter.
// After the comprehension finishes, this is normalized into a standard list
// value via the Build function.
class MutableListValue : public cel::OpaqueValue {
 public:
  MutableListValue(cel::Handle<MutableListType> type,
                   cel::UniqueRef<cel::ListValueBuilderInterface> list_builder);

  static bool Is(const cel::Value& value);

  using OpaqueValue::Is;

  static const MutableListValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->DebugString()
                           << " to MutableList";
    return static_cast<const MutableListValue&>(value);
  }

  // Add an element to this list.
  // Caller must validate that mutating this object is safe.
  absl::Status Append(cel::Handle<cel::Value> element);

  // Build a list value from this object.
  // The instance is no longer usable after the call to Build.
  // Caller must clean up any handles still referring to this object.
  absl::StatusOr<cel::Handle<cel::ListValue>> Build() &&;

  std::string DebugString() const override;

 private:
  cel::internal::TypeInfo TypeId() const override;

  cel::UniqueRef<cel::ListValueBuilderInterface> list_builder_;
};

// Mutable CelList implementation intended to be used in the accumulation of
// a list within a comprehension loop.
//
// This value should only ever be used as an intermediate result from CEL and
// not within user code.
class MutableListImpl : public CelList {
 public:
  // Create a list from an initial vector of CelValues.
  explicit MutableListImpl(std::vector<CelValue> values)
      : values_(std::move(values)) {}

  // List size.
  int size() const override { return values_.size(); }

  // Append a single element to the list.
  void Append(const CelValue& element) { values_.push_back(element); }

  // List element access operator.
  CelValue operator[](int index) const override { return values_[index]; }

 private:
  std::vector<CelValue> values_;
};

}  //  namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONCAT_LIST_IMPL_H_
