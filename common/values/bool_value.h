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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_

#include <ostream>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class BoolValue;
class TypeManager;

// `BoolValue` represents values of the primitive `bool` type.
class BoolValue final : private common_internal::ValueMixin<BoolValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kBool;

  BoolValue() = default;
  BoolValue(const BoolValue&) = default;
  BoolValue(BoolValue&&) = default;
  BoolValue& operator=(const BoolValue&) = default;
  BoolValue& operator=(BoolValue&&) = default;

  explicit BoolValue(bool value) noexcept : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator bool() const noexcept { return value_; }

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return BoolType::kName; }

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

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using ValueMixin::Equal;

  bool IsZeroValue() const { return NativeValue() == false; }

  bool NativeValue() const { return static_cast<bool>(*this); }

  friend void swap(BoolValue& lhs, BoolValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  friend class common_internal::ValueMixin<BoolValue>;

  bool value_ = false;
};

template <typename H>
H AbslHashValue(H state, BoolValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

inline std::ostream& operator<<(std::ostream& out, BoolValue value) {
  return out << value.DebugString();
}

inline BoolValue FalseValue() noexcept { return BoolValue(false); }

inline BoolValue TrueValue() noexcept { return BoolValue(true); }

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BOOL_VALUE_H_
