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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_INTEROP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_INTEROP_H_

#include <functional>
#include <memory>
#include <string>

#include "google/protobuf/arena.h"
#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/variant.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "eval/public/cel_value.h"
#include "eval/public/message_wrapper.h"

namespace cel::interop_internal {

struct CelListAccess final {
  static internal::TypeInfo TypeId(
      const google::api::expr::runtime::CelList& list);
};

struct CelMapAccess final {
  static internal::TypeInfo TypeId(
      const google::api::expr::runtime::CelMap& map);
};

struct LegacyStructTypeAccess final {
  static Handle<StructType> Create(uintptr_t message);
};

struct LegacyStructValueAccess final {
  static Handle<StructValue> Create(
      const google::api::expr::runtime::MessageWrapper& wrapper);
  static Handle<StructValue> Create(uintptr_t message, uintptr_t type_info);
  static uintptr_t Message(const base_internal::LegacyStructValue& value);
  static uintptr_t TypeInfo(const base_internal::LegacyStructValue& value);
  static google::api::expr::runtime::MessageWrapper ToMessageWrapper(
      const base_internal::LegacyStructValue& value);
};

struct MessageWrapperAccess final {
  static uintptr_t Message(
      const google::api::expr::runtime::MessageWrapper& wrapper);
  static uintptr_t TypeInfo(
      const google::api::expr::runtime::MessageWrapper& wrapper);
  static google::api::expr::runtime::MessageWrapper Make(uintptr_t message,
                                                         uintptr_t type_info);
  static google::api::expr::runtime::MessageWrapper::Builder ToBuilder(
      google::api::expr::runtime::MessageWrapper& wrapper);
};

// Unlike ValueFactory::CreateStringValue, this does not copy input and instead
// wraps it. It should only be used for interop with the legacy CelValue.
Handle<StringValue> CreateStringValueFromView(absl::string_view value);

// Unlike ValueFactory::CreateBytesValue, this does not copy input and instead
// wraps it. It should only be used for interop with the legacy CelValue.
Handle<BytesValue> CreateBytesValueFromView(absl::string_view value);

base_internal::StringValueRep GetStringValueRep(
    const Handle<StringValue>& value);

base_internal::BytesValueRep GetBytesValueRep(const Handle<BytesValue>& value);

// Converts a legacy CEL value to the new CEL value representation.
absl::StatusOr<Handle<Value>> FromLegacyValue(
    google::protobuf::Arena* arena,
    const google::api::expr::runtime::CelValue& legacy_value);

// Converts a new CEL value to the legacy CEL value representation.
absl::StatusOr<google::api::expr::runtime::CelValue> ToLegacyValue(
    google::protobuf::Arena* arena, const Handle<Value>& value);

Handle<NullValue> CreateNullValue();

Handle<BoolValue> CreateBoolValue(bool value);

Handle<IntValue> CreateIntValue(int64_t value);

Handle<UintValue> CreateUintValue(uint64_t value);

Handle<DoubleValue> CreateDoubleValue(double value);

// Create a modern string value, without validation or copying. Should only be
// used during interoperation.
Handle<StringValue> CreateStringValueFromView(absl::string_view value);

// Create a modern bytes value, without validation or copying. Should only be
// used during interoperation.
Handle<BytesValue> CreateBytesValueFromView(absl::string_view value);

// Create a modern duration value, without validation. Should only be used
// during interoperation.
Handle<DurationValue> CreateDurationValue(absl::Duration value);

// Create a modern timestamp value, without validation. Should only be used
// during interoperation.
Handle<TimestampValue> CreateTimestampValue(absl::Time value);

Handle<ErrorValue> CreateErrorValueFromView(const absl::Status* value);

Handle<UnknownValue> CreateUnknownValueFromView(
    const base_internal::UnknownSet* value);

// Convert a legacy value to a modern value, CHECK failing if its not possible.
// This should only be used during rewritting of the evaluator when it is
// guaranteed that all modern and legacy values are interoperable, and the
// memory manager is google::protobuf::Arena.
Handle<Value> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena, const google::api::expr::runtime::CelValue& value);
Handle<Value> LegacyValueToModernValueOrDie(
    MemoryManager& memory_manager,
    const google::api::expr::runtime::CelValue& value);

// Convert a modern value to a legacy value, CHECK failing if its not possible.
// This should only be used during rewritting of the evaluator when it is
// guaranteed that all modern and legacy values are interoperable, and the
// memory manager is google::protobuf::Arena.
google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, const Handle<Value>& value);
google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    MemoryManager& memory_manager, const Handle<Value>& value);

Handle<TypeValue> CreateTypeValueFromView(absl::string_view input);

}  // namespace cel::interop_internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_INTEROP_H_
