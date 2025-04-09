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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_FIELD_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_FIELD_VALUE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_map_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ValueIterator;
class ListValue;
class ParsedJsonMapValue;

// ParsedMapFieldValue is a MapValue over a map field of a parsed protocol
// buffer message.
class ParsedMapFieldValue final
    : private common_internal::MapValueMixin<ParsedMapFieldValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kMap;
  static constexpr absl::string_view kName = "map";

  ParsedMapFieldValue(const google::protobuf::Message* ABSL_NONNULL message,
                      const google::protobuf::FieldDescriptor* ABSL_NONNULL field,
                      google::protobuf::Arena* ABSL_NONNULL arena)
      : message_(message), field_(field), arena_(arena) {
    ABSL_DCHECK(message != nullptr);
    ABSL_DCHECK(field != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(field_->is_map())
        << field_->full_name() << " must be a map field";
    ABSL_DCHECK_OK(CheckArena(message_, arena_));
  }

  // Places the `ParsedMapFieldValue` into an invalid state. Anything
  // except assigning to `ParsedMapFieldValue` is undefined behavior.
  ParsedMapFieldValue() = default;

  ParsedMapFieldValue(const ParsedMapFieldValue&) = default;
  ParsedMapFieldValue(ParsedMapFieldValue&&) = default;
  ParsedMapFieldValue& operator=(const ParsedMapFieldValue&) = default;
  ParsedMapFieldValue& operator=(ParsedMapFieldValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  static constexpr absl::string_view GetTypeName() { return kName; }

  static MapType GetRuntimeType() { return MapType(); }

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::io::ZeroCopyOutputStream* ABSL_NONNULL output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Message* ABSL_NONNULL json) const;

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Message* ABSL_NONNULL json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
                     google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
                     google::protobuf::Arena* ABSL_NONNULL arena,
                     Value* ABSL_NONNULL result) const;
  using MapValueMixin::Equal;

  bool IsZeroValue() const;

  ParsedMapFieldValue Clone(google::protobuf::Arena* ABSL_NONNULL arena) const;

  bool IsEmpty() const;

  size_t Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(const Value& key,
                   const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
                   google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
                   google::protobuf::Arena* ABSL_NONNULL arena,
                   Value* ABSL_NONNULL result) const;
  using MapValueMixin::Get;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Arena* ABSL_NONNULL arena, Value* ABSL_NONNULL result) const;
  using MapValueMixin::Find;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(const Value& key,
                   const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
                   google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
                   google::protobuf::Arena* ABSL_NONNULL arena,
                   Value* ABSL_NONNULL result) const;
  using MapValueMixin::Has;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ListKeys(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Arena* ABSL_NONNULL arena, ListValue* ABSL_NONNULL result) const;
  using MapValueMixin::ListKeys;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename CustomMapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(
      ForEachCallback callback,
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Arena* ABSL_NONNULL arena) const;

  absl::StatusOr<ABSL_NONNULL std::unique_ptr<ValueIterator>> NewIterator()
      const;

  const google::protobuf::Message& message() const {
    ABSL_DCHECK(*this);
    return *message_;
  }

  const google::protobuf::FieldDescriptor* ABSL_NONNULL field() const {
    ABSL_DCHECK(*this);
    return field_;
  }

  // Returns `true` if `ParsedMapFieldValue` is in a valid state.
  explicit operator bool() const { return field_ != nullptr; }

  friend void swap(ParsedMapFieldValue& lhs,
                   ParsedMapFieldValue& rhs) noexcept {
    using std::swap;
    swap(lhs.message_, rhs.message_);
    swap(lhs.field_, rhs.field_);
    swap(lhs.arena_, rhs.arena_);
  }

 private:
  friend class ParsedJsonMapValue;
  friend class common_internal::ValueMixin<ParsedMapFieldValue>;
  friend class common_internal::MapValueMixin<ParsedMapFieldValue>;

  static absl::Status CheckArena(const google::protobuf::Message* ABSL_NULLABLE message,
                                 google::protobuf::Arena* ABSL_NONNULL arena) {
    if (message != nullptr && message->GetArena() != nullptr &&
        message->GetArena() != arena) {
      return absl::InvalidArgumentError(
          "message arena must be the same as arena");
    }
    return absl::OkStatus();
  }

  const google::protobuf::Reflection* ABSL_NONNULL GetReflection() const;

  const google::protobuf::Message* ABSL_NULLABLE message_ = nullptr;
  const google::protobuf::FieldDescriptor* ABSL_NULLABLE field_ = nullptr;
  google::protobuf::Arena* ABSL_NULLABLE arena_ = nullptr;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedMapFieldValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_FIELD_VALUE_H_
