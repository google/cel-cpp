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

#include "eval/internal/interop.h"

#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/arena.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/internal/message_wrapper.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/struct_value.h"
#include "eval/internal/errors.h"
#include "eval/public/cel_options.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/unknown_set.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"

namespace cel::interop_internal {

namespace {

using ::cel::base_internal::HandleFactory;
using ::cel::base_internal::InlinedStringViewBytesValue;
using ::cel::base_internal::InlinedStringViewStringValue;
using ::cel::base_internal::LegacyTypeValue;
using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelMap;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::LegacyTypeAccessApis;
using ::google::api::expr::runtime::LegacyTypeInfoApis;
using ::google::api::expr::runtime::MessageWrapper;
using ::google::api::expr::runtime::ProtoWrapperTypeOptions;
using ::google::api::expr::runtime::UnknownSet;

class LegacyCelList final : public CelList {
 public:
  explicit LegacyCelList(Handle<ListValue> impl) : impl_(std::move(impl)) {}

  CelValue operator[](int index) const override { return Get(nullptr, index); }

  CelValue Get(google::protobuf::Arena* arena, int index) const override {
    if (arena == nullptr) {
      static const absl::Status* status = []() {
        return new absl::Status(absl::InvalidArgumentError(
            "CelList::Get must be called with google::protobuf::Arena* for "
            "interoperation"));
      }();
      return CelValue::CreateError(status);
    }
    // Do not do this at  home. This is extremely unsafe, and we only do it for
    // interoperation, because we know that references to the below should not
    // persist past the return value.
    extensions::ProtoMemoryManager memory_manager(arena);
    TypeFactory type_factory(memory_manager);
    TypeManager type_manager(type_factory, TypeProvider::Builtin());
    ValueFactory value_factory(type_manager);
    auto value = impl_->Get(value_factory, static_cast<size_t>(index));
    if (!value.ok()) {
      return CelValue::CreateError(
          google::protobuf::Arena::Create<absl::Status>(arena, value.status()));
    }
    auto legacy_value = ToLegacyValue(arena, *value);
    if (!legacy_value.ok()) {
      return CelValue::CreateError(
          google::protobuf::Arena::Create<absl::Status>(arena, legacy_value.status()));
    }
    return std::move(legacy_value).value();
  }

  // List size
  int size() const override { return static_cast<int>(impl_->size()); }

  Handle<ListValue> value() const { return impl_; }

 private:
  internal::TypeInfo TypeId() const override {
    return internal::TypeId<LegacyCelList>();
  }

  Handle<ListValue> impl_;
};

class LegacyCelMap final : public CelMap {
 public:
  explicit LegacyCelMap(Handle<MapValue> impl) : impl_(std::move(impl)) {}

  absl::optional<CelValue> operator[](CelValue key) const override {
    return Get(nullptr, key);
  }

  absl::optional<CelValue> Get(google::protobuf::Arena* arena,
                               CelValue key) const override {
    if (arena == nullptr) {
      static const absl::Status* status = []() {
        return new absl::Status(absl::InvalidArgumentError(
            "CelMap::Get must be called with google::protobuf::Arena* for "
            "interoperation"));
      }();
      return CelValue::CreateError(status);
    }
    auto modern_key = FromLegacyValue(arena, key);
    if (!modern_key.ok()) {
      return CelValue::CreateError(
          google::protobuf::Arena::Create<absl::Status>(arena, modern_key.status()));
    }
    // Do not do this at  home. This is extremely unsafe, and we only do it for
    // interoperation, because we know that references to the below should not
    // persist past the return value.
    extensions::ProtoMemoryManager memory_manager(arena);
    TypeFactory type_factory(memory_manager);
    TypeManager type_manager(type_factory, TypeProvider::Builtin());
    ValueFactory value_factory(type_manager);
    auto modern_value = impl_->Get(value_factory, *modern_key);
    if (!modern_value.ok()) {
      return CelValue::CreateError(
          google::protobuf::Arena::Create<absl::Status>(arena, modern_value.status()));
    }
    if (!(*modern_value).has_value()) {
      return absl::nullopt;
    }
    auto legacy_value = ToLegacyValue(arena, **modern_value);
    if (!legacy_value.ok()) {
      return CelValue::CreateError(
          google::protobuf::Arena::Create<absl::Status>(arena, legacy_value.status()));
    }
    return std::move(legacy_value).value();
  }

  absl::StatusOr<bool> Has(const CelValue& key) const override {
    // Do not do this at  home. This is extremely unsafe, and we only do it for
    // interoperation, because we know that references to the below should not
    // persist past the return value.
    google::protobuf::Arena arena;
    CEL_ASSIGN_OR_RETURN(auto modern_key, FromLegacyValue(&arena, key));
    return impl_->Has(modern_key);
  }

  int size() const override { return static_cast<int>(impl_->size()); }

  bool empty() const override { return impl_->empty(); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return ListKeys(nullptr);
  }

  absl::StatusOr<const CelList*> ListKeys(google::protobuf::Arena* arena) const override {
    if (arena == nullptr) {
      return absl::InvalidArgumentError(
          "CelMap::ListKeys must be called with google::protobuf::Arena* for "
          "interoperation");
    }
    // Do not do this at  home. This is extremely unsafe, and we only do it for
    // interoperation, because we know that references to the below should not
    // persist past the return value.
    extensions::ProtoMemoryManager memory_manager(arena);
    TypeFactory type_factory(memory_manager);
    TypeManager type_manager(type_factory, TypeProvider::Builtin());
    ValueFactory value_factory(type_manager);
    CEL_ASSIGN_OR_RETURN(auto list_keys, impl_->ListKeys(value_factory));
    CEL_ASSIGN_OR_RETURN(auto legacy_list_keys,
                         ToLegacyValue(arena, list_keys));
    return legacy_list_keys.ListOrDie();
  }

  Handle<MapValue> value() const { return impl_; }

 private:
  internal::TypeInfo TypeId() const override {
    return internal::TypeId<LegacyCelMap>();
  }

  Handle<MapValue> impl_;
};

absl::StatusOr<Handle<Value>> LegacyStructGetFieldImpl(
    const MessageWrapper& wrapper, absl::string_view field,
    ProtoWrapperTypeOptions unbox_option, MemoryManager& memory_manager) {
  const LegacyTypeAccessApis* access_api =
      wrapper.legacy_type_info()->GetAccessApis(wrapper);

  if (access_api == nullptr) {
    return interop_internal::CreateErrorValueFromView(
        interop_internal::CreateNoSuchFieldError(memory_manager, field));
  }

  CEL_ASSIGN_OR_RETURN(
      auto legacy_value,
      access_api->GetField(field, wrapper, unbox_option, memory_manager));
  return FromLegacyValue(
      extensions::ProtoMemoryManager::CastToProtoArena(memory_manager),
      legacy_value);
}

}  // namespace

internal::TypeInfo CelListAccess::TypeId(const CelList& list) {
  return list.TypeId();
}

internal::TypeInfo CelMapAccess::TypeId(const CelMap& map) {
  return map.TypeId();
}

Handle<StructType> LegacyStructTypeAccess::Create(uintptr_t message) {
  return base_internal::HandleFactory<StructType>::Make<
      base_internal::LegacyStructType>(message);
}

Handle<StructValue> LegacyStructValueAccess::Create(
    const MessageWrapper& wrapper) {
  return Create(MessageWrapperAccess::Message(wrapper),
                MessageWrapperAccess::TypeInfo(wrapper));
}

Handle<StructValue> LegacyStructValueAccess::Create(uintptr_t message,
                                                    uintptr_t type_info) {
  return base_internal::HandleFactory<StructValue>::Make<
      base_internal::LegacyStructValue>(message, type_info);
}

uintptr_t LegacyStructValueAccess::Message(
    const base_internal::LegacyStructValue& value) {
  return value.msg_;
}

uintptr_t LegacyStructValueAccess::TypeInfo(
    const base_internal::LegacyStructValue& value) {
  return value.type_info_;
}

MessageWrapper LegacyStructValueAccess::ToMessageWrapper(
    const base_internal::LegacyStructValue& value) {
  return MessageWrapperAccess::Make(Message(value), TypeInfo(value));
}

uintptr_t MessageWrapperAccess::Message(const MessageWrapper& wrapper) {
  return wrapper.message_ptr_;
}

uintptr_t MessageWrapperAccess::TypeInfo(const MessageWrapper& wrapper) {
  return reinterpret_cast<uintptr_t>(wrapper.legacy_type_info_);
}

MessageWrapper MessageWrapperAccess::Make(uintptr_t message,
                                          uintptr_t type_info) {
  return MessageWrapper(message,
                        reinterpret_cast<const LegacyTypeInfoApis*>(type_info));
}

MessageWrapper::Builder MessageWrapperAccess::ToBuilder(
    MessageWrapper& wrapper) {
  return wrapper.ToBuilder();
}

Handle<TypeValue> CreateTypeValueFromView(absl::string_view input) {
  return HandleFactory<TypeValue>::Make<LegacyTypeValue>(input);
}

Handle<ListValue> CreateLegacyListValue(const CelList* value) {
  if (CelListAccess::TypeId(*value) == internal::TypeId<LegacyCelList>()) {
    // Fast path.
    return static_cast<const LegacyCelList*>(value)->value();
  }
  return HandleFactory<ListValue>::Make<base_internal::LegacyListValue>(
      reinterpret_cast<uintptr_t>(value));
}

Handle<MapValue> CreateLegacyMapValue(const CelMap* value) {
  if (CelMapAccess::TypeId(*value) == internal::TypeId<LegacyCelMap>()) {
    // Fast path.
    return static_cast<const LegacyCelMap*>(value)->value();
  }
  return HandleFactory<MapValue>::Make<base_internal::LegacyMapValue>(
      reinterpret_cast<uintptr_t>(value));
}

base_internal::StringValueRep GetStringValueRep(
    const Handle<StringValue>& value) {
  return value->rep();
}

base_internal::BytesValueRep GetBytesValueRep(const Handle<BytesValue>& value) {
  return value->rep();
}

absl::StatusOr<Handle<Value>> FromLegacyValue(google::protobuf::Arena* arena,
                                              const CelValue& legacy_value,
                                              bool unchecked) {
  switch (legacy_value.type()) {
    case CelValue::Type::kNullType:
      return CreateNullValue();
    case CelValue::Type::kBool:
      return CreateBoolValue(legacy_value.BoolOrDie());
    case CelValue::Type::kInt64:
      return CreateIntValue(legacy_value.Int64OrDie());
    case CelValue::Type::kUint64:
      return CreateUintValue(legacy_value.Uint64OrDie());
    case CelValue::Type::kDouble:
      return CreateDoubleValue(legacy_value.DoubleOrDie());
    case CelValue::Type::kString:
      return CreateStringValueFromView(legacy_value.StringOrDie().value());
    case CelValue::Type::kBytes:
      return CreateBytesValueFromView(legacy_value.BytesOrDie().value());
    case CelValue::Type::kMessage: {
      const auto& wrapper = legacy_value.MessageWrapperOrDie();
      return LegacyStructValueAccess::Create(
          MessageWrapperAccess::Message(wrapper),
          MessageWrapperAccess::TypeInfo(wrapper));
    }
    case CelValue::Type::kDuration:
      return CreateDurationValue(legacy_value.DurationOrDie(), unchecked);
    case CelValue::Type::kTimestamp:
      return CreateTimestampValue(legacy_value.TimestampOrDie());
    case CelValue::Type::kList:
      return CreateLegacyListValue(legacy_value.ListOrDie());
    case CelValue::Type::kMap:
      return CreateLegacyMapValue(legacy_value.MapOrDie());
    case CelValue::Type::kUnknownSet:
      return CreateUnknownValueFromView(legacy_value.UnknownSetOrDie());
    case CelValue::Type::kCelType:
      return CreateTypeValueFromView(legacy_value.CelTypeOrDie().value());
    case CelValue::Type::kError:
      return CreateErrorValueFromView(legacy_value.ErrorOrDie());
    case CelValue::Type::kAny:
      return absl::InternalError(absl::StrCat(
          "illegal attempt to convert special CelValue type ",
          CelValue::TypeName(legacy_value.type()), " to cel::Value"));
    default:
      break;
  }
  return absl::UnimplementedError(absl::StrCat(
      "conversion from CelValue to cel::Value for type ",
      CelValue::TypeName(legacy_value.type()), " is not yet implemented"));
}

namespace {

struct BytesValueToLegacyVisitor final {
  google::protobuf::Arena* arena;

  absl::StatusOr<CelValue> operator()(absl::string_view value) const {
    return CelValue::CreateBytesView(value);
  }

  absl::StatusOr<CelValue> operator()(const absl::Cord& value) const {
    return CelValue::CreateBytes(google::protobuf::Arena::Create<std::string>(
        arena, static_cast<std::string>(value)));
  }
};

struct StringValueToLegacyVisitor final {
  google::protobuf::Arena* arena;

  absl::StatusOr<CelValue> operator()(absl::string_view value) const {
    return CelValue::CreateStringView(value);
  }

  absl::StatusOr<CelValue> operator()(const absl::Cord& value) const {
    return CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
        arena, static_cast<std::string>(value)));
  }
};

}  // namespace

struct ErrorValueAccess final {
  static const absl::Status* value_ptr(const ErrorValue& value) {
    return value.value_ptr_;
  }
};

struct UnknownValueAccess final {
  static const base_internal::UnknownSet& value(const UnknownValue& value) {
    return value.value_;
  }

  static const base_internal::UnknownSet* value_ptr(const UnknownValue& value) {
    return value.value_ptr_;
  }
};

absl::StatusOr<CelValue> ToLegacyValue(google::protobuf::Arena* arena,
                                       const Handle<Value>& value,
                                       bool unchecked) {
  switch (value->kind()) {
    case Kind::kNullType:
      return CelValue::CreateNull();
    case Kind::kError: {
      if (base_internal::Metadata::IsTrivial(*value)) {
        return CelValue::CreateError(
            ErrorValueAccess::value_ptr(*value.As<ErrorValue>()));
      }
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, value.As<ErrorValue>()->value()));
    }
    case Kind::kDyn:
      break;
    case Kind::kAny:
      break;
    case Kind::kType:
      // Should be fine, so long as we are using an arena allocator.
      // We can only transport legacy type values.
      if (!base_internal::Metadata::IsTrivial(*value)) {
        // Only legacy type values are trivially copyable, this must be the
        // modern one which we cannot support here.
        return absl::UnimplementedError(
            "only legacy type values can be used for interop");
      }
      return CelValue::CreateCelTypeView(value.As<TypeValue>()->name());
    case Kind::kBool:
      return CelValue::CreateBool(value.As<BoolValue>()->value());
    case Kind::kInt:
      return CelValue::CreateInt64(value.As<IntValue>()->value());
    case Kind::kUint:
      return CelValue::CreateUint64(value.As<UintValue>()->value());
    case Kind::kDouble:
      return CelValue::CreateDouble(value.As<DoubleValue>()->value());
    case Kind::kString:
      return absl::visit(StringValueToLegacyVisitor{arena},
                         GetStringValueRep(value.As<StringValue>()));
    case Kind::kBytes:
      return absl::visit(BytesValueToLegacyVisitor{arena},
                         GetBytesValueRep(value.As<BytesValue>()));
    case Kind::kEnum:
      break;
    case Kind::kDuration:
      return unchecked
                 ? CelValue::CreateUncheckedDuration(
                       value.As<DurationValue>()->value())
                 : CelValue::CreateDuration(value.As<DurationValue>()->value());
    case Kind::kTimestamp:
      return CelValue::CreateTimestamp(value.As<TimestampValue>()->value());
    case Kind::kList: {
      if (value.Is<base_internal::LegacyListValue>()) {
        // Fast path.
        return CelValue::CreateList(reinterpret_cast<const CelList*>(
            value.As<base_internal::LegacyListValue>()->value()));
      }
      return CelValue::CreateList(
          google::protobuf::Arena::Create<LegacyCelList>(arena, value.As<ListValue>()));
    }
    case Kind::kMap: {
      if (value.Is<base_internal::LegacyMapValue>()) {
        // Fast path.
        return CelValue::CreateMap(reinterpret_cast<const CelMap*>(
            value.As<base_internal::LegacyMapValue>()->value()));
      }
      return CelValue::CreateMap(
          google::protobuf::Arena::Create<LegacyCelMap>(arena, value.As<MapValue>()));
    }
    case Kind::kStruct: {
      if (!value.Is<base_internal::LegacyStructValue>()) {
        return absl::UnimplementedError(
            "only legacy struct types and values can be used for interop");
      }
      uintptr_t message = LegacyStructValueAccess::Message(
          *value.As<base_internal::LegacyStructValue>());
      uintptr_t type_info = LegacyStructValueAccess::TypeInfo(
          *value.As<base_internal::LegacyStructValue>());
      return CelValue::CreateMessageWrapper(
          MessageWrapperAccess::Make(message, type_info));
    }
    case Kind::kUnknown: {
      if (base_internal::Metadata::IsTrivial(*value)) {
        return CelValue::CreateUnknownSet(
            UnknownValueAccess::value_ptr(*value.As<UnknownValue>()));
      }
      return CelValue::CreateUnknownSet(
          google::protobuf::Arena::Create<base_internal::UnknownSet>(
              arena, UnknownValueAccess::value(*value.As<UnknownValue>())));
    }
    default:
      break;
  }
  return absl::UnimplementedError(
      absl::StrCat("conversion from cel::Value to CelValue for type ",
                   KindToString(value->kind()), " is not yet implemented"));
}

Handle<NullValue> CreateNullValue() {
  return HandleFactory<NullValue>::Make<NullValue>();
}

Handle<BoolValue> CreateBoolValue(bool value) {
  return HandleFactory<BoolValue>::Make<BoolValue>(value);
}

Handle<IntValue> CreateIntValue(int64_t value) {
  return HandleFactory<IntValue>::Make<IntValue>(value);
}

Handle<UintValue> CreateUintValue(uint64_t value) {
  return HandleFactory<UintValue>::Make<UintValue>(value);
}

Handle<DoubleValue> CreateDoubleValue(double value) {
  return HandleFactory<DoubleValue>::Make<DoubleValue>(value);
}

Handle<StringValue> CreateStringValueFromView(absl::string_view value) {
  return HandleFactory<StringValue>::Make<InlinedStringViewStringValue>(value);
}

Handle<BytesValue> CreateBytesValueFromView(absl::string_view value) {
  return HandleFactory<BytesValue>::Make<InlinedStringViewBytesValue>(value);
}

Handle<Value> CreateDurationValue(absl::Duration value, bool unchecked) {
  if (!unchecked && (value >= kDurationHigh || value <= kDurationLow)) {
    return CreateErrorValueFromView(DurationOverflowError());
  }
  return HandleFactory<DurationValue>::Make<DurationValue>(value);
}

Handle<TimestampValue> CreateTimestampValue(absl::Time value) {
  return HandleFactory<TimestampValue>::Make<TimestampValue>(value);
}

Handle<ErrorValue> CreateErrorValueFromView(const absl::Status* value) {
  return HandleFactory<ErrorValue>::Make<ErrorValue>(value);
}

Handle<UnknownValue> CreateUnknownValueFromView(
    const base_internal::UnknownSet* value) {
  return HandleFactory<UnknownValue>::Make<UnknownValue>(value);
}

Handle<Value> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena, const google::api::expr::runtime::CelValue& value,
    bool unchecked) {
  auto modern_value = FromLegacyValue(arena, value, unchecked);
  CHECK_OK(modern_value);  // Crash OK
  return std::move(modern_value).value();
}

Handle<Value> LegacyValueToModernValueOrDie(
    MemoryManager& memory_manager,
    const google::api::expr::runtime::CelValue& value, bool unchecked) {
  return LegacyValueToModernValueOrDie(
      extensions::ProtoMemoryManager::CastToProtoArena(memory_manager), value,
      unchecked);
}

std::vector<Handle<Value>> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena,
    absl::Span<const google::api::expr::runtime::CelValue> values,
    bool unchecked) {
  std::vector<Handle<Value>> modern_values;
  modern_values.reserve(values.size());
  for (const auto& value : values) {
    modern_values.push_back(
        LegacyValueToModernValueOrDie(arena, value, unchecked));
  }
  return modern_values;
}

std::vector<Handle<Value>> LegacyValueToModernValueOrDie(
    MemoryManager& memory_manager,
    absl::Span<const google::api::expr::runtime::CelValue> values,
    bool unchecked) {
  return LegacyValueToModernValueOrDie(
      extensions::ProtoMemoryManager::CastToProtoArena(memory_manager), values);
}

google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, const Handle<Value>& value, bool unchecked) {
  auto legacy_value = ToLegacyValue(arena, value, unchecked);
  CHECK_OK(legacy_value);  // Crash OK
  return std::move(legacy_value).value();
}

google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    MemoryManager& memory_manager, const Handle<Value>& value, bool unchecked) {
  return ModernValueToLegacyValueOrDie(
      extensions::ProtoMemoryManager::CastToProtoArena(memory_manager), value,
      unchecked);
}

std::vector<google::api::expr::runtime::CelValue> ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, absl::Span<const Handle<Value>> values,
    bool unchecked) {
  std::vector<google::api::expr::runtime::CelValue> legacy_values;
  legacy_values.reserve(values.size());
  for (const auto& value : values) {
    legacy_values.push_back(
        ModernValueToLegacyValueOrDie(arena, value, unchecked));
  }
  return legacy_values;
}

std::vector<google::api::expr::runtime::CelValue> ModernValueToLegacyValueOrDie(
    MemoryManager& memory_manager, absl::Span<const Handle<Value>> values,
    bool unchecked) {
  return ModernValueToLegacyValueOrDie(
      extensions::ProtoMemoryManager::CastToProtoArena(memory_manager), values,
      unchecked);
}

absl::StatusOr<Handle<Value>> MessageValueGetFieldWithWrapperAsProtoDefault(
    const Handle<StructValue>& struct_value, ValueFactory& value_factory,
    absl::string_view field) {
  if (struct_value.Is<base_internal::LegacyStructValue>()) {
    const auto& legacy_value =
        struct_value.As<base_internal::LegacyStructValue>();
    auto wrapper = LegacyStructValueAccess::ToMessageWrapper(*legacy_value);
    return LegacyStructGetFieldImpl(wrapper, field,
                                    ProtoWrapperTypeOptions::kUnsetProtoDefault,
                                    value_factory.memory_manager());
  }

  // Otherwise assume struct impl has the appropriate option set.
  return struct_value->GetField(value_factory, StructValue::FieldId(field));
}

}  // namespace cel::interop_internal

namespace cel::base_internal {

namespace {

using ::cel::interop_internal::FromLegacyValue;
using ::cel::interop_internal::LegacyStructValueAccess;
using ::cel::interop_internal::MessageWrapperAccess;
using ::cel::interop_internal::ToLegacyValue;
using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelMap;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::LegacyTypeAccessApis;
using ::google::api::expr::runtime::LegacyTypeInfoApis;
using ::google::api::expr::runtime::MessageWrapper;
using ::google::api::expr::runtime::ProtoWrapperTypeOptions;

}  // namespace

absl::string_view MessageTypeName(uintptr_t msg) {
  if ((msg & kMessageWrapperTagMask) != kMessageWrapperTagMask) {
    // For google::protobuf::MessageLite, this is actually LegacyTypeInfoApis.
    return reinterpret_cast<const LegacyTypeInfoApis*>(msg)->GetTypename(
        MessageWrapper());
  }
  return reinterpret_cast<const google::protobuf::Message*>(msg & kMessageWrapperPtrMask)
      ->GetDescriptor()
      ->full_name();
}

void MessageValueHash(uintptr_t msg, uintptr_t type_info,
                      absl::HashState state) {
  // Getting rid of hash, do nothing.
}

bool MessageValueEquals(uintptr_t lhs_msg, uintptr_t lhs_type_info,
                        const Value& rhs) {
  if (!LegacyStructValue::Is(rhs)) {
    return false;
  }
  auto lhs_message_wrapper = MessageWrapperAccess::Make(lhs_msg, lhs_type_info);

  const LegacyTypeAccessApis* access_api =
      lhs_message_wrapper.legacy_type_info()->GetAccessApis(
          lhs_message_wrapper);

  if (access_api == nullptr) {
    return false;
  }

  return access_api->IsEqualTo(
      lhs_message_wrapper,
      LegacyStructValueAccess::ToMessageWrapper(
          static_cast<const base_internal::LegacyStructValue&>(rhs)));
}

absl::StatusOr<bool> MessageValueHasFieldByNumber(uintptr_t msg,
                                                  uintptr_t type_info,
                                                  int64_t number) {
  return absl::UnimplementedError(
      "legacy struct values do not support looking up fields by number");
}

absl::StatusOr<bool> MessageValueHasFieldByName(uintptr_t msg,
                                                uintptr_t type_info,
                                                absl::string_view name) {
  auto wrapper = MessageWrapperAccess::Make(msg, type_info);
  const LegacyTypeAccessApis* access_api =
      wrapper.legacy_type_info()->GetAccessApis(wrapper);

  if (access_api == nullptr) {
    return absl::NotFoundError(
        absl::StrCat(interop_internal::kErrNoSuchField, ": ", name));
  }

  return access_api->HasField(name, wrapper);
}

absl::StatusOr<Handle<Value>> MessageValueGetFieldByNumber(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    int64_t number) {
  return absl::UnimplementedError(
      "legacy struct values do not supported looking up fields by number");
}

absl::StatusOr<Handle<Value>> MessageValueGetFieldByName(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    absl::string_view name) {
  auto wrapper = MessageWrapperAccess::Make(msg, type_info);

  return interop_internal::LegacyStructGetFieldImpl(
      wrapper, name, ProtoWrapperTypeOptions::kUnsetNull,
      value_factory.memory_manager());
}

absl::StatusOr<Handle<Value>> LegacyListValueGet(uintptr_t impl,
                                                 ValueFactory& value_factory,
                                                 size_t index) {
  auto* arena = extensions::ProtoMemoryManager::CastToProtoArena(
      value_factory.memory_manager());
  return FromLegacyValue(arena, reinterpret_cast<const CelList*>(impl)->Get(
                                    arena, static_cast<int>(index)));
}

size_t LegacyListValueSize(uintptr_t impl) {
  return reinterpret_cast<const CelList*>(impl)->size();
}

bool LegacyListValueEmpty(uintptr_t impl) {
  return reinterpret_cast<const CelList*>(impl)->empty();
}

size_t LegacyMapValueSize(uintptr_t impl) {
  return reinterpret_cast<const CelMap*>(impl)->size();
}

bool LegacyMapValueEmpty(uintptr_t impl) {
  return reinterpret_cast<const CelMap*>(impl)->empty();
}

absl::StatusOr<absl::optional<Handle<Value>>> LegacyMapValueGet(
    uintptr_t impl, ValueFactory& value_factory, const Handle<Value>& key) {
  auto* arena = extensions::ProtoMemoryManager::CastToProtoArena(
      value_factory.memory_manager());
  CEL_ASSIGN_OR_RETURN(auto legacy_key, ToLegacyValue(arena, key));
  auto legacy_value =
      reinterpret_cast<const CelMap*>(impl)->Get(arena, legacy_key);
  if (!legacy_value.has_value()) {
    return absl::nullopt;
  }
  return FromLegacyValue(arena, *legacy_value);
}

absl::StatusOr<bool> LegacyMapValueHas(uintptr_t impl,
                                       const Handle<Value>& key) {
  google::protobuf::Arena arena;
  CEL_ASSIGN_OR_RETURN(auto legacy_key, ToLegacyValue(&arena, key));
  return reinterpret_cast<const CelMap*>(impl)->Has(legacy_key);
}

absl::StatusOr<Handle<ListValue>> LegacyMapValueListKeys(
    uintptr_t impl, ValueFactory& value_factory) {
  auto* arena = extensions::ProtoMemoryManager::CastToProtoArena(
      value_factory.memory_manager());
  CEL_ASSIGN_OR_RETURN(auto legacy_list_keys,
                       reinterpret_cast<const CelMap*>(impl)->ListKeys(arena));
  CEL_ASSIGN_OR_RETURN(
      auto list_keys,
      FromLegacyValue(arena, CelValue::CreateList(legacy_list_keys)));
  return list_keys.As<ListValue>();
}

}  // namespace cel::base_internal
