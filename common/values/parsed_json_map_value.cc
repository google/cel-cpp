// Copyright 2024 Google LLC
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

#include "common/values/parsed_json_map_value.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/parsed_json_value.h"
#include "internal/json.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/map.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel {

namespace common_internal {

absl::Status CheckWellKnownStructMessage(const google::protobuf::MessageLite& message) {
  return internal::CheckJsonMap(message);
}

}  // namespace common_internal

ParsedJsonMapValue::ParsedJsonMapValue()
    : ParsedJsonMapValue(common_internal::WrapEternal(
          &google::protobuf::Struct::default_instance())) {}

std::string ParsedJsonMapValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return "{}";
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return internal::JsonMapDebugString(*generated_message);
  }
  return internal::JsonMapDebugString(
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_));
}

absl::Status ParsedJsonMapValue::SerializeTo(AnyToJsonConverter& converter,
                                             absl::Cord& value) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    value.Clear();
    return absl::OkStatus();
  }
  if (!value_->SerializePartialToCord(&value)) {
    return absl::UnknownError("failed to serialize protocol buffer message");
  }
  return absl::OkStatus();
}

absl::StatusOr<Json> ParsedJsonMapValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return JsonObject();
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return internal::ProtoJsonMapToNativeJsonMap(*generated_message);
  }
  return internal::ProtoJsonMapToNativeJsonMap(
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_));
}

absl::Status ParsedJsonMapValue::Equal(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  {
    ABSL_DCHECK(*this);
    if (auto other_value = other.AsParsedJsonMap(); other_value) {
      result = BoolValue(*this == *other_value);
      return absl::OkStatus();
    }
    if (auto other_value = other.AsMap(); other_value) {
      return common_internal::MapValueEqual(value_manager, MapValue(*this),
                                            *other_value, result);
    }
    result = BoolValue(false);
    return absl::OkStatus();
  }
}

absl::StatusOr<Value> ParsedJsonMapValue::Equal(ValueManager& value_manager,
                                                const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

namespace {

size_t SizeGenerated(const google::protobuf::Struct& message) {
  return static_cast<size_t>(
      well_known_types::StructReflection::FieldsSize(message));
}

size_t SizeDynamic(const google::protobuf::Message& message) {
  return static_cast<size_t>(
      well_known_types::GetStructReflectionOrDie(message.GetDescriptor())
          .FieldsSize(message));
}

}  // namespace

size_t ParsedJsonMapValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return 0;
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return SizeGenerated(*generated_message);
  }
  return SizeDynamic(google::protobuf::DownCastMessage<google::protobuf::Message>(*value_));
}

absl::Status ParsedJsonMapValue::Get(ValueManager& value_manager,
                                     const Value& key, Value& result) const {
  CEL_ASSIGN_OR_RETURN(bool ok, Find(value_manager, key, result));
  if (ABSL_PREDICT_FALSE(!ok) && !(result.IsError() || result.IsUnknown())) {
    return absl::NotFoundError(
        absl::StrCat("Key not found in map : ", key.DebugString()));
  }
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonMapValue::Get(ValueManager& value_manager,
                                              const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

namespace {

bool FindGenerated(Borrowed<const google::protobuf::Struct> message,
                   ValueManager& value_manager, const StringValue& key,
                   Value& result) {
  std::string key_scratch;
  if (const auto* value = well_known_types::StructReflection::FindField(
          *message, key.NativeString(key_scratch));
      value != nullptr) {
    result = common_internal::ParsedJsonValue(Borrowed(message, value));
    return true;
  }
  result = NullValue();
  return false;
}

bool FindDynamic(Borrowed<const google::protobuf::Message> message,
                 ValueManager& value_manager, const StringValue& key,
                 Value& result) {
  std::string key_scratch;
  if (const auto* value =
          well_known_types::GetStructReflectionOrDie(message->GetDescriptor())
              .FindField(*message, key.NativeString(key_scratch));
      value != nullptr) {
    result = common_internal::ParsedJsonValue(
        value_manager.GetMemoryManager().arena(), Borrowed(message, value));
    return true;
  }
  result = NullValue();
  return false;
}

}  // namespace

absl::StatusOr<bool> ParsedJsonMapValue::Find(ValueManager& value_manager,
                                              const Value& key,
                                              Value& result) const {
  ABSL_DCHECK(*this);
  if (key.IsError() || key.IsUnknown()) {
    result = key;
    return false;
  }
  if (auto string_key = key.AsString(); string_key) {
    if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
      result = NullValue();
      return false;
    }
    if (const auto* generated_message =
            google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
                cel::to_address(value_));
        generated_message != nullptr) {
      return FindGenerated(Borrowed(value_, generated_message), value_manager,
                           *string_key, result);
    }
    return FindDynamic(
        Borrowed(value_, google::protobuf::DownCastMessage<google::protobuf::Message>(
                             cel::to_address(value_))),
        value_manager, *string_key, result);
  }
  result = NullValue();
  return false;
}

absl::StatusOr<std::pair<Value, bool>> ParsedJsonMapValue::Find(
    ValueManager& value_manager, const Value& key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto found, Find(value_manager, key, result));
  if (found) {
    return std::pair{std::move(result), found};
  }
  return std::pair{NullValue(), found};
}

namespace {

bool HasGenerated(const google::protobuf::Struct& message,
                  ValueManager& value_manager, const StringValue& key) {
  std::string key_scratch;
  if (const auto* value = well_known_types::StructReflection::FindField(
          message, key.NativeString(key_scratch));
      value != nullptr) {
    return true;
  }
  return false;
}

bool HasDynamic(const google::protobuf::Message& message, ValueManager& value_manager,
                const StringValue& key) {
  std::string key_scratch;
  if (const auto* value =
          well_known_types::GetStructReflectionOrDie(message.GetDescriptor())
              .FindField(message, key.NativeString(key_scratch));
      value != nullptr) {
    return true;
  }
  return false;
}

}  // namespace

absl::Status ParsedJsonMapValue::Has(ValueManager& value_manager,
                                     const Value& key, Value& result) const {
  ABSL_DCHECK(*this);
  if (key.IsError() || key.IsUnknown()) {
    result = key;
    return absl::OkStatus();
  }
  if (auto string_key = key.AsString(); string_key) {
    if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
      result = BoolValue(false);
      return absl::OkStatus();
    }
    if (const auto* generated_message =
            google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
                cel::to_address(value_));
        generated_message != nullptr) {
      result = BoolValue(
          HasGenerated(*generated_message, value_manager, *string_key));
    } else {
      result = BoolValue(
          HasDynamic(google::protobuf::DownCastMessage<google::protobuf::Message>(*value_),
                     value_manager, *string_key));
    }
    return absl::OkStatus();
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonMapValue::Has(ValueManager& value_manager,
                                              const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

namespace {

class ParsedJsonMapValueKeysList final
    : public ParsedListValueInterface,
      public EnableSharedFromThis<ParsedJsonMapValueKeysList> {
 public:
  ParsedJsonMapValueKeysList(Owned<const google::protobuf::MessageLite> message,
                             absl::Nullable<google::protobuf::Arena*> keys_arena,
                             std::string* keys, size_t keys_size)
      : message_(std::move(message)),
        keys_arena_(keys_arena),
        keys_(keys),
        keys_size_(keys_size) {}

  ~ParsedJsonMapValueKeysList() override {
    // Called if this was allocated using reference counting.
    if (keys_arena_ == nullptr) {
      delete[] keys_;
    }
  }

  std::string DebugString() const override {
    std::string result;
    result.push_back('[');
    for (size_t i = 0; i < keys_size_; ++i) {
      if (i > 0) {
        result.append(", ");
      }
      result.append(internal::FormatStringLiteral(keys_[i]));
    }
    result.push_back(']');
    return result;
  }

  size_t Size() const override { return keys_size_; }

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const override {
    if (ABSL_PREDICT_FALSE(other.IsError() || other.IsUnknown())) {
      result = other;
      return absl::OkStatus();
    }
    if (const auto other_string = other.AsString(); other_string) {
      for (size_t i = 0; i < keys_size_; ++i) {
        if (keys_[i] == *other_string) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    }
    result = BoolValue(false);
    return absl::OkStatus();
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter&) const override {
    JsonArrayBuilder builder;
    builder.reserve(keys_size_);
    for (size_t i = 0; i < keys_size_; ++i) {
      builder.push_back(JsonString(keys_[i]));
    }
    return std::move(builder).Build();
  }

 protected:
  absl::Status GetImpl(ValueManager& value_manager, size_t index,
                       Value& result) const override {
    result =
        StringValue(value_manager.GetMemoryManager().arena(), keys_[index]);
    return absl::OkStatus();
  }

 private:
  friend struct cel::NativeTypeTraits<ParsedJsonMapValueKeysList>;

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<ParsedJsonMapValueKeysList>();
  }

  const Owned<const google::protobuf::MessageLite> message_;
  const absl::Nullable<google::protobuf::Arena*> keys_arena_;
  std::string* const keys_;
  const size_t keys_size_;
};

struct ArenaStringArray {
  absl::Nullable<std::string*> data;
  size_t size;
};

void DestroyArenaStringArray(void* strings) {
  std::destroy_n(reinterpret_cast<ArenaStringArray*>(strings)->data,
                 reinterpret_cast<ArenaStringArray*>(strings)->size);
}

}  // namespace

template <>
struct NativeTypeTraits<ParsedJsonMapValueKeysList> final {
  static NativeTypeId Id(const ParsedJsonMapValueKeysList& type) {
    return type.GetNativeTypeId();
  }

  static bool SkipDestructor(const ParsedJsonMapValueKeysList& type) {
    return NativeType::SkipDestructor(type.message_) &&
           type.keys_arena_ != nullptr;
  }
};

absl::Status ParsedJsonMapValue::ListKeys(ValueManager& value_manager,
                                          ListValue& result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    result = ListValue();
    return absl::OkStatus();
  }
  google::protobuf::Arena* arena = value_manager.GetMemoryManager().arena();
  size_t keys_size;
  std::string* keys;
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
              cel::to_address(value_));
      generated_message != nullptr) {
    keys_size = static_cast<size_t>(
        well_known_types::StructReflection::FieldsSize(*generated_message));
    auto keys_it =
        well_known_types::StructReflection::BeginFields(*generated_message);
    if (arena != nullptr) {
      keys = reinterpret_cast<std::string*>(arena->AllocateAligned(
          keys_size * sizeof(std::string), alignof(std::string)));
      for (size_t i = 0; i < keys_size; ++i, ++keys_it) {
        ::new (static_cast<void*>(keys + i)) std::string(keys_it->first);
      }
    } else {
      keys = new std::string[keys_size];
      for (size_t i = 0; i < keys_size; ++i, ++keys_it) {
        *(keys + i) = keys_it->first;
      }
    }
  } else {
    const auto* dynamic_message =
        google::protobuf::DownCastMessage<google::protobuf::Message>(cel::to_address(value_));
    const auto reflection = well_known_types::GetStructReflectionOrDie(
        dynamic_message->GetDescriptor());
    keys_size = static_cast<size_t>(reflection.FieldsSize(*dynamic_message));
    auto keys_it = reflection.BeginFields(*dynamic_message);
    if (arena != nullptr) {
      keys = reinterpret_cast<std::string*>(arena->AllocateAligned(
          keys_size * sizeof(std::string), alignof(std::string)));
      for (size_t i = 0; i < keys_size; ++i, ++keys_it) {
        ::new (static_cast<void*>(keys + i))
            std::string(keys_it.GetKey().GetStringValue());
      }
    } else {
      keys = new std::string[keys_size];
      for (size_t i = 0; i < keys_size; ++i, ++keys_it) {
        const auto& key = keys_it.GetKey().GetStringValue();
        (keys + i)->assign(key.data(), key.size());
      }
    }
  }
  if (arena != nullptr) {
    ArenaStringArray* array = google::protobuf::Arena::Create<ArenaStringArray>(arena);
    array->data = keys;
    array->size = keys_size;
    arena->OwnCustomDestructor(array, &DestroyArenaStringArray);
  }
  result = ParsedListValue(
      value_manager.GetMemoryManager().MakeShared<ParsedJsonMapValueKeysList>(
          value_, arena, keys, keys_size));
  return absl::OkStatus();
}

absl::StatusOr<ListValue> ParsedJsonMapValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
}

namespace {

absl::Status ForEachGenerated(Borrowed<const google::protobuf::Struct> message,
                              ValueManager& value_manager,
                              ParsedJsonMapValue::ForEachCallback callback) {
  Value key_scratch;
  Value value_scratch;
  auto map_begin = well_known_types::StructReflection::BeginFields(*message);
  const auto map_end = well_known_types::StructReflection::EndFields(*message);
  for (; map_begin != map_end; ++map_begin) {
    key_scratch = StringValue(message, map_begin->first);
    value_scratch =
        common_internal::ParsedJsonValue(Borrowed(message, &map_begin->second));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key_scratch, value_scratch));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::Status ForEachDynamic(Borrowed<const google::protobuf::Message> message,
                            ValueManager& value_manager,
                            ParsedJsonMapValue::ForEachCallback callback) {
  const auto reflection =
      well_known_types::GetStructReflectionOrDie(message->GetDescriptor());
  Value key_scratch;
  Value value_scratch;
  auto map_begin = reflection.BeginFields(*message);
  const auto map_end = reflection.EndFields(*message);
  for (; map_begin != map_end; ++map_begin) {
    // We have to copy until `google::protobuf::MapKey` is just a view.
    key_scratch = StringValue(value_manager.GetMemoryManager().arena(),
                              map_begin.GetKey().GetStringValue());
    value_scratch = common_internal::ParsedJsonValue(
        value_manager.GetMemoryManager().arena(),
        Borrowed(message, &map_begin.GetValueRef().GetMessageValue()));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key_scratch, value_scratch));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status ParsedJsonMapValue::ForEach(ValueManager& value_manager,
                                         ForEachCallback callback) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return absl::OkStatus();
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return ForEachGenerated(Borrowed(value_, generated_message), value_manager,
                            callback);
  }
  return ForEachDynamic(
      Borrowed(value_, google::protobuf::DownCastMessage<google::protobuf::Message>(
                           cel::to_address(value_))),
      value_manager, callback);
}

namespace {

class GeneratedParsedJsonMapValueIterator final : public ValueIterator {
 public:
  explicit GeneratedParsedJsonMapValueIterator(
      Owned<const google::protobuf::Struct> message)
      : message_(std::move(message)),
        begin_(well_known_types::StructReflection::BeginFields(*message_)),
        end_(well_known_types::StructReflection::EndFields(*message_)) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    result = StringValue(message_, begin_->first);
    ++begin_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Struct> message_;
  google::protobuf::Map<std::string, google::protobuf::Value>::const_iterator begin_;
  const google::protobuf::Map<std::string, google::protobuf::Value>::const_iterator end_;
};

class DynamicParsedJsonMapValueIterator final : public ValueIterator {
 public:
  explicit DynamicParsedJsonMapValueIterator(
      Owned<const google::protobuf::Message> message)
      : message_(std::move(message)),
        reflection_(well_known_types::GetStructReflectionOrDie(
            message_->GetDescriptor())),
        begin_(reflection_.BeginFields(*message_)),
        end_(reflection_.EndFields(*message_)) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    // We have to copy until `google::protobuf::MapKey` is just a view.
    std::string scratch =
        static_cast<std::string>(begin_.GetKey().GetStringValue());
    result = StringValue(value_manager.GetMemoryManager().arena(),
                         std::move(scratch));
    ++begin_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Message> message_;
  const well_known_types::StructReflection reflection_;
  google::protobuf::MapIterator begin_;
  const google::protobuf::MapIterator end_;
  std::string scratch_;
};

}  // namespace

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedJsonMapValue::NewIterator(ValueManager& value_manager) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return NewEmptyValueIterator();
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(
              cel::to_address(value_));
      generated_message != nullptr) {
    return std::make_unique<GeneratedParsedJsonMapValueIterator>(
        Owned(value_, generated_message));
  }
  return std::make_unique<DynamicParsedJsonMapValueIterator>(
      Owned(value_,
            google::protobuf::DownCastMessage<google::protobuf::Message>(cel::to_address(value_))));
}

bool operator==(const ParsedJsonMapValue& lhs, const ParsedJsonMapValue& rhs) {
  if (cel::to_address(lhs.value_) == cel::to_address(rhs.value_)) {
    return true;
  }
  if (cel::to_address(lhs.value_) == nullptr ||
      cel::to_address(rhs.value_) == nullptr) {
    return false;
  }
  return internal::JsonMapEquals(*lhs.value_, *rhs.value_);
}

}  // namespace cel
