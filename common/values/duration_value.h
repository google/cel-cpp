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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_

#include <ostream>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class DurationValue;
class TypeManager;

// `DurationValue` represents values of the primitive `duration` type.
class DurationValue final : private common_internal::ValueMixin<DurationValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kDuration;

  explicit DurationValue(absl::Duration value) noexcept : value_(value) {}

  DurationValue& operator=(absl::Duration value) noexcept {
    value_ = value;
    return *this;
  }

  DurationValue() = default;
  DurationValue(const DurationValue&) = default;
  DurationValue(DurationValue&&) = default;
  DurationValue& operator=(const DurationValue&) = default;
  DurationValue& operator=(DurationValue&&) = default;

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return DurationType::kName; }

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

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using ValueMixin::Equal;

  bool IsZeroValue() const { return NativeValue() == absl::ZeroDuration(); }

  absl::Duration NativeValue() const {
    return static_cast<absl::Duration>(*this);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::Duration() const noexcept { return value_; }

  friend void swap(DurationValue& lhs, DurationValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

  friend bool operator<(const DurationValue& lhs, const DurationValue& rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class common_internal::ValueMixin<DurationValue>;

  absl::Duration value_ = absl::ZeroDuration();
};

inline bool operator==(DurationValue lhs, DurationValue rhs) {
  return static_cast<absl::Duration>(lhs) == static_cast<absl::Duration>(rhs);
}

inline bool operator!=(DurationValue lhs, DurationValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
