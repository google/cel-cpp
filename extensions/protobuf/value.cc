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

#include "extensions/protobuf/value.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/time/time.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "extensions/protobuf/internal/duration.h"
#include "extensions/protobuf/internal/reflection.h"
#include "extensions/protobuf/internal/timestamp.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/status_macros.h"

namespace cel::extensions {

namespace {

void AppendJsonValueDebugString(std::string& out,
                                const google::protobuf::Value& value);

void AppendJsonValueDebugString(std::string& out,
                                const google::protobuf::ListValue& value) {
  out.push_back('[');
  auto current = value.values().begin();
  if (current != value.values().end()) {
    AppendJsonValueDebugString(out, *current++);
  }
  for (; current != value.values().end(); ++current) {
    out.append(", ");
    AppendJsonValueDebugString(out, *current);
  }
  out.push_back(']');
}

void AppendJsonValueDebugString(std::string& out,
                                const google::protobuf::Struct& value) {
  out.push_back('{');
  std::vector<absl::string_view> field_names;
  field_names.reserve(value.fields_size());
  for (const auto& field : value.fields()) {
    field_names.push_back(field.first);
  }
  std::stable_sort(field_names.begin(), field_names.end());
  auto current = field_names.cbegin();
  if (current != field_names.cend()) {
    out.append(StringValue::DebugString(*current));
    out.append(": ");
    AppendJsonValueDebugString(out, value.fields().at(*current++));
    for (; current != field_names.cend(); ++current) {
      out.append(", ");
      out.append(StringValue::DebugString(*current));
      out.append(": ");
      AppendJsonValueDebugString(out, value.fields().at(*current));
    }
  }
  out.push_back('}');
}

void AppendJsonValueDebugString(std::string& out,
                                const google::protobuf::Value& value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      out.append(NullValue::DebugString());
      break;
    case google::protobuf::Value::kBoolValue:
      out.append(BoolValue::DebugString(value.bool_value()));
      break;
    case google::protobuf::Value::kNumberValue:
      out.append(DoubleValue::DebugString(value.number_value()));
      break;
    case google::protobuf::Value::kStringValue:
      out.append(StringValue::DebugString(value.string_value()));
      break;
    case google::protobuf::Value::kListValue:
      AppendJsonValueDebugString(out, value.list_value());
      break;
    case google::protobuf::Value::kStructValue:
      AppendJsonValueDebugString(out, value.struct_value());
      break;
    default:
      break;
  }
}

template <typename R>
absl::StatusOr<Handle<Value>> CreateMemberJsonValue(
    ValueFactory& value_factory, const google::protobuf::ListValue& value,
    Owner<R> reference);

template <typename R>
absl::StatusOr<Handle<Value>> CreateMemberJsonValue(
    ValueFactory& value_factory, const google::protobuf::Struct& value,
    Owner<R> reference);

template <typename HandleFromThis>
absl::StatusOr<Handle<Value>> CreateMemberJsonValue(
    ValueFactory& value_factory, const google::protobuf::Value& value,
    HandleFromThis&& owner_from_this) {
  switch (value.kind_case()) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValue:
      return value_factory.CreateBoolValue(value.bool_value());
    case google::protobuf::Value::kNumberValue:
      return value_factory.CreateDoubleValue(value.number_value());
    case google::protobuf::Value::kStringValue:
      return value_factory.CreateBorrowedStringValue(owner_from_this(),
                                                     value.string_value());
    case google::protobuf::Value::kListValue:
      return CreateMemberJsonValue(value_factory, value.list_value(),
                                   owner_from_this());
    case google::protobuf::Value::kStructValue:
      return CreateMemberJsonValue(value_factory, value.struct_value(),
                                   owner_from_this());
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected google.protobuf.Value kind: %d", value.kind_case()));
  }
}

class StaticProtoJsonListValue : public CEL_LIST_VALUE_CLASS {
 public:
  StaticProtoJsonListValue(Handle<ListType> type,
                           google::protobuf::ListValue value)
      : CEL_LIST_VALUE_CLASS(std::move(type)), value_(std::move(value)) {}

  std::string DebugString() const final {
    std::string out;
    AppendJsonValueDebugString(out, value_);
    return out;
  }

  size_t size() const final { return value_.values_size(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return CreateMemberJsonValue(
        context.value_factory(), value_.values(index),
        [this]() mutable { return owner_from_this(); });
  }

 private:
  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<StaticProtoJsonListValue>();
  }

  const google::protobuf::ListValue value_;
};

class ArenaStaticProtoJsonListValue : public CEL_LIST_VALUE_CLASS {
 public:
  ArenaStaticProtoJsonListValue(Handle<ListType> type,
                                const google::protobuf::ListValue* value)
      : CEL_LIST_VALUE_CLASS(std::move(type)), value_(value) {}

  std::string DebugString() const final {
    std::string out;
    AppendJsonValueDebugString(out, *value_);
    return out;
  }

  size_t size() const final { return value_->values_size(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return CreateMemberJsonValue(
        context.value_factory(), value_->values(index),
        [this]() mutable { return owner_from_this(); });
  }

 private:
  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ArenaStaticProtoJsonListValue>();
  }

  const google::protobuf::ListValue* const value_;
};

class StaticProtoJsonMapKeysListValue : public CEL_LIST_VALUE_CLASS {
 public:
  StaticProtoJsonMapKeysListValue(
      Handle<ListType> type, const google::protobuf::Struct* value,
      std::vector<absl::string_view, Allocator<absl::string_view>> field_names)
      : CEL_LIST_VALUE_CLASS(std::move(type)),
        value_(value),
        field_names_(std::move(field_names)) {}

  std::string DebugString() const final {
    std::string out;
    AppendJsonValueDebugString(out, *value_);
    return out;
  }

  size_t size() const final { return field_names_.size(); }

  absl::StatusOr<Handle<Value>> Get(const GetContext& context,
                                    size_t index) const final {
    return CreateMemberJsonValue(
        context.value_factory(), value_->fields().at(field_names_[index]),
        [this]() mutable { return owner_from_this(); });
  }

 private:
  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<StaticProtoJsonMapKeysListValue>();
  }

  const google::protobuf::Struct* const value_;
  std::vector<absl::string_view, Allocator<absl::string_view>> field_names_;
};

class StaticProtoJsonMapValue : public CEL_MAP_VALUE_CLASS {
 public:
  StaticProtoJsonMapValue(Handle<MapType> type, google::protobuf::Struct value)
      : CEL_MAP_VALUE_CLASS(std::move(type)), value_(std::move(value)) {}

  std::string DebugString() const final {
    std::string out;
    AppendJsonValueDebugString(out, value_);
    return out;
  }

  size_t size() const final { return value_.fields_size(); }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      const GetContext& context, const Handle<Value>& key) const final {
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError("expected key to be string value");
    }
    auto it = value_.fields().find(key->As<StringValue>().ToString());
    if (it == value_.fields().end()) {
      return absl::nullopt;
    }
    return CreateMemberJsonValue(
        context.value_factory(), it->second,
        [this]() mutable { return owner_from_this(); });
  }

  absl::StatusOr<bool> Has(const HasContext& context,
                           const Handle<Value>& key) const final {
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError("expected key to be string value");
    }
    return value_.fields().contains(key->As<StringValue>().ToString());
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      const ListKeysContext& context) const final {
    CEL_ASSIGN_OR_RETURN(
        auto list_type,
        context.value_factory().type_factory().CreateListType(type()->key()));
    std::vector<absl::string_view, Allocator<absl::string_view>> field_names(
        Allocator<absl::string_view>(context.value_factory().memory_manager()));
    field_names.reserve(value_.fields_size());
    for (const auto& field : value_.fields()) {
      field_names.push_back(field.first);
    }
    return context.value_factory()
        .CreateBorrowedListValue<StaticProtoJsonMapKeysListValue>(
            owner_from_this(), std::move(list_type), &value_,
            std::move(field_names));
  }

 private:
  // Called by CEL_IMPLEMENT_MAP_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const override {
    return internal::TypeId<StaticProtoJsonMapValue>();
  }

  const google::protobuf::Struct value_;
};

class ArenaStaticProtoJsonMapValue : public CEL_MAP_VALUE_CLASS {
 public:
  ArenaStaticProtoJsonMapValue(Handle<MapType> type,
                               const google::protobuf::Struct* value)
      : CEL_MAP_VALUE_CLASS(std::move(type)), value_(value) {}

  std::string DebugString() const final {
    std::string out;
    AppendJsonValueDebugString(out, *value_);
    return out;
  }

  size_t size() const final { return value_->fields_size(); }

  absl::StatusOr<absl::optional<Handle<Value>>> Get(
      const GetContext& context, const Handle<Value>& key) const final {
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError("expected key to be string value");
    }
    auto it = value_->fields().find(key->As<StringValue>().ToString());
    if (it == value_->fields().end()) {
      return absl::nullopt;
    }
    return CreateMemberJsonValue(
        context.value_factory(), it->second,
        [this]() mutable { return owner_from_this(); });
  }

  absl::StatusOr<bool> Has(const HasContext& context,
                           const Handle<Value>& key) const final {
    if (!key->Is<StringValue>()) {
      return absl::InvalidArgumentError("expected key to be string value");
    }
    return value_->fields().contains(key->As<StringValue>().ToString());
  }

  absl::StatusOr<Handle<ListValue>> ListKeys(
      const ListKeysContext& context) const final {
    CEL_ASSIGN_OR_RETURN(
        auto list_type,
        context.value_factory().type_factory().CreateListType(type()->key()));
    std::vector<absl::string_view, Allocator<absl::string_view>> field_names(
        Allocator<absl::string_view>(context.value_factory().memory_manager()));
    field_names.reserve(value_->fields_size());
    for (const auto& field : value_->fields()) {
      field_names.push_back(field.first);
    }
    return context.value_factory()
        .CreateBorrowedListValue<StaticProtoJsonMapKeysListValue>(
            owner_from_this(), std::move(list_type), value_,
            std::move(field_names));
  }

 private:
  // Called by CEL_IMPLEMENT_MAP_VALUE() and Is() to perform type checking.
  internal::TypeInfo TypeId() const final {
    return internal::TypeId<ArenaStaticProtoJsonMapValue>();
  }

  const google::protobuf::Struct* const value_;
};

template <typename R>
absl::StatusOr<Handle<Value>> CreateMemberJsonValue(
    ValueFactory& value_factory, const google::protobuf::ListValue& value,
    Owner<R> reference) {
  CEL_ASSIGN_OR_RETURN(auto list_type,
                       value_factory.type_factory().CreateListType(
                           value_factory.type_factory().GetDynType()));
  return value_factory.CreateBorrowedListValue<ArenaStaticProtoJsonListValue>(
      std::move(reference), std::move(list_type), &value);
}

template <typename R>
absl::StatusOr<Handle<Value>> CreateMemberJsonValue(
    ValueFactory& value_factory, const google::protobuf::Struct& value,
    Owner<R> reference) {
  CEL_ASSIGN_OR_RETURN(auto map_type,
                       value_factory.type_factory().CreateMapType(
                           value_factory.type_factory().GetStringType(),
                           value_factory.type_factory().GetDynType()));
  return value_factory.CreateBorrowedMapValue<ArenaStaticProtoJsonMapValue>(
      std::move(reference), std::move(map_type), &value);
}

}  // namespace

absl::StatusOr<Handle<ListValue>> ProtoValue::Create(
    ValueFactory& value_factory, google::protobuf::ListValue value) {
  CEL_ASSIGN_OR_RETURN(auto list_type,
                       value_factory.type_factory().CreateListType(
                           value_factory.type_factory().GetDynType()));
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::ListValue>(arena);
      *arena_value = std::move(value);
      return value_factory.CreateListValue<ArenaStaticProtoJsonListValue>(
          std::move(list_type), arena_value);
    }
  }
  return value_factory.CreateListValue<StaticProtoJsonListValue>(
      std::move(list_type), std::move(value));
}

absl::StatusOr<Handle<ListValue>> ProtoValue::Create(
    ValueFactory& value_factory,
    std::unique_ptr<google::protobuf::ListValue> value) {
  CEL_ASSIGN_OR_RETURN(auto list_type,
                       value_factory.type_factory().CreateListType(
                           value_factory.type_factory().GetDynType()));
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::ListValue>(arena);
      arena_value->Swap(value.get());
      return value_factory.CreateListValue<ArenaStaticProtoJsonListValue>(
          std::move(list_type), arena_value);
    }
  }
  return value_factory.CreateListValue<StaticProtoJsonListValue>(
      std::move(list_type), std::move(*value));
}

absl::StatusOr<Handle<ListValue>> ProtoValue::CreateBorrowed(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::ListValue& value) {
  CEL_ASSIGN_OR_RETURN(auto list_type,
                       value_factory.type_factory().CreateListType(
                           value_factory.type_factory().GetDynType()));
  return value_factory.CreateBorrowedListValue<ArenaStaticProtoJsonListValue>(
      std::move(owner), std::move(list_type), &value);
}

absl::StatusOr<Handle<MapValue>> ProtoValue::Create(
    ValueFactory& value_factory, google::protobuf::Struct value) {
  CEL_ASSIGN_OR_RETURN(auto map_type,
                       value_factory.type_factory().CreateMapType(
                           value_factory.type_factory().GetStringType(),
                           value_factory.type_factory().GetDynType()));
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::Struct>(arena);
      *arena_value = std::move(value);
      return value_factory.CreateMapValue<ArenaStaticProtoJsonMapValue>(
          std::move(map_type), arena_value);
    }
  }
  return value_factory.CreateMapValue<StaticProtoJsonMapValue>(
      std::move(map_type), std::move(value));
}

absl::StatusOr<Handle<MapValue>> ProtoValue::Create(
    ValueFactory& value_factory,
    std::unique_ptr<google::protobuf::Struct> value) {
  CEL_ASSIGN_OR_RETURN(auto map_type,
                       value_factory.type_factory().CreateMapType(
                           value_factory.type_factory().GetStringType(),
                           value_factory.type_factory().GetDynType()));
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::Struct>(arena);
      *arena_value = std::move(*value);
      return value_factory.CreateMapValue<ArenaStaticProtoJsonMapValue>(
          std::move(map_type), arena_value);
    }
  }
  return value_factory.CreateMapValue<StaticProtoJsonMapValue>(
      std::move(map_type), std::move(*value));
}

absl::StatusOr<Handle<MapValue>> ProtoValue::CreateBorrowed(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Struct& value) {
  CEL_ASSIGN_OR_RETURN(auto map_type,
                       value_factory.type_factory().CreateMapType(
                           value_factory.type_factory().GetStringType(),
                           value_factory.type_factory().GetDynType()));
  return value_factory.CreateBorrowedMapValue<ArenaStaticProtoJsonMapValue>(
      std::move(owner), std::move(map_type), &value);
}

absl::StatusOr<Handle<Value>> ProtoValue::Create(
    ValueFactory& value_factory,
    std::unique_ptr<google::protobuf::Value> value) {
  switch (value->kind_case()) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValue:
      return value_factory.CreateBoolValue(value->bool_value());
    case google::protobuf::Value::kNumberValue:
      return value_factory.CreateDoubleValue(value->number_value());
    case google::protobuf::Value::kStringValue:
      return value_factory.CreateUncheckedStringValue(
          std::move(*value->mutable_string_value()));
    case google::protobuf::Value::kListValue:
      return Create(value_factory,
                    absl::WrapUnique(value->release_list_value()));
    case google::protobuf::Value::kStructValue:
      return Create(value_factory,
                    absl::WrapUnique(value->release_struct_value()));
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected google.protobuf.Value kind: ", value->kind_case()));
  }
}

absl::StatusOr<Handle<Value>> ProtoValue::Create(
    ValueFactory& value_factory, google::protobuf::Value value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValue:
      return value_factory.CreateBoolValue(value.bool_value());
    case google::protobuf::Value::kNumberValue:
      return value_factory.CreateDoubleValue(value.number_value());
    case google::protobuf::Value::kStringValue:
      return value_factory.CreateUncheckedStringValue(value.string_value());
    case google::protobuf::Value::kListValue:
      return Create(value_factory, std::move(*value.mutable_list_value()));
    case google::protobuf::Value::kStructValue:
      return Create(value_factory, std::move(*value.mutable_struct_value()));
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected google.protobuf.Value kind: ", value.kind_case()));
  }
}

absl::StatusOr<Handle<Value>> ProtoValue::CreateBorrowed(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Value& value) {
  switch (value.kind_case()) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValue:
      return value_factory.CreateBoolValue(value.bool_value());
    case google::protobuf::Value::kNumberValue:
      return value_factory.CreateDoubleValue(value.number_value());
    case google::protobuf::Value::kStringValue:
      return value_factory.CreateBorrowedStringValue(std::move(owner),
                                                     value.string_value());
    case google::protobuf::Value::kListValue:
      return CreateBorrowed(std::move(owner), value_factory,
                            value.list_value());
    case google::protobuf::Value::kStructValue:
      return CreateBorrowed(std::move(owner), value_factory,
                            value.struct_value());
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected google.protobuf.Value kind: ", value.kind_case()));
  }
}

namespace {

using DynamicMessageCopyConverter =
    absl::StatusOr<Handle<Value>> (*)(ValueFactory&, const google::protobuf::Message&);
using DynamicMessageMoveConverter =
    absl::StatusOr<Handle<Value>> (*)(ValueFactory&, google::protobuf::Message&&);
using DynamicMessageBorrowConverter = absl::StatusOr<Handle<Value>> (*)(
    Owner<Value>&, ValueFactory&, const google::protobuf::Message&);

using DynamicMessageConverter =
    std::tuple<absl::string_view, DynamicMessageCopyConverter,
               DynamicMessageMoveConverter, DynamicMessageBorrowConverter>;

absl::StatusOr<Handle<Value>> DurationMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto duration,
                       protobuf_internal::UnwrapDynamicDurationProto(value));
  return value_factory.CreateUncheckedDurationValue(duration);
}

absl::StatusOr<Handle<Value>> DurationMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto duration,
                       protobuf_internal::UnwrapDynamicDurationProto(value));
  return value_factory.CreateUncheckedDurationValue(duration);
}

absl::StatusOr<Handle<Value>> DurationMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto duration,
                       protobuf_internal::UnwrapDynamicDurationProto(value));
  return value_factory.CreateUncheckedDurationValue(duration);
}

absl::StatusOr<Handle<Value>> TimestampMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto time,
                       protobuf_internal::UnwrapDynamicTimestampProto(value));
  return value_factory.CreateUncheckedTimestampValue(time);
}

absl::StatusOr<Handle<Value>> TimestampMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto time,
                       protobuf_internal::UnwrapDynamicTimestampProto(value));
  return value_factory.CreateUncheckedTimestampValue(time);
}

absl::StatusOr<Handle<Value>> TimestampMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto time,
                       protobuf_internal::UnwrapDynamicTimestampProto(value));
  return value_factory.CreateUncheckedTimestampValue(time);
}

absl::StatusOr<Handle<Value>> BoolValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicBoolValueProto(value));
  return value_factory.CreateBoolValue(wrapped);
}

absl::StatusOr<Handle<Value>> BoolValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicBoolValueProto(value));
  return value_factory.CreateBoolValue(wrapped);
}

absl::StatusOr<Handle<Value>> BoolValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicBoolValueProto(value));
  return value_factory.CreateBoolValue(wrapped);
}

absl::StatusOr<Handle<Value>> BytesValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicBytesValueProto(value));
  return value_factory.CreateBytesValue(std::move(wrapped));
}

absl::StatusOr<Handle<Value>> BytesValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicBytesValueProto(value));
  return value_factory.CreateBytesValue(std::move(wrapped));
}

absl::StatusOr<Handle<Value>> BytesValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicBytesValueProto(value));
  return value_factory.CreateBytesValue(std::move(wrapped));
}

absl::StatusOr<Handle<Value>> FloatValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicFloatValueProto(value));
  return value_factory.CreateDoubleValue(wrapped);
}

absl::StatusOr<Handle<Value>> FloatValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicFloatValueProto(value));
  return value_factory.CreateDoubleValue(wrapped);
}

absl::StatusOr<Handle<Value>> FloatValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicFloatValueProto(value));
  return value_factory.CreateDoubleValue(wrapped);
}

absl::StatusOr<Handle<Value>> DoubleValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicDoubleValueProto(value));
  return value_factory.CreateDoubleValue(wrapped);
}

absl::StatusOr<Handle<Value>> DoubleValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicDoubleValueProto(value));
  return value_factory.CreateDoubleValue(wrapped);
}

absl::StatusOr<Handle<Value>> DoubleValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicDoubleValueProto(value));
  return value_factory.CreateDoubleValue(wrapped);
}

absl::StatusOr<Handle<Value>> Int32ValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicInt32ValueProto(value));
  return value_factory.CreateIntValue(wrapped);
}

absl::StatusOr<Handle<Value>> Int32ValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicInt32ValueProto(value));
  return value_factory.CreateIntValue(wrapped);
}

absl::StatusOr<Handle<Value>> Int32ValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicInt32ValueProto(value));
  return value_factory.CreateIntValue(wrapped);
}

absl::StatusOr<Handle<Value>> Int64ValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicInt64ValueProto(value));
  return value_factory.CreateIntValue(wrapped);
}

absl::StatusOr<Handle<Value>> Int64ValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicInt64ValueProto(value));
  return value_factory.CreateIntValue(wrapped);
}

absl::StatusOr<Handle<Value>> Int64ValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicInt64ValueProto(value));
  return value_factory.CreateIntValue(wrapped);
}

absl::StatusOr<Handle<Value>> StringValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicStringValueProto(value));
  return value_factory.CreateUncheckedStringValue(std::move(wrapped));
}

absl::StatusOr<Handle<Value>> StringValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicStringValueProto(value));
  return value_factory.CreateUncheckedStringValue(std::move(wrapped));
}

absl::StatusOr<Handle<Value>> StringValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicStringValueProto(value));
  return value_factory.CreateUncheckedStringValue(std::move(wrapped));
}

absl::StatusOr<Handle<Value>> UInt32ValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicUInt32ValueProto(value));
  return value_factory.CreateUintValue(wrapped);
}

absl::StatusOr<Handle<Value>> UInt32ValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicUInt32ValueProto(value));
  return value_factory.CreateUintValue(wrapped);
}

absl::StatusOr<Handle<Value>> UInt32ValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicUInt32ValueProto(value));
  return value_factory.CreateUintValue(wrapped);
}

absl::StatusOr<Handle<Value>> UInt64ValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicUInt64ValueProto(value));
  return value_factory.CreateUintValue(wrapped);
}

absl::StatusOr<Handle<Value>> UInt64ValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicUInt64ValueProto(value));
  return value_factory.CreateUintValue(wrapped);
}

absl::StatusOr<Handle<Value>> UInt64ValueMessageBorrowConverter(
    Owner<Value>& owner ABSL_ATTRIBUTE_UNUSED, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  CEL_ASSIGN_OR_RETURN(auto wrapped,
                       protobuf_internal::UnwrapDynamicUInt64ValueProto(value));
  return value_factory.CreateUintValue(wrapped);
}

absl::StatusOr<Handle<Value>> StructMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  if (value.GetDescriptor() == google::protobuf::Struct::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        cel::internal::down_cast<const google::protobuf::Struct&>(value));
  }
  std::string serialized;
  if (!value.SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.Struct");
  }
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto map_type,
                           value_factory.type_factory().CreateMapType(
                               value_factory.type_factory().GetStringType(),
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::Struct>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.Struct");
      }
      return value_factory.CreateMapValue<ArenaStaticProtoJsonMapValue>(
          std::move(map_type), arena_value);
    }
  }
  google::protobuf::Struct parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.Struct");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> StructMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  if (value.GetDescriptor() == google::protobuf::Struct::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        std::move(cel::internal::down_cast<google::protobuf::Struct&>(value)));
  }
  std::string serialized;
  if (!value.SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.Struct");
  }
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto map_type,
                           value_factory.type_factory().CreateMapType(
                               value_factory.type_factory().GetStringType(),
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::Struct>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.Struct");
      }
      return value_factory.CreateMapValue<ArenaStaticProtoJsonMapValue>(
          std::move(map_type), arena_value);
    }
  }
  google::protobuf::Struct parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.Struct");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> StructMessageBorrowConverter(
    Owner<Value>& owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  if (value.GetDescriptor() == google::protobuf::Struct::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        cel::internal::down_cast<const google::protobuf::Struct&>(value));
  }
  std::string serialized;
  if (!value.SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.Struct");
  }
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto map_type,
                           value_factory.type_factory().CreateMapType(
                               value_factory.type_factory().GetStringType(),
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::Struct>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.Struct");
      }
      return value_factory.CreateMapValue<ArenaStaticProtoJsonMapValue>(
          std::move(map_type), arena_value);
    }
  }
  google::protobuf::Struct parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.Struct");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> StructMessageOwnConverter(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value) {
  if (value->GetDescriptor() == google::protobuf::Struct::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        std::move(cel::internal::down_cast<google::protobuf::Struct&>(*value)));
  }
  std::string serialized;
  if (!value->SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.Struct");
  }
  value.reset();
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto map_type,
                           value_factory.type_factory().CreateMapType(
                               value_factory.type_factory().GetStringType(),
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::Struct>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.Struct");
      }
      return value_factory.CreateMapValue<ArenaStaticProtoJsonMapValue>(
          std::move(map_type), arena_value);
    }
  }
  google::protobuf::Struct parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.Struct");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> ListValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  if (value.GetDescriptor() == google::protobuf::ListValue::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        cel::internal::down_cast<const google::protobuf::ListValue&>(value));
  }
  std::string serialized;
  if (!value.SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.ListValue");
  }
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto map_type,
                           value_factory.type_factory().CreateListType(
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::ListValue>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.ListValue");
      }
      return value_factory.CreateListValue<ArenaStaticProtoJsonListValue>(
          std::move(map_type), arena_value);
    }
  }
  google::protobuf::ListValue parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.ListValue");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> ListValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  if (value.GetDescriptor() == google::protobuf::ListValue::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        std::move(
            cel::internal::down_cast<google::protobuf::ListValue&>(value)));
  }
  std::string serialized;
  if (!value.SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.ListValue");
  }
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto map_type,
                           value_factory.type_factory().CreateListType(
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::ListValue>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.ListValue");
      }
      return value_factory.CreateListValue<ArenaStaticProtoJsonListValue>(
          std::move(map_type), arena_value);
    }
  }
  google::protobuf::ListValue parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.ListValue");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> ListValueMessageBorrowConverter(
    Owner<Value>& owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  if (value.GetDescriptor() == google::protobuf::ListValue::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        cel::internal::down_cast<const google::protobuf::ListValue&>(value));
  }
  std::string serialized;
  if (!value.SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.ListValue");
  }
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto map_type,
                           value_factory.type_factory().CreateListType(
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::ListValue>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.ListValue");
      }
      return value_factory.CreateListValue<ArenaStaticProtoJsonListValue>(
          std::move(map_type), arena_value);
    }
  }
  google::protobuf::ListValue parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.ListValue");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> ListValueMessageOwnConverter(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value) {
  if (value->GetDescriptor() == google::protobuf::ListValue::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        std::move(
            cel::internal::down_cast<google::protobuf::ListValue&>(*value)));
  }
  std::string serialized;
  if (!value->SerializePartialToString(&serialized)) {
    return absl::InternalError("failed to serialize google.protobuf.ListValue");
  }
  value.reset();
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (arena != nullptr) {
      CEL_ASSIGN_OR_RETURN(auto type,
                           value_factory.type_factory().CreateListType(
                               value_factory.type_factory().GetDynType()));
      auto* arena_value =
          google::protobuf::Arena::CreateMessage<google::protobuf::ListValue>(arena);
      if (!arena_value->ParsePartialFromString(serialized)) {
        return absl::InternalError("failed to parse google.protobuf.ListValue");
      }
      return value_factory.CreateListValue<ArenaStaticProtoJsonListValue>(
          std::move(type), arena_value);
    }
  }
  google::protobuf::ListValue parsed;
  if (!parsed.ParsePartialFromString(serialized)) {
    return absl::InternalError("failed to parse google.protobuf.ListValue");
  }
  return ProtoValue::Create(value_factory, std::move(parsed));
}

absl::StatusOr<Handle<Value>> ValueMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  const auto* desc = value.GetDescriptor();
  if (desc == google::protobuf::Value::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        cel::internal::down_cast<const google::protobuf::Value&>(value));
  }
  const auto* oneof_desc = desc->FindOneofByName("kind");
  if (ABSL_PREDICT_FALSE(oneof_desc == nullptr)) {
    return absl::InvalidArgumentError(
        "oneof descriptor missing for google.protobuf.Value");
  }
  const auto* reflect = value.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InvalidArgumentError(
        "reflection missing for google.protobuf.Value");
  }
  const auto* field_desc = reflect->GetOneofFieldDescriptor(value, oneof_desc);
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return value_factory.GetNullValue();
  }
  switch (field_desc->number()) {
    case google::protobuf::Value::kNullValueFieldNumber:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValueFieldNumber:
      return value_factory.CreateBoolValue(reflect->GetBool(value, field_desc));
    case google::protobuf::Value::kNumberValueFieldNumber:
      return value_factory.CreateDoubleValue(
          reflect->GetDouble(value, field_desc));
    case google::protobuf::Value::kStringValueFieldNumber:
      return protobuf_internal::GetStringField(value_factory, value, reflect,
                                               field_desc);
    case google::protobuf::Value::kListValueFieldNumber:
      return ListValueMessageCopyConverter(
          value_factory, reflect->GetMessage(value, field_desc));
    case google::protobuf::Value::kStructValueFieldNumber:
      return StructMessageCopyConverter(value_factory,
                                        reflect->GetMessage(value, field_desc));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected oneof field set for google.protobuf.Value: ",
                       field_desc->number()));
  }
}

absl::StatusOr<Handle<Value>> ValueMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  const auto* desc = value.GetDescriptor();
  if (desc == google::protobuf::Value::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        std::move(cel::internal::down_cast<google::protobuf::Value&>(value)));
  }
  const auto* oneof_desc = desc->FindOneofByName("kind");
  if (ABSL_PREDICT_FALSE(oneof_desc == nullptr)) {
    return absl::InvalidArgumentError(
        "oneof descriptor missing for google.protobuf.Value");
  }
  const auto* reflect = value.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InvalidArgumentError(
        "reflection missing for google.protobuf.Value");
  }
  const auto* field_desc = reflect->GetOneofFieldDescriptor(value, oneof_desc);
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return value_factory.GetNullValue();
  }
  switch (field_desc->number()) {
    case google::protobuf::Value::kNullValueFieldNumber:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValueFieldNumber:
      return value_factory.CreateBoolValue(reflect->GetBool(value, field_desc));
    case google::protobuf::Value::kNumberValueFieldNumber:
      return value_factory.CreateDoubleValue(
          reflect->GetDouble(value, field_desc));
    case google::protobuf::Value::kStringValueFieldNumber:
      return protobuf_internal::GetStringField(value_factory, value, reflect,
                                               field_desc);
    case google::protobuf::Value::kListValueFieldNumber:
      return ListValueMessageMoveConverter(
          value_factory,
          std::move(*reflect->MutableMessage(&value, field_desc)));
    case google::protobuf::Value::kStructValueFieldNumber:
      return StructMessageMoveConverter(
          value_factory,
          std::move(*reflect->MutableMessage(&value, field_desc)));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected oneof field set for google.protobuf.Value: ",
                       field_desc->number()));
  }
}

absl::StatusOr<Handle<Value>> ValueMessageBorrowConverter(
    Owner<Value>& owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  const auto* desc = value.GetDescriptor();
  if (desc == google::protobuf::Value::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        cel::internal::down_cast<const google::protobuf::Value&>(value));
  }
  const auto* oneof_desc = desc->FindOneofByName("kind");
  if (ABSL_PREDICT_FALSE(oneof_desc == nullptr)) {
    return absl::InvalidArgumentError(
        "oneof descriptor missing for google.protobuf.Value");
  }
  const auto* reflect = value.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InvalidArgumentError(
        "reflection missing for google.protobuf.Value");
  }
  const auto* field_desc = reflect->GetOneofFieldDescriptor(value, oneof_desc);
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return value_factory.GetNullValue();
  }
  switch (field_desc->number()) {
    case google::protobuf::Value::kNullValueFieldNumber:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValueFieldNumber:
      return value_factory.CreateBoolValue(reflect->GetBool(value, field_desc));
    case google::protobuf::Value::kNumberValueFieldNumber:
      return value_factory.CreateDoubleValue(
          reflect->GetDouble(value, field_desc));
    case google::protobuf::Value::kStringValueFieldNumber:
      return protobuf_internal::GetBorrowedStringField(
          value_factory, std::move(owner), value, reflect, field_desc);
    case google::protobuf::Value::kListValueFieldNumber:
      return ListValueMessageBorrowConverter(
          owner, value_factory, reflect->GetMessage(value, field_desc));
    case google::protobuf::Value::kStructValueFieldNumber:
      return StructMessageBorrowConverter(
          owner, value_factory, reflect->GetMessage(value, field_desc));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected oneof field set for google.protobuf.Value: ",
                       field_desc->number()));
  }
}

absl::StatusOr<Handle<Value>> ValueMessageOwnConverter(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value) {
  const auto* desc = value->GetDescriptor();
  if (desc == google::protobuf::Value::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        absl::WrapUnique(cel::internal::down_cast<google::protobuf::Value*>(
            value.release())));
  }
  const auto* oneof_desc = desc->FindOneofByName("kind");
  if (ABSL_PREDICT_FALSE(oneof_desc == nullptr)) {
    return absl::InvalidArgumentError(
        "oneof descriptor missing for google.protobuf.Value");
  }
  const auto* reflect = value->GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InvalidArgumentError(
        "reflection missing for google.protobuf.Value");
  }
  const auto* field_desc = reflect->GetOneofFieldDescriptor(*value, oneof_desc);
  if (ABSL_PREDICT_FALSE(field_desc == nullptr)) {
    return value_factory.GetNullValue();
  }
  switch (field_desc->number()) {
    case google::protobuf::Value::kNullValueFieldNumber:
      return value_factory.GetNullValue();
    case google::protobuf::Value::kBoolValueFieldNumber:
      return value_factory.CreateBoolValue(
          reflect->GetBool(*value, field_desc));
    case google::protobuf::Value::kNumberValueFieldNumber:
      return value_factory.CreateDoubleValue(
          reflect->GetDouble(*value, field_desc));
    case google::protobuf::Value::kStringValueFieldNumber:
      return protobuf_internal::GetStringField(value_factory, *value, reflect,
                                               field_desc);
    case google::protobuf::Value::kListValueFieldNumber:
      return ListValueMessageCopyConverter(
          value_factory, reflect->GetMessage(*value, field_desc));
    case google::protobuf::Value::kStructValueFieldNumber:
      return StructMessageCopyConverter(
          value_factory, reflect->GetMessage(*value, field_desc));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected oneof field set for google.protobuf.Value: ",
                       field_desc->number()));
  }
}

absl::StatusOr<Handle<Value>> AnyMessageCopyConverter(
    ValueFactory& value_factory, const google::protobuf::Message& value) {
  const auto* descriptor = value.GetDescriptor();
  if (descriptor == google::protobuf::Any::descriptor()) {
    return ProtoValue::Create(
        value_factory,
        cel::internal::down_cast<const google::protobuf::Any&>(value));
  }
  const auto* reflect = value.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InvalidArgumentError(
        "reflection missing for google.protobuf.Any");
  }
  const auto* type_url_field =
      descriptor->FindFieldByNumber(google::protobuf::Any::kTypeUrlFieldNumber);
  if (ABSL_PREDICT_FALSE(type_url_field == nullptr)) {
    return absl::InvalidArgumentError(
        "type_url field descriptor missing for google.protobuf.Any");
  }
  if (ABSL_PREDICT_FALSE(type_url_field->is_repeated() ||
                         type_url_field->is_map() ||
                         type_url_field->cpp_type() !=
                             google::protobuf::FieldDescriptor::CPPTYPE_STRING)) {
    return absl::InvalidArgumentError(
        "type_url field descriptor has unexpected type");
  }
  const auto* value_field =
      descriptor->FindFieldByNumber(google::protobuf::Any::kValueFieldNumber);
  if (ABSL_PREDICT_FALSE(value_field == nullptr)) {
    return absl::InvalidArgumentError(
        "value field descriptor missing for google.protobuf.Any");
  }
  if (ABSL_PREDICT_FALSE(value_field->is_repeated() || value_field->is_map() ||
                         value_field->cpp_type() !=
                             google::protobuf::FieldDescriptor::CPPTYPE_STRING)) {
    return absl::InvalidArgumentError(
        "value field descriptor has unexpected type");
  }
  std::string type_url;
  return ProtoValue::Create(
      value_factory,
      reflect->GetStringReference(value, type_url_field, &type_url),
      reflect->GetCord(value, value_field));
}

absl::StatusOr<Handle<Value>> AnyMessageMoveConverter(
    ValueFactory& value_factory, google::protobuf::Message&& value) {
  // We currently do nothing special for moving.
  return AnyMessageCopyConverter(value_factory, value);
}

absl::StatusOr<Handle<Value>> AnyMessageBorrowConverter(
    Owner<Value>& owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  // We currently do nothing special for borrowing.
  return AnyMessageCopyConverter(value_factory, value);
}

ABSL_CONST_INIT absl::once_flag proto_value_once;
ABSL_CONST_INIT DynamicMessageConverter dynamic_message_converters[] = {
    {"google.protobuf.Duration", DurationMessageCopyConverter,
     DurationMessageMoveConverter, DurationMessageBorrowConverter},
    {"google.protobuf.Timestamp", TimestampMessageCopyConverter,
     TimestampMessageMoveConverter, TimestampMessageBorrowConverter},
    {"google.protobuf.BoolValue", BoolValueMessageCopyConverter,
     BoolValueMessageMoveConverter, BoolValueMessageBorrowConverter},
    {"google.protobuf.BytesValue", BytesValueMessageCopyConverter,
     BytesValueMessageMoveConverter, BytesValueMessageBorrowConverter},
    {"google.protobuf.FloatValue", FloatValueMessageCopyConverter,
     FloatValueMessageMoveConverter, FloatValueMessageBorrowConverter},
    {"google.protobuf.DoubleValue", DoubleValueMessageCopyConverter,
     DoubleValueMessageMoveConverter, DoubleValueMessageBorrowConverter},
    {"google.protobuf.Int32Value", Int32ValueMessageCopyConverter,
     Int32ValueMessageMoveConverter, Int32ValueMessageBorrowConverter},
    {"google.protobuf.Int64Value", Int64ValueMessageCopyConverter,
     Int64ValueMessageMoveConverter, Int64ValueMessageBorrowConverter},
    {"google.protobuf.StringValue", StringValueMessageCopyConverter,
     StringValueMessageMoveConverter, StringValueMessageBorrowConverter},
    {"google.protobuf.UInt32Value", UInt32ValueMessageCopyConverter,
     UInt32ValueMessageMoveConverter, UInt32ValueMessageBorrowConverter},
    {"google.protobuf.UInt64Value", UInt64ValueMessageCopyConverter,
     UInt64ValueMessageMoveConverter, UInt64ValueMessageBorrowConverter},
    {"google.protobuf.Struct", StructMessageCopyConverter,
     StructMessageMoveConverter, StructMessageBorrowConverter},
    {"google.protobuf.ListValue", ListValueMessageCopyConverter,
     ListValueMessageMoveConverter, ListValueMessageBorrowConverter},
    {"google.protobuf.Value", ValueMessageCopyConverter,
     ValueMessageMoveConverter, ValueMessageBorrowConverter},
    {"google.protobuf.Any", AnyMessageCopyConverter, AnyMessageMoveConverter,
     AnyMessageBorrowConverter},
};

DynamicMessageConverter* dynamic_message_converters_begin() {
  return dynamic_message_converters;
}

DynamicMessageConverter* dynamic_message_converters_end() {
  return dynamic_message_converters +
         ABSL_ARRAYSIZE(dynamic_message_converters);
}

const DynamicMessageConverter* dynamic_message_converters_cbegin() {
  return dynamic_message_converters_begin();
}

const DynamicMessageConverter* dynamic_message_converters_cend() {
  return dynamic_message_converters_end();
}

struct DynamicMessageConverterComparer {
  bool operator()(const DynamicMessageConverter& lhs,
                  absl::string_view rhs) const {
    return std::get<absl::string_view>(lhs) < rhs;
  }

  bool operator()(absl::string_view lhs,
                  const DynamicMessageConverter& rhs) const {
    return lhs < std::get<absl::string_view>(rhs);
  }
};

void InitializeProtoValue() {
  std::stable_sort(dynamic_message_converters_begin(),
                   dynamic_message_converters_end(),
                   [](const DynamicMessageConverter& lhs,
                      const DynamicMessageConverter& rhs) {
                     return std::get<absl::string_view>(lhs) <
                            std::get<absl::string_view>(rhs);
                   });
}

}  // namespace

absl::StatusOr<Handle<Value>> ProtoValue::Create(ValueFactory& value_factory,
                                                 const google::protobuf::Message& value) {
  const auto* desc = value.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError("protocol buffer message missing descriptor");
  }
  const auto& type_name = desc->full_name();
  absl::call_once(proto_value_once, InitializeProtoValue);
  auto converter = std::lower_bound(
      dynamic_message_converters_cbegin(), dynamic_message_converters_cend(),
      type_name, DynamicMessageConverterComparer{});
  if (converter != dynamic_message_converters_cend() &&
      std::get<absl::string_view>(*converter) == type_name) {
    return std::get<DynamicMessageCopyConverter>(*converter)(value_factory,
                                                             value);
  }
  return ProtoStructValue::Create(value_factory, value);
}

absl::StatusOr<Handle<Value>> ProtoValue::CreateBorrowed(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  const auto* desc = value.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError("protocol buffer message missing descriptor");
  }
  const auto& type_name = desc->full_name();
  absl::call_once(proto_value_once, InitializeProtoValue);
  auto converter = std::lower_bound(
      dynamic_message_converters_cbegin(), dynamic_message_converters_cend(),
      type_name, DynamicMessageConverterComparer{});
  if (converter != dynamic_message_converters_cend() &&
      std::get<absl::string_view>(*converter) == type_name) {
    return std::get<DynamicMessageBorrowConverter>(*converter)(
        owner, value_factory, value);
  }
  return ProtoStructValue::CreateBorrowed(std::move(owner), value_factory,
                                          value);
}

absl::StatusOr<Handle<Value>> ProtoValue::Create(ValueFactory& value_factory,
                                                 google::protobuf::Message&& value) {
  const auto* desc = value.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InternalError("protocol buffer message missing descriptor");
  }
  const auto& type_name = desc->full_name();
  absl::call_once(proto_value_once, InitializeProtoValue);
  auto converter = std::lower_bound(
      dynamic_message_converters_cbegin(), dynamic_message_converters_cend(),
      type_name, DynamicMessageConverterComparer{});
  if (converter != dynamic_message_converters_cend() &&
      std::get<absl::string_view>(*converter) == type_name) {
    return std::get<DynamicMessageMoveConverter>(*converter)(value_factory,
                                                             std::move(value));
  }
  return ProtoStructValue::Create(value_factory, std::move(value));
}

absl::StatusOr<Handle<Value>> ProtoValue::Create(
    ValueFactory& value_factory, const google::protobuf::EnumDescriptor& descriptor,
    int value) {
  CEL_ASSIGN_OR_RETURN(
      auto type, ProtoType::Resolve(value_factory.type_manager(), descriptor));
  switch (type->kind()) {
    case TypeKind::kNullType:
      // google.protobuf.NullValue is an enum, which represents JSON null.
      return value_factory.GetNullValue();
    case TypeKind::kEnum:
      return value_factory.CreateEnumValue(std::move(type).As<ProtoEnumType>(),
                                           value);
    default:
      ABSL_UNREACHABLE();
  }
}

namespace {

template <typename T>
absl::StatusOr<T> UnpackTo(const absl::Cord& cord) {
  T proto;
  if (ABSL_PREDICT_FALSE(!proto.ParseFromCord(cord))) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to unpack google.protobuf.Any as ",
                     T::descriptor()->full_name()));
  }
  return proto;
}

}  // namespace

absl::StatusOr<Handle<Value>> ProtoValue::Create(ValueFactory& value_factory,
                                                 absl::string_view type_url,
                                                 const absl::Cord& payload) {
  if (type_url.empty()) {
    return value_factory.CreateErrorValue(
        absl::UnknownError("invalid empty type URL in google.protobuf.Any"));
  }
  auto type_name = absl::StripPrefix(type_url, "type.googleapis.com/");
  CEL_ASSIGN_OR_RETURN(auto type,
                       value_factory.type_manager().ResolveType(type_name));
  if (ABSL_PREDICT_FALSE(!type.has_value())) {
    return value_factory.CreateErrorValue(
        absl::NotFoundError(absl::StrCat("type not found: ", type_url)));
  }
  switch ((*type)->kind()) {
    case TypeKind::kAny:
      ABSL_DCHECK(type_name == "google.protobuf.Any") << type_name;
      // google.protobuf.Any
      //
      // We refuse google.protobuf.Any wrapped in google.protobuf.Any.
      return absl::InvalidArgumentError(
          "refusing to unpack google.protobuf.Any to google.protobuf.Any");
    case TypeKind::kStruct: {
      if (!ProtoStructType::Is(**type)) {
        return absl::FailedPreconditionError(
            "google.protobuf.Any can only be unpacked to protocol "
            "buffer message based structs");
      }
      const auto& struct_type = (*type)->As<ProtoStructType>();
      const auto* prototype =
          struct_type.factory_->GetPrototype(struct_type.descriptor_);
      if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
        return absl::InternalError(absl::StrCat(
            "protocol buffer message factory does not have prototype for ",
            struct_type.DebugString()));
      }
      auto proto = absl::WrapUnique(prototype->New());
      if (ABSL_PREDICT_FALSE(!proto->ParseFromCord(payload))) {
        return absl::InvalidArgumentError(
            absl::StrCat("failed to unpack google.protobuf.Any to ",
                         struct_type.DebugString()));
      }
      return ProtoStructValue::Create(value_factory, std::move(*proto));
    }
    case TypeKind::kWrapper: {
      switch ((*type)->As<WrapperType>().wrapped()->kind()) {
        case TypeKind::kBool: {
          // google.protobuf.BoolValue
          CEL_ASSIGN_OR_RETURN(auto proto,
                               UnpackTo<google::protobuf::BoolValue>(payload));
          return Create(value_factory, proto);
        }
        case TypeKind::kInt: {
          // google.protobuf.{Int32Value,Int64Value}
          if (type_name == "google.protobuf.Int32Value") {
            CEL_ASSIGN_OR_RETURN(
                auto proto, UnpackTo<google::protobuf::Int32Value>(payload));
            return Create(value_factory, std::move(proto));
          }
          if (type_name == "google.protobuf.Int64Value") {
            CEL_ASSIGN_OR_RETURN(
                auto proto, UnpackTo<google::protobuf::Int64Value>(payload));
            return Create(value_factory, std::move(proto));
          }
        } break;
        case TypeKind::kUint: {
          // google.protobuf.{UInt32Value,UInt64Value}
          if (type_name == "google.protobuf.UInt32Value") {
            CEL_ASSIGN_OR_RETURN(
                auto proto, UnpackTo<google::protobuf::UInt32Value>(payload));
            return Create(value_factory, std::move(proto));
          }
          if (type_name == "google.protobuf.UInt64Value") {
            CEL_ASSIGN_OR_RETURN(
                auto proto, UnpackTo<google::protobuf::UInt64Value>(payload));
            return Create(value_factory, std::move(proto));
          }
        } break;
        case TypeKind::kDouble: {
          // google.protobuf.{FloatValue,DoubleValue}
          if (type_name == "google.protobuf.FloatValue") {
            CEL_ASSIGN_OR_RETURN(
                auto proto, UnpackTo<google::protobuf::FloatValue>(payload));
            return Create(value_factory, std::move(proto));
          }
          if (type_name == "google.protobuf.DoubleValue") {
            CEL_ASSIGN_OR_RETURN(
                auto proto, UnpackTo<google::protobuf::DoubleValue>(payload));
            return Create(value_factory, std::move(proto));
          }
        } break;
        case TypeKind::kBytes: {
          // google.protobuf.BytesValue
          CEL_ASSIGN_OR_RETURN(auto proto,
                               UnpackTo<google::protobuf::BytesValue>(payload));
          return Create(value_factory, std::move(proto));
        }
        case TypeKind::kString: {
          // google.protobuf.StringValue
          CEL_ASSIGN_OR_RETURN(
              auto proto, UnpackTo<google::protobuf::StringValue>(payload));
          return Create(value_factory, std::move(proto));
        }
        default:
          ABSL_UNREACHABLE();
      }
    } break;
    case TypeKind::kList: {
      // google.protobuf.ListValue
      ABSL_DCHECK(type_name == "google.protobuf.ListValue") << type_name;
      CEL_ASSIGN_OR_RETURN(auto proto,
                           UnpackTo<google::protobuf::ListValue>(payload));
      return Create(value_factory, std::move(proto));
    }
    case TypeKind::kMap: {
      // google.protobuf.Struct
      ABSL_DCHECK(type_name == "google.protobuf.Struct") << type_name;
      CEL_ASSIGN_OR_RETURN(auto proto,
                           UnpackTo<google::protobuf::Struct>(payload));
      return Create(value_factory, std::move(proto));
    }
    case TypeKind::kDyn: {
      // google.protobuf.Value
      ABSL_DCHECK(type_name == "google.protobuf.Value") << type_name;
      CEL_ASSIGN_OR_RETURN(auto proto,
                           UnpackTo<google::protobuf::Value>(payload));
      return Create(value_factory, std::move(proto));
    }
    case TypeKind::kDuration: {
      // google.protobuf.Duration
      ABSL_DCHECK(type_name == "google.protobuf.Duration") << type_name;
      CEL_ASSIGN_OR_RETURN(auto proto,
                           UnpackTo<google::protobuf::Duration>(payload));
      return Create(value_factory, proto);
    }
    case TypeKind::kTimestamp: {
      // google.protobuf.Timestamp
      ABSL_DCHECK(type_name == "google.protobuf.Timestamp") << type_name;
      CEL_ASSIGN_OR_RETURN(auto proto,
                           UnpackTo<google::protobuf::Timestamp>(payload));
      return Create(value_factory, proto);
    }
    default:
      break;
  }
  return absl::UnimplementedError(
      absl::StrCat("google.protobuf.Any unpacking to ", (*type)->DebugString(),
                   " is not implemented"));
}

namespace protobuf_internal {

absl::StatusOr<Handle<Value>> CreateBorrowedListValue(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  return ListValueMessageBorrowConverter(owner, value_factory, value);
}

absl::StatusOr<Handle<Value>> CreateBorrowedStruct(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  return StructMessageBorrowConverter(owner, value_factory, value);
}

absl::StatusOr<Handle<Value>> CreateBorrowedValue(
    Owner<Value> owner, ValueFactory& value_factory,
    const google::protobuf::Message& value) {
  return ValueMessageBorrowConverter(owner, value_factory, value);
}

absl::StatusOr<Handle<Value>> CreateListValue(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value) {
  return ListValueMessageOwnConverter(value_factory, std::move(value));
}

absl::StatusOr<Handle<Value>> CreateStruct(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value) {
  return StructMessageOwnConverter(value_factory, std::move(value));
}

absl::StatusOr<Handle<Value>> CreateValue(
    ValueFactory& value_factory, std::unique_ptr<google::protobuf::Message> value) {
  return ValueMessageOwnConverter(value_factory, std::move(value));
}

}  // namespace protobuf_internal

}  // namespace cel::extensions
