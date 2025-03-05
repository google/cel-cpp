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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_REPEATED_FIELD_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_REPEATED_FIELD_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_list_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ValueIterator;
class ParsedJsonListValue;

// ParsedRepeatedFieldValue is a ListValue over a repeated field of a parsed
// protocol buffer message.
class ParsedRepeatedFieldValue final
    : private common_internal::ListValueMixin<ParsedRepeatedFieldValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;
  static constexpr absl::string_view kName = "list";

  ParsedRepeatedFieldValue(Owned<const google::protobuf::Message> message,
                           absl::Nonnull<const google::protobuf::FieldDescriptor*> field)
      : message_(std::move(message)), field_(field) {
    ABSL_DCHECK(field_->is_repeated() && !field_->is_map())
        << field_->full_name() << " must be a repeated field";
  }

  // Places the `ParsedRepeatedFieldValue` into an invalid state. Anything
  // except assigning to `ParsedRepeatedFieldValue` is undefined behavior.
  ParsedRepeatedFieldValue() = default;

  ParsedRepeatedFieldValue(const ParsedRepeatedFieldValue&) = default;
  ParsedRepeatedFieldValue(ParsedRepeatedFieldValue&&) = default;
  ParsedRepeatedFieldValue& operator=(const ParsedRepeatedFieldValue&) =
      default;
  ParsedRepeatedFieldValue& operator=(ParsedRepeatedFieldValue&&) = default;

  static ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return kName; }

  static ListType GetRuntimeType() { return ListType(); }

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<absl::Cord*> value) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  // See Value::ConvertToJsonArray().
  absl::Status ConvertToJsonArray(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using ListValueMixin::Equal;

  bool IsZeroValue() const;

  bool IsEmpty() const;

  ParsedRepeatedFieldValue Clone(absl::Nonnull<google::protobuf::Arena*> arena) const;

  size_t Size() const;

  // See ListValueInterface::Get for documentation.
  absl::Status Get(size_t index,
                   absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
                   absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
                   absl::Nonnull<google::protobuf::Arena*> arena,
                   absl::Nonnull<Value*> result) const;
  using ListValueMixin::Get;

  using ForEachCallback = typename CustomListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename CustomListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(
      ForEachWithIndexCallback callback,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;
  using ListValueMixin::ForEach;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator() const;

  absl::Status Contains(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using ListValueMixin::Contains;

  const google::protobuf::Message& message() const {
    ABSL_DCHECK(*this);
    return *message_;
  }

  absl::Nonnull<const google::protobuf::FieldDescriptor*> field() const {
    ABSL_DCHECK(*this);
    return field_;
  }

  // Returns `true` if `ParsedRepeatedFieldValue` is in a valid state.
  explicit operator bool() const { return field_ != nullptr; }

  friend void swap(ParsedRepeatedFieldValue& lhs,
                   ParsedRepeatedFieldValue& rhs) noexcept {
    using std::swap;
    swap(lhs.message_, rhs.message_);
    swap(lhs.field_, rhs.field_);
  }

 private:
  friend class ParsedJsonListValue;
  friend class common_internal::ValueMixin<ParsedRepeatedFieldValue>;
  friend class common_internal::ListValueMixin<ParsedRepeatedFieldValue>;

  absl::Nonnull<const google::protobuf::Reflection*> GetReflection() const;

  Owned<const google::protobuf::Message> message_;
  absl::Nullable<const google::protobuf::FieldDescriptor*> field_ = nullptr;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedRepeatedFieldValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_REPEATED_FIELD_VALUE_H_
