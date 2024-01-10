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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/internal/message_wrapper.h"
#include "base/internal/unknown_set.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/null_value.h"
#include "base/values/struct_value.h"
#include "common/native_type.h"
#include "common/value_kind.h"
#include "eval/internal/cel_value_equal.h"
#include "eval/internal/errors.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/unknown_set.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/overloaded.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {
ABSL_ATTRIBUTE_WEAK const LegacyTypeInfoApis& GetGenericProtoTypeInfoInstance();
}

namespace cel::interop_internal {

ABSL_ATTRIBUTE_WEAK absl::optional<google::api::expr::runtime::MessageWrapper>
ProtoStructValueToMessageWrapper(const Value& value);

namespace {

using ::cel::base_internal::HandleFactory;
using ::cel::base_internal::InlinedStringViewBytesValue;
using ::cel::base_internal::InlinedStringViewStringValue;
using ::cel::base_internal::LegacyTypeValue;
using ::cel::runtime_internal::DurationOverflowError;
using ::cel::runtime_internal::kDurationHigh;
using ::cel::runtime_internal::kDurationLow;
using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelMap;
using ::google::api::expr::runtime::CelValue;
using MessageWrapper = ::google::api::expr::runtime::CelValue::MessageWrapper;
using extensions::ProtoMemoryManagerArena;
using extensions::ProtoMemoryManagerRef;
using ::google::api::expr::runtime::LegacyTypeAccessApis;
using ::google::api::expr::runtime::LegacyTypeInfoApis;
using ::google::api::expr::runtime::LegacyTypeMutationApis;
using ::google::api::expr::runtime::ProtoWrapperTypeOptions;
using ::google::api::expr::runtime::UnknownSet;

// Implementation of `StructValueBuilderInterface` for legacy struct types.
class LegacyAbstractStructValueBuilder final
    : public StructValueBuilderInterface {
 public:
  LegacyAbstractStructValueBuilder(ValueFactory& value_factory,
                                   const LegacyTypeInfoApis& type_info,
                                   const LegacyTypeMutationApis& mutation,
                                   MessageWrapper instance,
                                   MessageWrapper::Builder builder)
      : value_factory_(value_factory),
        type_info_(type_info),
        mutation_(mutation),
        instance_(instance),
        builder_(std::move(builder)) {}

  absl::Status SetFieldByName(absl::string_view name,
                              Handle<Value> value) override {
    CEL_ASSIGN_OR_RETURN(auto arg,
                         ToLegacyValue(ProtoMemoryManagerArena(
                                           value_factory_.GetMemoryManager()),
                                       value, true));
    return mutation_.SetField(name, arg, value_factory_.GetMemoryManager(),
                              builder_);
  }

  absl::Status SetFieldByNumber(int64_t number, Handle<Value> value) override {
    CEL_ASSIGN_OR_RETURN(auto arg,
                         ToLegacyValue(ProtoMemoryManagerArena(
                                           value_factory_.GetMemoryManager()),
                                       value, true));
    return mutation_.SetFieldByNumber(
        number, arg, value_factory_.GetMemoryManager(), builder_);
  }

  absl::StatusOr<cel::Handle<cel::StructValue>> Build() && override {
    CEL_ASSIGN_OR_RETURN(auto legacy, mutation_.AdaptFromWellKnownType(
                                          value_factory_.GetMemoryManager(),
                                          std::move(builder_)));
    if (!legacy.IsMessage()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Expected struct/message when parsing and adapting ",
          type_info_.GetTypename(instance_), ": ", legacy.DebugString()));
    }
    CEL_ASSIGN_OR_RETURN(auto modern,
                         FromLegacyValue(ProtoMemoryManagerArena(
                                             value_factory_.GetMemoryManager()),
                                         legacy, true));
    return std::move(modern).As<StructValue>();
  }

 private:
  ValueFactory& value_factory_;
  const LegacyTypeInfoApis& type_info_;
  const LegacyTypeMutationApis& mutation_;
  MessageWrapper instance_;
  MessageWrapper::Builder builder_;
};

class LegacyCelList final : public CelList {
 public:
  // Arena ptr must be the same arena the list was allocated on.
  LegacyCelList(Handle<ListValue> impl, google::protobuf::Arena* arena)
      : impl_(std::move(impl)), arena_(arena) {}

  CelValue operator[](int index) const override { return Get(arena_, index); }

  CelValue Get(google::protobuf::Arena* arena, int index) const override {
    if (arena == nullptr) {
      arena = arena_;
    }
    if (arena == nullptr) {
      static const absl::Status* status = []() {
        return new absl::Status(absl::InvalidArgumentError(
            "CelList::Get must be called with non-null google::protobuf::Arena* for "
            "interoperation"));
      }();
      return CelValue::CreateError(status);
    }
    // Do not do this at  home. This is extremely unsafe, and we only do it for
    // interoperation, because we know that references to the below should not
    // persist past the return value.
    auto memory_manager = ProtoMemoryManagerRef(arena);
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
  int size() const override { return static_cast<int>(impl_->Size()); }

  Handle<ListValue> value() const { return impl_; }

 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<LegacyCelList>();
  }

  Handle<ListValue> impl_;
  google::protobuf::Arena* arena_;
};

class LegacyCelMap final : public CelMap {
 public:
  explicit LegacyCelMap(Handle<MapValue> impl, google::protobuf::Arena* arena)
      : impl_(std::move(impl)), arena_(arena) {}

  absl::optional<CelValue> operator[](CelValue key) const override {
    return Get(arena_, key);
  }

  absl::optional<CelValue> Get(google::protobuf::Arena* arena,
                               CelValue key) const override {
    if (arena == nullptr) {
      arena = arena_;
    }
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
    auto memory_manager = ProtoMemoryManagerRef(arena);
    TypeFactory type_factory(memory_manager);
    TypeManager type_manager(type_factory, TypeProvider::Builtin());
    ValueFactory value_factory(type_manager);
    auto modern_value = impl_->Find(value_factory, *modern_key);
    if (!modern_value.ok()) {
      return CelValue::CreateError(
          google::protobuf::Arena::Create<absl::Status>(arena, modern_value.status()));
    }
    if (!(*modern_value).second) {
      return absl::nullopt;
    }
    auto legacy_value = ToLegacyValue(arena, (*modern_value).first);
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
    auto memory_manager = ProtoMemoryManagerRef(&arena);
    TypeFactory type_factory(memory_manager);
    TypeManager type_manager(type_factory, TypeProvider::Builtin());
    ValueFactory value_factory(type_manager);
    CEL_ASSIGN_OR_RETURN(auto modern_key, FromLegacyValue(&arena, key));
    CEL_ASSIGN_OR_RETURN(auto has, impl_->Has(value_factory, modern_key));
    return has->Is<BoolValue>() && has->As<BoolValue>().NativeValue();
  }

  int size() const override { return static_cast<int>(impl_->Size()); }

  bool empty() const override { return impl_->IsEmpty(); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return ListKeys(arena_);
  }

  absl::StatusOr<const CelList*> ListKeys(google::protobuf::Arena* arena) const override {
    if (arena == nullptr) {
      arena = arena_;
    }
    if (arena == nullptr) {
      return absl::InvalidArgumentError(
          "CelMap::ListKeys must be called with google::protobuf::Arena* for "
          "interoperation");
    }
    // Do not do this at  home. This is extremely unsafe, and we only do it for
    // interoperation, because we know that references to the below should not
    // persist past the return value.
    auto memory_manager = ProtoMemoryManagerRef(arena);
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
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<LegacyCelMap>();
  }

  Handle<MapValue> impl_;
  google::protobuf::Arena* arena_;
};

absl::StatusOr<Handle<Value>> LegacyStructGetFieldImpl(
    const MessageWrapper& wrapper, absl::string_view field,
    bool unbox_null_wrapper_types, MemoryManagerRef memory_manager) {
  const LegacyTypeAccessApis* access_api =
      wrapper.legacy_type_info()->GetAccessApis(wrapper);

  google::protobuf::Arena* arena = extensions::ProtoMemoryManagerArena(memory_manager);
  if (access_api == nullptr) {
    return interop_internal::CreateErrorValueFromView(
        interop_internal::CreateNoSuchFieldError(arena, field));
  }

  CEL_ASSIGN_OR_RETURN(
      auto legacy_value,
      access_api->GetField(field, wrapper,
                           unbox_null_wrapper_types
                               ? ProtoWrapperTypeOptions::kUnsetNull
                               : ProtoWrapperTypeOptions::kUnsetProtoDefault,
                           memory_manager));
  return FromLegacyValue(arena, legacy_value);
}

absl::StatusOr<QualifyResult> LegacyStructQualifyImpl(
    const MessageWrapper& wrapper, absl::Span<const cel::SelectQualifier> path,
    bool presence_test, MemoryManagerRef memory_manager) {
  if (path.empty()) {
    return absl::InvalidArgumentError("invalid select qualifier path.");
  }
  const LegacyTypeAccessApis* access_api =
      wrapper.legacy_type_info()->GetAccessApis(wrapper);

  google::protobuf::Arena* arena = extensions::ProtoMemoryManagerArena(memory_manager);
  if (access_api == nullptr) {
    absl::string_view field_name = absl::visit(
        cel::internal::Overloaded{
            [](const cel::FieldSpecifier& field) -> absl::string_view {
              return field.name;
            },
            [](const cel::AttributeQualifier& field) -> absl::string_view {
              return field.GetStringKey().value_or("<invalid field>");
            }},
        path.front());
    return QualifyResult{
        interop_internal::CreateErrorValueFromView(
            interop_internal::CreateNoSuchFieldError(arena, field_name)),
        -1};
  }

  CEL_ASSIGN_OR_RETURN(
      LegacyTypeAccessApis::LegacyQualifyResult legacy_result,
      access_api->Qualify(path, wrapper, presence_test, memory_manager));

  QualifyResult result;
  result.qualifier_count = legacy_result.qualifier_count;

  CEL_ASSIGN_OR_RETURN(result.value,
                       FromLegacyValue(arena, legacy_result.value));
  return result;
}

}  // namespace

absl::string_view LegacyAbstractStructType::name() const {
  return type_info_.GetTypename(MessageWrapper());
}

size_t LegacyAbstractStructType::field_count() const {
  // Field iteration is unsupported.
  return 0;
}

// Called by FindField.
absl::StatusOr<absl::optional<StructType::Field>>
LegacyAbstractStructType::FindFieldByName(TypeManager& type_manager,
                                          absl::string_view name) const {
  absl::optional<LegacyTypeInfoApis::FieldDescription> maybe_field =
      type_info_.FindFieldByName(name);

  if (!maybe_field.has_value()) {
    const auto* mutation = type_info_.GetMutationApis(MessageWrapper());
    if (mutation == nullptr || !mutation->DefinesField(name)) {
      return absl::nullopt;
    }
    return StructType::Field(StructType::MakeFieldId(name), name, 0,
                             type_manager.type_factory().GetDynType());
  }
  return StructType::Field(maybe_field->number != 0
                               ? StructType::MakeFieldId(maybe_field->number)
                               : StructType::MakeFieldId(maybe_field->name),
                           maybe_field->name, maybe_field->number,
                           type_manager.type_factory().GetDynType());
}

absl::StatusOr<absl::optional<StructType::Field>>
LegacyAbstractStructType::FindFieldByNumber(TypeManager& type_manager,
                                            int64_t number) const {
  return absl::UnimplementedError(
      "FindFieldByNumber not implemented for legacy types.");
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<StructType::FieldIterator>>>
LegacyAbstractStructType::NewFieldIterator(TypeManager& type_manager) const {
  return absl::UnimplementedError(
      "Field iteration not implemented for legacy types");
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<StructValueBuilderInterface>>>
LegacyAbstractStructType::NewValueBuilder(
    ValueFactory& value_factory ABSL_ATTRIBUTE_LIFETIME_BOUND) const {
  const auto* mutation = type_info_.GetMutationApis(MessageWrapper());
  if (mutation == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Missing mutation APIs for ", name()));
  }
  CEL_ASSIGN_OR_RETURN(auto builder,
                       mutation->NewInstance(value_factory.GetMemoryManager()));
  return std::make_unique<LegacyAbstractStructValueBuilder>(
      value_factory, type_info_, *mutation, MessageWrapper(),
      std::move(builder));
}

absl::StatusOr<Handle<StructValue>> LegacyAbstractStructType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  const auto* mutation = type_info_.GetMutationApis(MessageWrapper());
  if (mutation == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Missing mutation APIs for ", name()));
  }
  CEL_ASSIGN_OR_RETURN(auto builder,
                       mutation->NewInstance(value_factory.GetMemoryManager()));
  if (!builder.message_ptr()->ParsePartialFromCord(value)) {
    return absl::InvalidArgumentError(absl::StrCat("Failed to parse ", name()));
  }
  CEL_ASSIGN_OR_RETURN(auto legacy,
                       mutation->AdaptFromWellKnownType(
                           value_factory.GetMemoryManager(), builder));
  if (!legacy.IsMessage()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected struct/message when parsing and adapting ",
                     name(), ": ", legacy.DebugString()));
  }
  CEL_ASSIGN_OR_RETURN(
      auto modern,
      FromLegacyValue(ProtoMemoryManagerArena(value_factory.GetMemoryManager()),
                      legacy, true));
  return std::move(modern).As<StructValue>();
}

NativeTypeId CelListAccess::TypeId(const CelList& list) {
  return list.GetNativeTypeId();
}

NativeTypeId CelMapAccess::TypeId(const CelMap& map) {
  return map.GetNativeTypeId();
}

Handle<StructType> LegacyStructTypeAccess::Create(uintptr_t message) {
  return base_internal::HandleFactory<StructType>::Make<
      base_internal::LegacyStructType>(message);
}

uintptr_t LegacyStructTypeAccess::GetStorage(
    const base_internal::LegacyStructType& type) {
  return type.msg_;
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
  if (CelListAccess::TypeId(*value) == NativeTypeId::For<LegacyCelList>()) {
    // Fast path.
    return static_cast<const LegacyCelList*>(value)->value();
  }
  return HandleFactory<ListValue>::Make<base_internal::LegacyListValue>(
      reinterpret_cast<uintptr_t>(value));
}

Handle<MapValue> CreateLegacyMapValue(const CelMap* value) {
  if (CelMapAccess::TypeId(*value) == NativeTypeId::For<LegacyCelMap>()) {
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
    case ValueKind::kNullType:
      return CelValue::CreateNull();
    case ValueKind::kError: {
      if (base_internal::Metadata::IsTrivial(*value)) {
        return CelValue::CreateError(
            ErrorValueAccess::value_ptr(*value.As<ErrorValue>()));
      }
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, value.As<ErrorValue>()->NativeValue()));
    }
    case ValueKind::kType: {
      // Should be fine, so long as we are using an arena allocator.
      // We can only transport legacy type values.
      if (base_internal::Metadata::GetInlineVariant<
              base_internal::InlinedTypeValueVariant>(*value) ==
          base_internal::InlinedTypeValueVariant::kLegacy) {
        return CelValue::CreateCelTypeView(value.As<TypeValue>()->name());
      }
      auto* type_name = google::protobuf::Arena::Create<std::string>(
          arena, value.As<TypeValue>()->name());

      return CelValue::CreateCelTypeView(*type_name);
    }
    case ValueKind::kBool:
      return CelValue::CreateBool(value.As<BoolValue>()->NativeValue());
    case ValueKind::kInt:
      return CelValue::CreateInt64(value.As<IntValue>()->NativeValue());
    case ValueKind::kUint:
      return CelValue::CreateUint64(value.As<UintValue>()->NativeValue());
    case ValueKind::kDouble:
      return CelValue::CreateDouble(value.As<DoubleValue>()->NativeValue());
    case ValueKind::kString:
      return absl::visit(StringValueToLegacyVisitor{arena},
                         GetStringValueRep(value.As<StringValue>()));
    case ValueKind::kBytes:
      return absl::visit(BytesValueToLegacyVisitor{arena},
                         GetBytesValueRep(value.As<BytesValue>()));
    case ValueKind::kEnum:
      break;
    case ValueKind::kDuration:
      return unchecked ? CelValue::CreateUncheckedDuration(
                             value.As<DurationValue>()->NativeValue())
                       : CelValue::CreateDuration(
                             value.As<DurationValue>()->NativeValue());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(
          value.As<TimestampValue>()->NativeValue());
    case ValueKind::kList: {
      if (value->Is<base_internal::LegacyListValue>()) {
        // Fast path.
        return CelValue::CreateList(reinterpret_cast<const CelList*>(
            value.As<base_internal::LegacyListValue>()->value()));
      }
      return CelValue::CreateList(google::protobuf::Arena::Create<LegacyCelList>(
          arena, value.As<ListValue>(), arena));
    }
    case ValueKind::kMap: {
      if (value->Is<base_internal::LegacyMapValue>()) {
        // Fast path.
        return CelValue::CreateMap(reinterpret_cast<const CelMap*>(
            value.As<base_internal::LegacyMapValue>()->value()));
      }
      return CelValue::CreateMap(google::protobuf::Arena::Create<LegacyCelMap>(
          arena, value.As<MapValue>(), arena));
    }
    case ValueKind::kStruct: {
      if (value->Is<base_internal::LegacyStructValue>()) {
        // "Legacy".
        uintptr_t message = LegacyStructValueAccess::Message(
            *value.As<base_internal::LegacyStructValue>());
        uintptr_t type_info = LegacyStructValueAccess::TypeInfo(
            *value.As<base_internal::LegacyStructValue>());
        return CelValue::CreateMessageWrapper(
            MessageWrapperAccess::Make(message, type_info));
      }
      if (ProtoStructValueToMessageWrapper) {
        auto maybe_message_wrapper = ProtoStructValueToMessageWrapper(*value);
        if (maybe_message_wrapper.has_value()) {
          return CelValue::CreateMessageWrapper(
              std::move(maybe_message_wrapper).value());
        }
      }
      return absl::UnimplementedError(
          "only legacy struct types and values can be used for interop");
    }
    case ValueKind::kUnknown: {
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
  return absl::UnimplementedError(absl::StrCat(
      "conversion from cel::Value to CelValue for type ",
      ValueKindToString(value->kind()), " is not yet implemented"));
}

Handle<StructType> CreateStructTypeFromLegacyTypeInfo(
    const LegacyTypeInfoApis* type_info) {
  return LegacyStructTypeAccess::Create(reinterpret_cast<uintptr_t>(type_info));
}

const google::api::expr::runtime::LegacyTypeInfoApis* LegacyTypeInfoFromType(
    const Handle<Type>& type) {
  if (!type->Is<base_internal::LegacyStructType>()) {
    return nullptr;
  }
  uintptr_t representation = LegacyStructTypeAccess::GetStorage(
      type->As<base_internal::LegacyStructType>());
  if ((representation & base_internal::kMessageWrapperTagMask) !=
      base_internal::kMessageWrapperTagTypeInfoValue) {
    return nullptr;
  }

  return reinterpret_cast<google::api::expr::runtime::LegacyTypeInfoApis*>(
      representation & base_internal::kMessageWrapperPtrMask);
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
  ABSL_CHECK_OK(modern_value);  // Crash OK
  return std::move(modern_value).value();
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

google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, const Handle<Value>& value, bool unchecked) {
  auto legacy_value = ToLegacyValue(arena, value, unchecked);
  ABSL_CHECK_OK(legacy_value);  // Crash OK
  return std::move(legacy_value).value();
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
using MessageWrapper = ::google::api::expr::runtime::CelValue::MessageWrapper;
using ::google::api::expr::runtime::LegacyTypeAccessApis;
using ::google::api::expr::runtime::LegacyTypeInfoApis;

}  // namespace

absl::string_view MessageTypeName(uintptr_t msg) {
  uintptr_t tag = (msg & kMessageWrapperTagMask);
  uintptr_t ptr = (msg & kMessageWrapperPtrMask);

  if (tag == kMessageWrapperTagTypeInfoValue) {
    // For google::protobuf::MessageLite, this is actually LegacyTypeInfoApis.
    return reinterpret_cast<const LegacyTypeInfoApis*>(ptr)->GetTypename(
        MessageWrapper());
  }
  ABSL_ASSERT(tag == kMessageWrapperTagMessageValue);

  return reinterpret_cast<const google::protobuf::Message*>(ptr)
      ->GetDescriptor()
      ->full_name();
}

size_t MessageTypeFieldCount(uintptr_t msg) { return 0; }

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

size_t MessageValueFieldCount(uintptr_t msg, uintptr_t type_info) {
  auto message_wrapper = MessageWrapperAccess::Make(msg, type_info);
  if (message_wrapper.message_ptr() == nullptr) {
    return 0;
  }
  const LegacyTypeAccessApis* access_api =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  return access_api->ListFields(message_wrapper).size();
}

std::vector<absl::string_view> MessageValueListFields(uintptr_t msg,
                                                      uintptr_t type_info) {
  auto message_wrapper = MessageWrapperAccess::Make(msg, type_info);
  if (message_wrapper.message_ptr() == nullptr) {
    return std::vector<absl::string_view>{};
  }
  const LegacyTypeAccessApis* access_api =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  return access_api->ListFields(message_wrapper);
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
        absl::StrCat(runtime_internal::kErrNoSuchField, ": ", name));
  }

  return access_api->HasField(name, wrapper);
}

absl::StatusOr<Handle<Value>> MessageValueGetFieldByNumber(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    int64_t number, bool unbox_null_wrapper_types) {
  return absl::UnimplementedError(
      "legacy struct values do not supported looking up fields by number");
}

absl::StatusOr<Handle<Value>> MessageValueGetFieldByName(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    absl::string_view name, bool unbox_null_wrapper_types) {
  auto wrapper = MessageWrapperAccess::Make(msg, type_info);

  return interop_internal::LegacyStructGetFieldImpl(
      wrapper, name, unbox_null_wrapper_types,
      value_factory.GetMemoryManager());
}

absl::StatusOr<QualifyResult> MessageValueQualify(
    uintptr_t msg, uintptr_t type_info, ValueFactory& value_factory,
    absl::Span<const SelectQualifier> qualifiers, bool presence_test) {
  auto wrapper = MessageWrapperAccess::Make(msg, type_info);

  return interop_internal::LegacyStructQualifyImpl(
      wrapper, qualifiers, presence_test, value_factory.GetMemoryManager());
}

absl::StatusOr<Handle<Value>> LegacyListValueGet(uintptr_t impl,
                                                 ValueFactory& value_factory,
                                                 size_t index) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_factory.GetMemoryManager());
  return FromLegacyValue(arena, reinterpret_cast<const CelList*>(impl)->Get(
                                    arena, static_cast<int>(index)));
}

absl::StatusOr<Handle<Value>> LegacyListValueContains(
    ValueFactory& value_factory, uintptr_t impl, const Handle<Value>& other) {
  // Specialization for contains on a legacy list. It's cheaper to convert the
  // search element to legacy type than to convert all of the elements of
  // the legacy list for 'in' checks.
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_factory.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto legacy_value, ToLegacyValue(arena, other));
  const auto* list = reinterpret_cast<const CelList*>(impl);

  for (int i = 0; i < list->size(); i++) {
    CelValue element = list->Get(arena, i);
    absl::optional<bool> equal =
        interop_internal::CelValueEqualImpl(element, legacy_value);
    // Heterogenous equality behavior is to just return false if equality
    // undefined.
    if (equal.has_value() && *equal) {
      return value_factory.CreateBoolValue(true);
    }
  }
  return value_factory.CreateBoolValue(false);
}

size_t LegacyListValueSize(uintptr_t impl) {
  return reinterpret_cast<const CelList*>(impl)->size();
}

bool LegacyListValueEmpty(uintptr_t impl) {
  return reinterpret_cast<const CelList*>(impl)->empty();
}

absl::StatusOr<bool> LegacyListValueAnyOf(ValueFactory& value_factory,
                                          uintptr_t impl,
                                          ListValue::AnyOfCallback cb) {
  const auto* list = reinterpret_cast<const CelList*>(impl);
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_factory.GetMemoryManager());
  for (int i = 0; i < list->size(); ++i) {
    absl::StatusOr<Handle<Value>> value =
        FromLegacyValue(arena, list->Get(arena, i));
    if (!value.ok()) {
      return value.status();
    }
    auto condition = cb(*value);
    if (!condition.ok() || *condition) {
      return condition;
    }
  }
  return false;
}

size_t LegacyMapValueSize(uintptr_t impl) {
  return reinterpret_cast<const CelMap*>(impl)->size();
}

bool LegacyMapValueEmpty(uintptr_t impl) {
  return reinterpret_cast<const CelMap*>(impl)->empty();
}

absl::StatusOr<absl::optional<Handle<Value>>> LegacyMapValueGet(
    uintptr_t impl, ValueFactory& value_factory, const Handle<Value>& key) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_factory.GetMemoryManager());
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
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_factory.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto legacy_list_keys,
                       reinterpret_cast<const CelMap*>(impl)->ListKeys(arena));
  CEL_ASSIGN_OR_RETURN(
      auto list_keys,
      FromLegacyValue(arena, CelValue::CreateList(legacy_list_keys)));
  return list_keys.As<ListValue>();
}

}  // namespace cel::base_internal
