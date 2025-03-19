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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MESSAGE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MESSAGE_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_struct_value.h"
#include "common/values/values.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class MessageValue;
class StructValue;
class Value;

class ParsedMessageValue final
    : private common_internal::StructValueMixin<ParsedMessageValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  using element_type = const google::protobuf::Message;

  ParsedMessageValue(
      absl::Nonnull<const google::protobuf::Message*> value ABSL_ATTRIBUTE_LIFETIME_BOUND,
      absl::Nonnull<google::protobuf::Arena*> arena ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_(value), arena_(arena) {
    ABSL_DCHECK(value != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(!value_ || !IsWellKnownMessageType(value_->GetDescriptor()))
        << value_->GetTypeName() << " is a well known type";
    ABSL_DCHECK(!value_ || value_->GetReflection() != nullptr)
        << value_->GetTypeName() << " is missing reflection";
    ABSL_DCHECK_OK(CheckArena(value_, arena_));
  }

  // Places the `ParsedMessageValue` into an invalid state. Anything except
  // assigning to `MessageValue` is undefined behavior.
  ParsedMessageValue() = default;

  ParsedMessageValue(const ParsedMessageValue&) = default;
  ParsedMessageValue(ParsedMessageValue&&) = default;
  ParsedMessageValue& operator=(const ParsedMessageValue&) = default;
  ParsedMessageValue& operator=(ParsedMessageValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  absl::string_view GetTypeName() const { return GetDescriptor()->full_name(); }

  MessageType GetRuntimeType() const { return MessageType(GetDescriptor()); }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const {
    return (*this)->GetDescriptor();
  }

  absl::Nonnull<const google::protobuf::Reflection*> GetReflection() const {
    return (*this)->GetReflection();
  }

  const google::protobuf::Message& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return *value_;
  }

  absl::Nonnull<const google::protobuf::Message*> operator->() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return value_;
  }

  bool IsZeroValue() const;

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

  // See Value::ConvertToJsonObject().
  absl::Status ConvertToJsonObject(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using StructValueMixin::Equal;

  ParsedMessageValue Clone(absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::Status GetFieldByName(
      absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using StructValueMixin::GetFieldByName;

  absl::Status GetFieldByNumber(
      int64_t number, ProtoWrapperTypeOptions unboxing_options,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using StructValueMixin::GetFieldByNumber;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = CustomStructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(
      ForEachFieldCallback callback,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena) const;

  absl::Status Qualify(
      absl::Span<const SelectQualifier> qualifiers, bool presence_test,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result,
      absl::Nonnull<int*> count) const;
  using StructValueMixin::Qualify;

  // Returns `true` if `ParsedMessageValue` is in a valid state.
  explicit operator bool() const { return value_ != nullptr; }

  friend void swap(ParsedMessageValue& lhs, ParsedMessageValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.arena_, rhs.arena_);
  }

 private:
  friend std::pointer_traits<ParsedMessageValue>;
  friend class StructValue;
  friend class common_internal::ValueMixin<ParsedMessageValue>;
  friend class common_internal::StructValueMixin<ParsedMessageValue>;

  static absl::Status CheckArena(absl::Nullable<const google::protobuf::Message*> message,
                                 absl::Nonnull<google::protobuf::Arena*> arena) {
    if (message != nullptr && message->GetArena() != nullptr &&
        message->GetArena() != arena) {
      return absl::InvalidArgumentError(
          "message arena must be the same as arena");
    }
    return absl::OkStatus();
  }

  absl::Status GetField(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      ProtoWrapperTypeOptions unboxing_options,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;

  bool HasField(absl::Nonnull<const google::protobuf::FieldDescriptor*> field) const;

  absl::Nullable<const google::protobuf::Message*> value_ = nullptr;
  absl::Nullable<google::protobuf::Arena*> arena_ = nullptr;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedMessageValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

namespace std {

template <>
struct pointer_traits<cel::ParsedMessageValue> {
  using pointer = cel::ParsedMessageValue;
  using element_type = typename cel::ParsedMessageValue::element_type;
  using difference_type = ptrdiff_t;

  static element_type* to_address(const pointer& p) noexcept {
    return cel::to_address(p.value_);
  }
};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MESSAGE_VALUE_H_
