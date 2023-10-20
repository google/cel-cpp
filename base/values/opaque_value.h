// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_OPAQUE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_OPAQUE_VALUE_H_

#include <string>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/types/opaque_type.h"
#include "base/value.h"
#include "common/any.h"
#include "common/json.h"
#include "common/native_type.h"

namespace cel {

class OpaqueValue : public Value,
                    public base_internal::HeapData,
                    public base_internal::EnableHandleFromThis<OpaqueValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kOpaque;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using Value::Is;

  static const OpaqueValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->DebugString()
                           << " to opaque";
    return static_cast<const OpaqueValue&>(value);
  }

  constexpr ValueKind kind() const { return kKind; }

  const Handle<OpaqueType>& type() const { return type_; }

  virtual std::string DebugString() const = 0;

  virtual absl::StatusOr<Any> ConvertToAny(ValueFactory& value_factory) const;

  virtual absl::StatusOr<Json> ConvertToJson(ValueFactory& value_factory) const;

  absl::StatusOr<Handle<Value>> ConvertToType(ValueFactory& value_factory,
                                              const Handle<Type>& type) const;

  virtual absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                               const Value& other) const;

 protected:
  static NativeTypeId TypeId(const OpaqueValue& value) {
    return value.TypeId();
  }

  explicit OpaqueValue(Handle<OpaqueType> type)
      : Value(), HeapData(kKind), type_(std::move(type)) {}

 private:
  virtual NativeTypeId TypeId() const = 0;

  const Handle<OpaqueType> type_;
};

extern template class Handle<OpaqueValue>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_OPAQUE_VALUE_H_
