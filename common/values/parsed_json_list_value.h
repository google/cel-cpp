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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_LIST_VALUE_H_

#include <cstddef>
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
#include "common/allocator.h"
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
class ValueManager;
class ValueIterator;
class ParsedRepeatedFieldValue;

namespace common_internal {
absl::Status CheckWellKnownListValueMessage(const google::protobuf::Message& message);
}  // namespace common_internal

// ParsedJsonListValue is a ListValue backed by the google.protobuf.ListValue
// well known message type.
class ParsedJsonListValue final
    : private common_internal::ListValueMixin<ParsedJsonListValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;
  static constexpr absl::string_view kName = "google.protobuf.ListValue";

  using element_type = const google::protobuf::Message;

  explicit ParsedJsonListValue(Owned<const google::protobuf::Message> value)
      : value_(std::move(value)) {
    ABSL_DCHECK_OK(CheckListValue(cel::to_address(value_)));
  }

  // Constructs an empty `ParsedJsonListValue`.
  ParsedJsonListValue() = default;
  ParsedJsonListValue(const ParsedJsonListValue&) = default;
  ParsedJsonListValue(ParsedJsonListValue&&) = default;
  ParsedJsonListValue& operator=(const ParsedJsonListValue&) = default;
  ParsedJsonListValue& operator=(ParsedJsonListValue&&) = default;

  static ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return kName; }

  static ListType GetRuntimeType() { return JsonListType(); }

  const google::protobuf::Message& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return *value_;
  }

  absl::Nonnull<const google::protobuf::Message*> operator->() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return value_.operator->();
  }

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Cord& value) const;

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

  bool IsZeroValue() const { return IsEmpty(); }

  ParsedJsonListValue Clone(Allocator<> allocator) const;

  bool IsEmpty() const { return Size() == 0; }

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

  explicit operator bool() const { return static_cast<bool>(value_); }

  friend void swap(ParsedJsonListValue& lhs,
                   ParsedJsonListValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

  friend bool operator==(const ParsedJsonListValue& lhs,
                         const ParsedJsonListValue& rhs);

 private:
  friend std::pointer_traits<ParsedJsonListValue>;
  friend class ParsedRepeatedFieldValue;
  friend class common_internal::ValueMixin<ParsedJsonListValue>;
  friend class common_internal::ListValueMixin<ParsedJsonListValue>;

  static absl::Status CheckListValue(
      absl::Nullable<const google::protobuf::Message*> message) {
    return message == nullptr
               ? absl::OkStatus()
               : common_internal::CheckWellKnownListValueMessage(*message);
  }

  Owned<const google::protobuf::Message> value_;
};

inline bool operator!=(const ParsedJsonListValue& lhs,
                       const ParsedJsonListValue& rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedJsonListValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

namespace std {

template <>
struct pointer_traits<cel::ParsedJsonListValue> {
  using pointer = cel::ParsedJsonListValue;
  using element_type = typename cel::ParsedJsonListValue::element_type;
  using difference_type = ptrdiff_t;

  static element_type* to_address(const pointer& p) noexcept {
    return cel::to_address(p.value_);
  }
};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_LIST_VALUE_H_
