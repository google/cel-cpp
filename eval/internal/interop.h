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
#include <vector>

#include "google/protobuf/arena.h"
#include "absl/base/nullability.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "base/value_manager.h"
#include "base/values/struct_value_builder.h"
#include "base/values/type_value.h"
#include "common/native_type.h"
#include "eval/public/cel_value.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/legacy_type_info_apis.h"

namespace cel::interop_internal {

// Implementation of cel::StructType that adapts modern APIs from legacy APIs.
//
// This implements a subset of type APIs needed for describing the type for
// extensions. Field iteration is unsupported (return
// absl::StatusCode::kUnimplemented or default values).
//
// This implementation is distinct from LegacyStructType (which supports
// conversion to/from legacy type values without additional allocations).
class LegacyAbstractStructType : public base_internal::AbstractStructType {
 public:
  // The provided LegacyTypeInfoApis reference must outlive any references to
  // the constructed type instance. Normally, it should be owned by the
  // type registry backing the expression builder.
  explicit LegacyAbstractStructType(
      const google::api::expr::runtime::LegacyTypeInfoApis& type_info)
      : type_info_(type_info) {}

  absl::string_view name() const override;

  size_t field_count() const override;

  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const override;

  // Called by FindField.
  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager, int64_t number) const override;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<FieldIterator>>>
  NewFieldIterator(TypeManager& type_manager) const override;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<StructValueBuilderInterface>>>
  NewValueBuilder(
      ValueManager& value_factory ABSL_ATTRIBUTE_LIFETIME_BOUND) const override;

  absl::StatusOr<Handle<StructValue>> NewValueFromAny(
      ValueManager& value_factory, const absl::Cord& value) const override;

 private:
  NativeTypeId GetNativeTypeId() const override {
    return cel::NativeTypeId::For<LegacyAbstractStructType>();
  }

  const google::api::expr::runtime::LegacyTypeInfoApis& type_info_;
};

struct CelListAccess final {
  static NativeTypeId TypeId(const google::api::expr::runtime::CelList& list);
};

struct CelMapAccess final {
  static NativeTypeId TypeId(const google::api::expr::runtime::CelMap& map);
};

struct LegacyStructTypeAccess final {
  static Handle<StructType> Create(uintptr_t message);
  static uintptr_t GetStorage(const base_internal::LegacyStructType& type);
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

Handle<StructType> CreateStructTypeFromLegacyTypeInfo(
    const google::api::expr::runtime::LegacyTypeInfoApis* type_info);

// Nullptr is returned if this is not a legacy message type.
const google::api::expr::runtime::LegacyTypeInfoApis* LegacyTypeInfoFromType(
    const Handle<Type>& type);

// Unlike ValueManager::CreateStringValue, this does not copy input and instead
// wraps it. It should only be used for interop with the legacy CelValue.
Handle<StringValue> CreateStringValueFromView(absl::string_view value);

// Unlike ValueManager::CreateBytesValue, this does not copy input and instead
// wraps it. It should only be used for interop with the legacy CelValue.
Handle<BytesValue> CreateBytesValueFromView(absl::string_view value);

base_internal::StringValueRep GetStringValueRep(
    const Handle<StringValue>& value);

base_internal::BytesValueRep GetBytesValueRep(const Handle<BytesValue>& value);

// Converts a legacy CEL value to the new CEL value representation.
absl::StatusOr<Handle<Value>> FromLegacyValue(
    google::protobuf::Arena* arena,
    const google::api::expr::runtime::CelValue& legacy_value,
    bool unchecked = false);

// Converts a new CEL value to the legacy CEL value representation.
absl::StatusOr<google::api::expr::runtime::CelValue> ToLegacyValue(
    google::protobuf::Arena* arena, const Handle<Value>& value, bool unchecked = false);

Handle<NullValue> CreateNullValue();

Handle<BoolValue> CreateBoolValue(bool value);

Handle<IntValue> CreateIntValue(int64_t value);

Handle<UintValue> CreateUintValue(uint64_t value);

Handle<DoubleValue> CreateDoubleValue(double value);

Handle<ListValue> CreateLegacyListValue(
    const google::api::expr::runtime::CelList* value);

Handle<MapValue> CreateLegacyMapValue(
    const google::api::expr::runtime::CelMap* value);

// Create a modern string value, without validation or copying. Should only be
// used during interoperation.
Handle<StringValue> CreateStringValueFromView(absl::string_view value);

// Create a modern bytes value, without validation or copying. Should only be
// used during interoperation.
Handle<BytesValue> CreateBytesValueFromView(absl::string_view value);

// Create a modern duration value, without validation. Should only be used
// during interoperation.
// If value is out of CEL's supported range, returns an ErrorValue.
Handle<Value> CreateDurationValue(absl::Duration value, bool unchecked = false);

// Create a modern timestamp value, without validation. Should only be used
// during interoperation.
// TODO(uncreated-issue/39): Consider adding a check that the timestamp is in the
// supported range for CEL.
Handle<TimestampValue> CreateTimestampValue(absl::Time value);

Handle<ErrorValue> CreateErrorValueFromView(const absl::Status* value);

Handle<UnknownValue> CreateUnknownValueFromView(
    const base_internal::UnknownSet* value);

// Convert a legacy value to a modern value, CHECK failing if its not possible.
// This should only be used during rewriting of the evaluator when it is
// guaranteed that all modern and legacy values are interoperable, and the
// memory manager is google::protobuf::Arena.
Handle<Value> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena, const google::api::expr::runtime::CelValue& value,
    bool unchecked = false);
std::vector<Handle<Value>> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena,
    absl::Span<const google::api::expr::runtime::CelValue> values,
    bool unchecked = false);

// Convert a modern value to a legacy value, CHECK failing if its not possible.
// This should only be used during rewriting of the evaluator when it is
// guaranteed that all modern and legacy values are interoperable, and the
// memory manager is google::protobuf::Arena.
google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, const Handle<Value>& value, bool unchecked = false);

Handle<TypeValue> CreateTypeValueFromView(google::protobuf::Arena* arena,
                                          absl::string_view input);

}  // namespace cel::interop_internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_INTEROP_H_
