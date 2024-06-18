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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_MUTABLE_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_MUTABLE_LIST_IMPL_H_

#include <string>

#include "absl/status/status.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/casts.h"

namespace cel::runtime_internal {

constexpr char kMutableListTypeName[] = "#cel.MutableList";

// Runtime internal value type representing a list that is built from a
// comprehension.
// This should only be used as an optimization for the builtin comprehensions
// map and filter.
// After the comprehension finishes, this is normalized into a standard list
// value via the Build function.
class MutableListValue final : public cel::OpaqueValueInterface {
 public:
  static bool Is(const Value& value) {
    return InstanceOf<OpaqueValue>(value) &&
           NativeTypeId::Of(value) == NativeTypeId::For<MutableListValue>();
  }

  static MutableListValue& Cast(Value& value) {
    return const_cast<MutableListValue&>(
        cel::internal::down_cast<const MutableListValue&>(
            *cel::Cast<OpaqueValue>(value)));
  }

  static MutableListValue& Cast(OpaqueValue& value) {
    return const_cast<MutableListValue&>(
        cel::internal::down_cast<const MutableListValue&>(*value));
  }

  explicit MutableListValue(cel::Unique<cel::ListValueBuilder> list_builder);

  absl::string_view GetTypeName() const override {
    return kMutableListTypeName;
  }

  absl::Status Equal(ValueManager&, ValueView,
                     cel::Value& result) const override {
    result = BoolValueView{false};
    return absl::OkStatus();
  }

  // Add an element to this list.
  // Caller must validate that mutating this object is safe.
  absl::Status Append(cel::Value element);

  // Build a list value from this object.
  // The instance is no longer usable after the call to Build.
  // Caller must clean up any handles still referring to this object.
  absl::StatusOr<cel::ListValue> Build() &&;

  std::string DebugString() const override;

 private:
  Type GetTypeImpl(TypeManager& type_manager) const override;

  cel::NativeTypeId GetNativeTypeId() const override;

  cel::Unique<cel::ListValueBuilder> list_builder_;
};

}  //  namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_MUTABLE_LIST_IMPL_H_
