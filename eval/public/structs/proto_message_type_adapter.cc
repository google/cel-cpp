// Copyright 2022 Google LLC
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

#include "eval/public/structs/proto_message_type_adapter.h"

#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/util/message_differencer.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "base/builtins.h"
#include "common/kind.h"
#include "common/memory.h"
#include "common/value.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/internal_field_backed_list_impl.h"
#include "eval/public/containers/internal_field_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrap_util.h"
#include "eval/public/structs/field_access_impl.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/status_macros.h"
#include "runtime/internal/errors.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::extensions::ProtoMemoryManagerArena;
using ::cel::runtime_internal::CreateInvalidMapKeyTypeError;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;
using ::cel::runtime_internal::CreateNoSuchKeyError;
using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::MapValueConstRef;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

using LegacyQualifyResult = LegacyTypeAccessApis::LegacyQualifyResult;

const std::string& UnsupportedTypeName() {
  static absl::NoDestructor<std::string> kUnsupportedTypeName(
      "<unknown message>");
  return *kUnsupportedTypeName;
}

// JSON container types and Any have special unpacking rules.
//
// Not considered for qualify traversal for simplicity, but
// could be supported in a follow-up if needed.
bool IsUnsupportedQualifyType(const Descriptor& desc) {
  switch (desc.well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return true;
    default:
      return false;
  }
}

CelValue MessageCelValueFactory(const google::protobuf::Message* message);

inline absl::StatusOr<const google::protobuf::Message*> UnwrapMessage(
    const MessageWrapper& value, absl::string_view op) {
  if (!value.HasFullProto() || value.message_ptr() == nullptr) {
    return absl::InternalError(
        absl::StrCat(op, " called on non-message type."));
  }
  return cel::internal::down_cast<const google::protobuf::Message*>(value.message_ptr());
}

inline absl::StatusOr<google::protobuf::Message*> UnwrapMessage(
    const MessageWrapper::Builder& value, absl::string_view op) {
  if (!value.HasFullProto() || value.message_ptr() == nullptr) {
    return absl::InternalError(
        absl::StrCat(op, " called on non-message type."));
  }
  return cel::internal::down_cast<google::protobuf::Message*>(value.message_ptr());
}

bool ProtoEquals(const google::protobuf::Message& m1, const google::protobuf::Message& m2) {
  // Equality behavior is undefined for message differencer if input messages
  // have different descriptors. For CEL just return false.
  if (m1.GetDescriptor() != m2.GetDescriptor()) {
    return false;
  }
  return google::protobuf::util::MessageDifferencer::Equals(m1, m2);
}

// Implements CEL's notion of field presence for protobuf.
// Assumes all arguments non-null.
bool CelFieldIsPresent(const google::protobuf::Message* message,
                       const google::protobuf::FieldDescriptor* field_desc,
                       const google::protobuf::Reflection* reflection) {
  if (field_desc->is_map()) {
    // When the map field appears in a has(msg.map_field) expression, the map
    // is considered 'present' when it is non-empty. Since maps are repeated
    // fields they don't participate with standard proto presence testing since
    // the repeated field is always at least empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  if (field_desc->is_repeated()) {
    // When the list field appears in a has(msg.list_field) expression, the list
    // is considered 'present' when it is non-empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  // Standard proto presence test for non-repeated fields.
  return reflection->HasField(*message, field_desc);
}

// Shared implementation for HasField.
// Handles list or map specific behavior before calling reflection helpers.
absl::StatusOr<bool> HasFieldImpl(const google::protobuf::Message* message,
                                  const google::protobuf::Descriptor* descriptor,
                                  absl::string_view field_name) {
  ABSL_ASSERT(descriptor == message->GetDescriptor());
  const Reflection* reflection = message->GetReflection();
  const FieldDescriptor* field_desc = descriptor->FindFieldByName(field_name);
  if (field_desc == nullptr && reflection != nullptr) {
    // Search to see whether the field name is referring to an extension.
    field_desc = reflection->FindKnownExtensionByName(field_name);
  }
  if (field_desc == nullptr) {
    return absl::NotFoundError(absl::StrCat("no_such_field : ", field_name));
  }

  if (reflection == nullptr) {
    return absl::FailedPreconditionError(
        "google::protobuf::Reflection unavailble in CEL field access.");
  }
  return CelFieldIsPresent(message, field_desc, reflection);
}

absl::StatusOr<CelValue> CreateCelValueFromField(
    const google::protobuf::Message* message, const google::protobuf::FieldDescriptor* field_desc,
    ProtoWrapperTypeOptions unboxing_option, google::protobuf::Arena* arena) {
  if (field_desc->is_map()) {
    auto* map = google::protobuf::Arena::Create<internal::FieldBackedMapImpl>(
        arena, message, field_desc, &MessageCelValueFactory, arena);

    return CelValue::CreateMap(map);
  }
  if (field_desc->is_repeated()) {
    auto* list = google::protobuf::Arena::Create<internal::FieldBackedListImpl>(
        arena, message, field_desc, &MessageCelValueFactory, arena);
    return CelValue::CreateList(list);
  }

  CEL_ASSIGN_OR_RETURN(
      CelValue result,
      internal::CreateValueFromSingleField(message, field_desc, unboxing_option,
                                           &MessageCelValueFactory, arena));
  return result;
}

// Shared implementation for GetField.
// Handles list or map specific behavior before calling reflection helpers.
absl::StatusOr<CelValue> GetFieldImpl(const google::protobuf::Message* message,
                                      const google::protobuf::Descriptor* descriptor,
                                      absl::string_view field_name,
                                      ProtoWrapperTypeOptions unboxing_option,
                                      cel::MemoryManagerRef memory_manager) {
  ABSL_ASSERT(descriptor == message->GetDescriptor());
  const Reflection* reflection = message->GetReflection();
  const FieldDescriptor* field_desc = descriptor->FindFieldByName(field_name);
  if (field_desc == nullptr && reflection != nullptr) {
    std::string ext_name(field_name);
    field_desc = reflection->FindKnownExtensionByName(ext_name);
  }
  if (field_desc == nullptr) {
    return CreateNoSuchFieldError(memory_manager, field_name);
  }

  google::protobuf::Arena* arena = ProtoMemoryManagerArena(memory_manager);

  return CreateCelValueFromField(message, field_desc, unboxing_option, arena);
}

const google::protobuf::FieldDescriptor* GetNormalizedFieldByNumber(
    const google::protobuf::Descriptor* descriptor, const google::protobuf::Reflection* reflection,
    int field_number) {
  const google::protobuf::FieldDescriptor* field_desc =
      descriptor->FindFieldByNumber(field_number);
  if (field_desc == nullptr && reflection != nullptr) {
    field_desc = reflection->FindKnownExtensionByNumber(field_number);
  }
  return field_desc;
}

// Map entries have two field tags
// 1 - for key
// 2 - for value
constexpr int kKeyTag = 1;
constexpr int kValueTag = 2;

bool MatchesMapKeyType(const FieldDescriptor* key_desc,
                       const cel::AttributeQualifier& key) {
  switch (key_desc->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return key.kind() == cel::Kind::kBool;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return key.kind() == cel::Kind::kInt64;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      // fall through
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return key.kind() == cel::Kind::kUint64;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return key.kind() == cel::Kind::kString;

    default:
      return false;
  }
}

absl::StatusOr<absl::optional<MapValueConstRef>> LookupMapValue(
    const google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
    const google::protobuf::FieldDescriptor* field_desc,
    const google::protobuf::FieldDescriptor* key_desc,
    const cel::AttributeQualifier& key) {
  if (!MatchesMapKeyType(key_desc, key)) {
    return CreateInvalidMapKeyTypeError(key_desc->cpp_type_name());
  }

  google::protobuf::MapKey proto_key;
  switch (key_desc->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      proto_key.SetBoolValue(*key.GetBoolKey());
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
      int64_t key_value = *key.GetInt64Key();
      if (key_value > std::numeric_limits<int32_t>::max() ||
          key_value < std::numeric_limits<int32_t>::lowest()) {
        return absl::OutOfRangeError("integer overflow");
      }
      proto_key.SetInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      proto_key.SetInt64Value(*key.GetInt64Key());
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      proto_key.SetStringValue(std::string(*key.GetStringKey()));
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
      uint64_t key_value = *key.GetUint64Key();
      if (key_value > std::numeric_limits<uint32_t>::max()) {
        return absl::OutOfRangeError("unsigned integer overflow");
      }
      proto_key.SetUInt32Value(key_value);
    } break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
      proto_key.SetUInt64Value(*key.GetUint64Key());
    } break;
    default:
      return CreateInvalidMapKeyTypeError(key_desc->cpp_type_name());
  }

  // Look the value up
  MapValueConstRef value_ref;
  bool found = cel::extensions::protobuf_internal::LookupMapValue(
      *reflection, *message, *field_desc, proto_key, &value_ref);
  if (!found) {
    return absl::nullopt;
  }
  return value_ref;
}

// State machine for incrementally applying qualifiers.
//
// Reusing the state machine to represent intermediate states (as opposed to
// returning the intermediates) is more efficient for longer select chains while
// still allowing decomposition of the qualify routine.
class QualifyState {
 public:
  QualifyState(absl::Nonnull<const Message*> message,
               absl::Nonnull<const Descriptor*> descriptor,
               absl::Nonnull<const Reflection*> reflection)
      : message_(message),
        descriptor_(descriptor),
        reflection_(reflection),
        repeated_field_desc_(nullptr) {}

  QualifyState(const QualifyState&) = delete;
  QualifyState& operator=(const QualifyState&) = delete;

  absl::optional<CelValue>& result() { return result_; }

  absl::Status ApplySelectQualifier(const cel::SelectQualifier& qualifier,

                                    google::protobuf::Arena* arena) {
    return absl::visit(
        absl::Overload(
            [&](const cel::AttributeQualifier& qualifier) -> absl::Status {
              if (repeated_field_desc_ == nullptr) {
                return absl::UnimplementedError(
                    "dynamic field access on message not supported");
              }
              return ApplyAttributeQualifer(qualifier, arena);
            },
            [&](const cel::FieldSpecifier& field_specifier) -> absl::Status {
              if (repeated_field_desc_ != nullptr) {
                return absl::UnimplementedError(
                    "strong field access on container not supported");
              }
              return ApplyFieldSpecifier(field_specifier, arena);
            }),
        qualifier);
  }

  absl::StatusOr<CelValue> ApplyLastQualifierHas(
      const cel::SelectQualifier& qualifier, google::protobuf::Arena* arena) const {
    const cel::FieldSpecifier* specifier =
        absl::get_if<cel::FieldSpecifier>(&qualifier);
    return absl::visit(
        absl::Overload(
            [&](const cel::AttributeQualifier& qualifier)
                -> absl::StatusOr<CelValue> {
              if (qualifier.kind() != cel::Kind::kString ||
                  repeated_field_desc_ == nullptr ||
                  !repeated_field_desc_->is_map()) {
                return CreateErrorValue(arena,
                                        CreateNoMatchingOverloadError("has"));
              }
              return MapHas(qualifier, arena);
            },
            [&](const cel::FieldSpecifier& field_specifier)
                -> absl::StatusOr<CelValue> {
              const auto* field_desc = GetNormalizedFieldByNumber(
                  descriptor_, reflection_, specifier->number);
              if (field_desc == nullptr) {
                return CreateNoSuchFieldError(arena, specifier->name);
              }
              return CelValue::CreateBool(
                  CelFieldIsPresent(message_, field_desc, reflection_));
            }),
        qualifier);
  }

  absl::StatusOr<CelValue> ApplyLastQualifierGet(
      const cel::SelectQualifier& qualifier, google::protobuf::Arena* arena) const {
    return absl::visit(
        absl::Overload(
            [&](const cel::AttributeQualifier& attr_qualifier)
                -> absl::StatusOr<CelValue> {
              if (repeated_field_desc_ == nullptr) {
                return absl::UnimplementedError(
                    "dynamic field access on message not supported");
              }
              if (repeated_field_desc_->is_map()) {
                return ApplyLastQualifierGetMap(attr_qualifier, arena);
              }
              return ApplyLastQualifierGetList(attr_qualifier, arena);
            },
            [&](const cel::FieldSpecifier& specifier)
                -> absl::StatusOr<CelValue> {
              if (repeated_field_desc_ != nullptr) {
                return absl::UnimplementedError(
                    "strong field access on container not supported");
              }
              return ApplyLastQualifierMessageGet(specifier, arena);
            }),
        qualifier);
  }

 private:
  absl::Status ApplyFieldSpecifier(const cel::FieldSpecifier& field_specifier,
                                   google::protobuf::Arena* arena) {
    const FieldDescriptor* field_desc = GetNormalizedFieldByNumber(
        descriptor_, reflection_, field_specifier.number);
    if (field_desc == nullptr) {
      result_ = CreateNoSuchFieldError(arena, field_specifier.name);
      return absl::OkStatus();
    }

    if (field_desc->is_repeated()) {
      repeated_field_desc_ = field_desc;
      return absl::OkStatus();
    }

    if (field_desc->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE ||
        IsUnsupportedQualifyType(*field_desc->message_type())) {
      CEL_ASSIGN_OR_RETURN(
          result_,
          CreateCelValueFromField(message_, field_desc,
                                  ProtoWrapperTypeOptions::kUnsetNull, arena));

      return absl::OkStatus();
    }

    message_ = &reflection_->GetMessage(*message_, field_desc);
    descriptor_ = message_->GetDescriptor();
    reflection_ = message_->GetReflection();
    return absl::OkStatus();
  }

  absl::StatusOr<int> CheckListIndex(
      const cel::AttributeQualifier& qualifier) const {
    if (qualifier.kind() != cel::Kind::kInt64) {
      return CreateNoMatchingOverloadError(cel::builtin::kIndex);
    }

    int index = *qualifier.GetInt64Key();
    int size = reflection_->FieldSize(*message_, repeated_field_desc_);
    if (index < 0 || index >= size) {
      return absl::InvalidArgumentError(
          absl::StrCat("index out of bounds: index=", index, " size=", size));
    }
    return index;
  }

  absl::Status ApplyAttributeQualifierList(
      const cel::AttributeQualifier& qualifier, google::protobuf::Arena* arena) {
    ABSL_DCHECK_NE(repeated_field_desc_, nullptr);
    ABSL_DCHECK(!repeated_field_desc_->is_map());
    ABSL_DCHECK_EQ(repeated_field_desc_->cpp_type(),
                   FieldDescriptor::CPPTYPE_MESSAGE);

    auto index_or = CheckListIndex(qualifier);
    if (!index_or.ok()) {
      result_ = CreateErrorValue(arena, std::move(index_or).status());
      return absl::OkStatus();
    }

    if (IsUnsupportedQualifyType(*repeated_field_desc_->message_type())) {
      CEL_ASSIGN_OR_RETURN(result_,
                           internal::CreateValueFromRepeatedField(
                               message_, repeated_field_desc_, *index_or,
                               &MessageCelValueFactory, arena));
      return absl::OkStatus();
    }

    message_ = &reflection_->GetRepeatedMessage(*message_, repeated_field_desc_,
                                                *index_or);
    descriptor_ = message_->GetDescriptor();
    reflection_ = message_->GetReflection();
    repeated_field_desc_ = nullptr;
    return absl::OkStatus();
  }

  absl::StatusOr<MapValueConstRef> CheckMapIndex(
      const cel::AttributeQualifier& qualifier) const {
    const auto* key_desc =
        repeated_field_desc_->message_type()->FindFieldByNumber(kKeyTag);

    CEL_ASSIGN_OR_RETURN(
        absl::optional<MapValueConstRef> value_ref,
        LookupMapValue(message_, reflection_, repeated_field_desc_, key_desc,
                       qualifier));

    if (!value_ref.has_value()) {
      return CreateNoSuchKeyError("");
    }
    return std::move(value_ref).value();
  }

  absl::Status ApplyAttributeQualifierMap(
      const cel::AttributeQualifier& qualifier, google::protobuf::Arena* arena) {
    ABSL_DCHECK_NE(repeated_field_desc_, nullptr);
    ABSL_DCHECK(repeated_field_desc_->is_map());
    ABSL_DCHECK_EQ(repeated_field_desc_->cpp_type(),
                   FieldDescriptor::CPPTYPE_MESSAGE);

    absl::StatusOr<MapValueConstRef> value_ref = CheckMapIndex(qualifier);
    if (!value_ref.ok()) {
      result_ = CreateErrorValue(arena, std::move(value_ref).status());
      return absl::OkStatus();
    }

    const auto* value_desc =
        repeated_field_desc_->message_type()->FindFieldByNumber(kValueTag);

    if (value_desc->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE ||
        IsUnsupportedQualifyType(*value_desc->message_type())) {
      CEL_ASSIGN_OR_RETURN(result_, internal::CreateValueFromMapValue(
                                        message_, value_desc, &(*value_ref),
                                        &MessageCelValueFactory, arena));
      return absl::OkStatus();
    }

    message_ = &(value_ref->GetMessageValue());
    descriptor_ = message_->GetDescriptor();
    reflection_ = message_->GetReflection();
    repeated_field_desc_ = nullptr;
    return absl::OkStatus();
  }

  absl::Status ApplyAttributeQualifer(const cel::AttributeQualifier& qualifier,

                                      google::protobuf::Arena* arena) {
    ABSL_DCHECK_NE(repeated_field_desc_, nullptr);
    if (repeated_field_desc_->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE) {
      return absl::InternalError("Unexpected qualify intermediate type");
    }
    if (repeated_field_desc_->is_map()) {
      return ApplyAttributeQualifierMap(qualifier, arena);
    }  // else simple repeated
    return ApplyAttributeQualifierList(qualifier, arena);
  }

  absl::StatusOr<CelValue> MapHas(const cel::AttributeQualifier& key,
                                  google::protobuf::Arena* arena) const {
    const auto* key_desc =
        repeated_field_desc_->message_type()->FindFieldByNumber(kKeyTag);

    absl::StatusOr<absl::optional<MapValueConstRef>> value_ref = LookupMapValue(
        message_, reflection_, repeated_field_desc_, key_desc, key);

    if (!value_ref.ok()) {
      return CreateErrorValue(arena, std::move(value_ref).status());
    }

    return CelValue::CreateBool(value_ref->has_value());
  }

  absl::StatusOr<CelValue> ApplyLastQualifierMessageGet(
      const cel::FieldSpecifier& specifier, google::protobuf::Arena* arena) const {
    const auto* field_desc =
        GetNormalizedFieldByNumber(descriptor_, reflection_, specifier.number);
    if (field_desc == nullptr) {
      return CreateNoSuchFieldError(arena, specifier.name);
    }
    return CreateCelValueFromField(message_, field_desc,
                                   ProtoWrapperTypeOptions::kUnsetNull, arena);
  }

  absl::StatusOr<CelValue> ApplyLastQualifierGetList(
      const cel::AttributeQualifier& qualifier, google::protobuf::Arena* arena) const {
    ABSL_DCHECK(!repeated_field_desc_->is_map());

    absl::StatusOr<int> index = CheckListIndex(qualifier);
    if (!index.ok()) {
      return CreateErrorValue(arena, std::move(index).status());
    }

    return internal::CreateValueFromRepeatedField(
        message_, repeated_field_desc_, *index, &MessageCelValueFactory, arena);
  }

  absl::StatusOr<CelValue> ApplyLastQualifierGetMap(
      const cel::AttributeQualifier& qualifier, google::protobuf::Arena* arena) const {
    ABSL_DCHECK(repeated_field_desc_->is_map());

    absl::StatusOr<MapValueConstRef> value_ref = CheckMapIndex(qualifier);

    if (!value_ref.ok()) {
      return CreateErrorValue(arena, std::move(value_ref).status());
    }

    const auto* value_desc =
        repeated_field_desc_->message_type()->FindFieldByNumber(kValueTag);

    return internal::CreateValueFromMapValue(
        message_, value_desc, &(*value_ref), &MessageCelValueFactory, arena);
  }

  absl::optional<CelValue> result_;

  absl::Nonnull<const Message*> message_;
  absl::Nonnull<const Descriptor*> descriptor_;
  absl::Nonnull<const Reflection*> reflection_;
  absl::Nullable<const FieldDescriptor*> repeated_field_desc_;
};

absl::StatusOr<LegacyQualifyResult> QualifyImpl(
    const google::protobuf::Message* message, const google::protobuf::Descriptor* descriptor,
    absl::Span<const cel::SelectQualifier> path, bool presence_test,
    cel::MemoryManagerRef memory_manager) {
  google::protobuf::Arena* arena = ProtoMemoryManagerArena(memory_manager);
  ABSL_DCHECK(descriptor == message->GetDescriptor());
  QualifyState qualify_state(message, descriptor, message->GetReflection());

  for (int i = 0; i < path.size() - 1; i++) {
    const auto& qualifier = path.at(i);
    CEL_RETURN_IF_ERROR(qualify_state.ApplySelectQualifier(qualifier, arena));
    if (qualify_state.result().has_value()) {
      LegacyQualifyResult result;
      result.value = std::move(qualify_state.result()).value();
      result.qualifier_count = result.value.IsError() ? -1 : i + 1;
      return result;
    }
  }

  const auto& last_qualifier = path.back();
  LegacyQualifyResult result;
  result.qualifier_count = -1;

  if (presence_test) {
    CEL_ASSIGN_OR_RETURN(result.value, qualify_state.ApplyLastQualifierHas(
                                           last_qualifier, arena));
  } else {
    CEL_ASSIGN_OR_RETURN(result.value, qualify_state.ApplyLastQualifierGet(
                                           last_qualifier, arena));
  }
  return result;
}

std::vector<absl::string_view> ListFieldsImpl(
    const CelValue::MessageWrapper& instance) {
  if (instance.message_ptr() == nullptr) {
    return std::vector<absl::string_view>();
  }
  const auto* message =
      cel::internal::down_cast<const google::protobuf::Message*>(instance.message_ptr());
  const auto* reflect = message->GetReflection();
  std::vector<const google::protobuf::FieldDescriptor*> fields;
  reflect->ListFields(*message, &fields);
  std::vector<absl::string_view> field_names;
  field_names.reserve(fields.size());
  for (const auto* field : fields) {
    field_names.emplace_back(field->name());
  }
  return field_names;
}

class DucktypedMessageAdapter : public LegacyTypeAccessApis,
                                public LegacyTypeMutationApis,
                                public LegacyTypeInfoApis {
 public:
  // Implement field access APIs.
  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(value, "HasField"));
    return HasFieldImpl(message, message->GetDescriptor(), field_name);
  }

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManagerRef memory_manager) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(instance, "GetField"));
    return GetFieldImpl(message, message->GetDescriptor(), field_name,
                        unboxing_option, memory_manager);
  }

  absl::StatusOr<LegacyTypeAccessApis::LegacyQualifyResult> Qualify(
      absl::Span<const cel::SelectQualifier> qualifiers,
      const CelValue::MessageWrapper& instance, bool presence_test,
      cel::MemoryManagerRef memory_manager) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(instance, "Qualify"));

    return QualifyImpl(message, message->GetDescriptor(), qualifiers,
                       presence_test, memory_manager);
  }

  bool IsEqualTo(
      const CelValue::MessageWrapper& instance,
      const CelValue::MessageWrapper& other_instance) const override {
    absl::StatusOr<const google::protobuf::Message*> lhs =
        UnwrapMessage(instance, "IsEqualTo");
    absl::StatusOr<const google::protobuf::Message*> rhs =
        UnwrapMessage(other_instance, "IsEqualTo");
    if (!lhs.ok() || !rhs.ok()) {
      // Treat this as though the underlying types are different, just return
      // false.
      return false;
    }
    return ProtoEquals(**lhs, **rhs);
  }

  // Implement TypeInfo Apis
  const std::string& GetTypename(
      const MessageWrapper& wrapped_message) const override {
    if (!wrapped_message.HasFullProto() ||
        wrapped_message.message_ptr() == nullptr) {
      return UnsupportedTypeName();
    }
    auto* message = cel::internal::down_cast<const google::protobuf::Message*>(
        wrapped_message.message_ptr());
    return message->GetDescriptor()->full_name();
  }

  std::string DebugString(
      const MessageWrapper& wrapped_message) const override {
    if (!wrapped_message.HasFullProto() ||
        wrapped_message.message_ptr() == nullptr) {
      return UnsupportedTypeName();
    }
    auto* message = cel::internal::down_cast<const google::protobuf::Message*>(
        wrapped_message.message_ptr());
    return message->ShortDebugString();
  }

  bool DefinesField(absl::string_view field_name) const override {
    // Pretend all our fields exist. Real errors will be returned from field
    // getters and setters.
    return true;
  }

  absl::StatusOr<CelValue::MessageWrapper::Builder> NewInstance(
      cel::MemoryManagerRef memory_manager) const override {
    return absl::UnimplementedError("NewInstance is not implemented");
  }

  absl::StatusOr<CelValue> AdaptFromWellKnownType(
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder instance) const override {
    if (!instance.HasFullProto() || instance.message_ptr() == nullptr) {
      return absl::UnimplementedError(
          "MessageLite is not supported, descriptor is required");
    }
    return ProtoMessageTypeAdapter(
               cel::internal::down_cast<const google::protobuf::Message*>(
                   instance.message_ptr())
                   ->GetDescriptor(),
               nullptr)
        .AdaptFromWellKnownType(memory_manager, instance);
  }

  absl::Status SetField(
      absl::string_view field_name, const CelValue& value,
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder& instance) const override {
    if (!instance.HasFullProto() || instance.message_ptr() == nullptr) {
      return absl::UnimplementedError(
          "MessageLite is not supported, descriptor is required");
    }
    return ProtoMessageTypeAdapter(
               cel::internal::down_cast<const google::protobuf::Message*>(
                   instance.message_ptr())
                   ->GetDescriptor(),
               nullptr)
        .SetField(field_name, value, memory_manager, instance);
  }

  std::vector<absl::string_view> ListFields(
      const CelValue::MessageWrapper& instance) const override {
    return ListFieldsImpl(instance);
  }

  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const override {
    return this;
  }

  const LegacyTypeMutationApis* GetMutationApis(
      const MessageWrapper& wrapped_message) const override {
    return this;
  }

  static const DucktypedMessageAdapter& GetSingleton() {
    static absl::NoDestructor<DucktypedMessageAdapter> instance;
    return *instance;
  }
};

CelValue MessageCelValueFactory(const google::protobuf::Message* message) {
  return CelValue::CreateMessageWrapper(
      MessageWrapper(message, &DucktypedMessageAdapter::GetSingleton()));
}

}  // namespace

std::string ProtoMessageTypeAdapter::DebugString(
    const MessageWrapper& wrapped_message) const {
  if (!wrapped_message.HasFullProto() ||
      wrapped_message.message_ptr() == nullptr) {
    return UnsupportedTypeName();
  }
  auto* message = cel::internal::down_cast<const google::protobuf::Message*>(
      wrapped_message.message_ptr());
  return message->ShortDebugString();
}

const std::string& ProtoMessageTypeAdapter::GetTypename(
    const MessageWrapper& wrapped_message) const {
  return descriptor_->full_name();
}

const LegacyTypeMutationApis* ProtoMessageTypeAdapter::GetMutationApis(
    const MessageWrapper& wrapped_message) const {
  // Defer checks for misuse on wrong message kind in the accessor calls.
  return this;
}

const LegacyTypeAccessApis* ProtoMessageTypeAdapter::GetAccessApis(
    const MessageWrapper& wrapped_message) const {
  // Defer checks for misuse on wrong message kind in the builder calls.
  return this;
}

absl::optional<LegacyTypeInfoApis::FieldDescription>
ProtoMessageTypeAdapter::FindFieldByName(absl::string_view field_name) const {
  if (descriptor_ == nullptr) {
    return absl::nullopt;
  }

  const google::protobuf::FieldDescriptor* field_descriptor =
      descriptor_->FindFieldByName(field_name);

  if (field_descriptor == nullptr) {
    return absl::nullopt;
  }

  return LegacyTypeInfoApis::FieldDescription{field_descriptor->number(),
                                              field_descriptor->name()};
}

absl::Status ProtoMessageTypeAdapter::ValidateSetFieldOp(
    bool assertion, absl::string_view field, absl::string_view detail) const {
  if (!assertion) {
    return absl::InvalidArgumentError(
        absl::Substitute("SetField failed on message $0, field '$1': $2",
                         descriptor_->full_name(), field, detail));
  }
  return absl::OkStatus();
}

absl::StatusOr<CelValue::MessageWrapper::Builder>
ProtoMessageTypeAdapter::NewInstance(
    cel::MemoryManagerRef memory_manager) const {
  if (message_factory_ == nullptr) {
    return absl::UnimplementedError(
        absl::StrCat("Cannot create message ", descriptor_->name()));
  }

  // This implementation requires arena-backed memory manager.
  google::protobuf::Arena* arena = ProtoMemoryManagerArena(memory_manager);
  const Message* prototype = message_factory_->GetPrototype(descriptor_);

  Message* msg = (prototype != nullptr) ? prototype->New(arena) : nullptr;

  if (msg == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to create message ", descriptor_->name()));
  }
  return MessageWrapper::Builder(msg);
}

bool ProtoMessageTypeAdapter::DefinesField(absl::string_view field_name) const {
  return descriptor_->FindFieldByName(field_name) != nullptr;
}

absl::StatusOr<bool> ProtoMessageTypeAdapter::HasField(
    absl::string_view field_name, const CelValue::MessageWrapper& value) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(value, "HasField"));
  return HasFieldImpl(message, descriptor_, field_name);
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::GetField(
    absl::string_view field_name, const CelValue::MessageWrapper& instance,
    ProtoWrapperTypeOptions unboxing_option,
    cel::MemoryManagerRef memory_manager) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(instance, "GetField"));

  return GetFieldImpl(message, descriptor_, field_name, unboxing_option,
                      memory_manager);
}

absl::StatusOr<LegacyTypeAccessApis::LegacyQualifyResult>
ProtoMessageTypeAdapter::Qualify(
    absl::Span<const cel::SelectQualifier> qualifiers,
    const CelValue::MessageWrapper& instance, bool presence_test,
    cel::MemoryManagerRef memory_manager) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(instance, "Qualify"));

  return QualifyImpl(message, descriptor_, qualifiers, presence_test,
                     memory_manager);
}

absl::Status ProtoMessageTypeAdapter::SetField(
    const google::protobuf::FieldDescriptor* field, const CelValue& value,
    google::protobuf::Arena* arena, google::protobuf::Message* message) const {
  if (field->is_map()) {
    constexpr int kKeyField = 1;
    constexpr int kValueField = 2;

    const CelMap* cel_map;
    CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
        value.GetValue<const CelMap*>(&cel_map) && cel_map != nullptr,
        field->name(), "value is not CelMap"));

    auto entry_descriptor = field->message_type();

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(entry_descriptor != nullptr, field->name(),
                           "failed to find map entry descriptor"));
    auto key_field_descriptor = entry_descriptor->FindFieldByNumber(kKeyField);
    auto value_field_descriptor =
        entry_descriptor->FindFieldByNumber(kValueField);

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(key_field_descriptor != nullptr, field->name(),
                           "failed to find key field descriptor"));

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(value_field_descriptor != nullptr, field->name(),
                           "failed to find value field descriptor"));

    CEL_ASSIGN_OR_RETURN(const CelList* key_list, cel_map->ListKeys(arena));
    for (int i = 0; i < key_list->size(); i++) {
      CelValue key = (*key_list).Get(arena, i);

      auto value = (*cel_map).Get(arena, key);
      CEL_RETURN_IF_ERROR(ValidateSetFieldOp(value.has_value(), field->name(),
                                             "error serializing CelMap"));
      Message* entry_msg = message->GetReflection()->AddMessage(message, field);
      CEL_RETURN_IF_ERROR(internal::SetValueToSingleField(
          key, key_field_descriptor, entry_msg, arena));
      CEL_RETURN_IF_ERROR(internal::SetValueToSingleField(
          value.value(), value_field_descriptor, entry_msg, arena));
    }

  } else if (field->is_repeated()) {
    const CelList* cel_list;
    CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
        value.GetValue<const CelList*>(&cel_list) && cel_list != nullptr,
        field->name(), "expected CelList value"));

    for (int i = 0; i < cel_list->size(); i++) {
      CEL_RETURN_IF_ERROR(internal::AddValueToRepeatedField(
          (*cel_list).Get(arena, i), field, message, arena));
    }
  } else {
    CEL_RETURN_IF_ERROR(
        internal::SetValueToSingleField(value, field, message, arena));
  }
  return absl::OkStatus();
}

absl::Status ProtoMessageTypeAdapter::SetField(
    absl::string_view field_name, const CelValue& value,
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder& instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManagerArena(memory_manager);

  CEL_ASSIGN_OR_RETURN(google::protobuf::Message * mutable_message,
                       UnwrapMessage(instance, "SetField"));

  const google::protobuf::FieldDescriptor* field_descriptor =
      descriptor_->FindFieldByName(field_name);
  CEL_RETURN_IF_ERROR(
      ValidateSetFieldOp(field_descriptor != nullptr, field_name, "not found"));

  return SetField(field_descriptor, value, arena, mutable_message);
}

absl::Status ProtoMessageTypeAdapter::SetFieldByNumber(
    int64_t field_number, const CelValue& value,
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder& instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManagerArena(memory_manager);

  CEL_ASSIGN_OR_RETURN(google::protobuf::Message * mutable_message,
                       UnwrapMessage(instance, "SetField"));

  const google::protobuf::FieldDescriptor* field_descriptor =
      descriptor_->FindFieldByNumber(field_number);
  CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
      field_descriptor != nullptr, absl::StrCat(field_number), "not found"));

  return SetField(field_descriptor, value, arena, mutable_message);
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::AdaptFromWellKnownType(
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManagerArena(memory_manager);
  CEL_ASSIGN_OR_RETURN(google::protobuf::Message * message,
                       UnwrapMessage(instance, "AdaptFromWellKnownType"));
  return internal::UnwrapMessageToValue(message, &MessageCelValueFactory,
                                        arena);
}

bool ProtoMessageTypeAdapter::IsEqualTo(
    const CelValue::MessageWrapper& instance,
    const CelValue::MessageWrapper& other_instance) const {
  absl::StatusOr<const google::protobuf::Message*> lhs =
      UnwrapMessage(instance, "IsEqualTo");
  absl::StatusOr<const google::protobuf::Message*> rhs =
      UnwrapMessage(other_instance, "IsEqualTo");
  if (!lhs.ok() || !rhs.ok()) {
    // Treat this as though the underlying types are different, just return
    // false.
    return false;
  }
  return ProtoEquals(**lhs, **rhs);
}

std::vector<absl::string_view> ProtoMessageTypeAdapter::ListFields(
    const CelValue::MessageWrapper& instance) const {
  return ListFieldsImpl(instance);
}

const LegacyTypeInfoApis& GetGenericProtoTypeInfoInstance() {
  return DucktypedMessageAdapter::GetSingleton();
}

}  // namespace google::api::expr::runtime
