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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/int_value.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_util.h"
#include "google/protobuf/message.h"

namespace cel {
namespace common_internal {

template <typename T, typename U = absl::remove_cv_t<T>>
inline constexpr bool kIsWellKnownEnumType =
    std::is_same<google::protobuf::NullValue, U>::value;

template <typename T, typename U = absl::remove_cv_t<T>>
inline constexpr bool kIsGeneratedEnum = google::protobuf::is_proto_enum<U>::value;

template <typename T, typename U, typename R = void>
using EnableIfWellKnownEnum = std::enable_if_t<
    kIsWellKnownEnumType<T> && std::is_same<absl::remove_cv_t<T>, U>::value, R>;

template <typename T, typename R = void>
using EnableIfGeneratedEnum = std::enable_if_t<
    absl::conjunction<
        std::bool_constant<kIsGeneratedEnum<T>>,
        absl::negation<std::bool_constant<kIsWellKnownEnumType<T>>>>::value,
    R>;

}  // namespace common_internal

class Value;
class ValueManager;
class IntValue;
class TypeManager;
class EnumValue;

// `EnumValue` represents protobuf enum values which behave like values of the
// primitive `int` type, except that they return the enum name in
// `DebugString()`.
class EnumValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kInt;

  explicit EnumValue(
      absl::Nonnull<const google::protobuf::EnumValueDescriptor*> value) noexcept
      : value_(value->number()), name_(value->name()) {}
  explicit EnumValue(absl::string_view name, int64_t value) noexcept
      : value_(value), name_(name) {}

  EnumValue(const EnumValue&) = default;
  EnumValue(EnumValue&&) = default;
  EnumValue& operator=(const EnumValue&) = default;
  EnumValue& operator=(EnumValue&&) = default;

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return IntType::kName; }

  absl::string_view GetEnumName() const { return name_; }

  std::string DebugString() const { return std::string(GetEnumName()); }

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Cord& value) const {
    return IntValue(NativeValue())
        .SerializeTo(descriptor_pool, message_factory, value);
  }

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const {
    return IntValue(NativeValue())
        .ConvertToJson(descriptor_pool, message_factory, json);
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const {
    return IntValue(NativeValue()).Equal(value_manager, other, result);
  }
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return NativeValue() == 0; }

  int64_t NativeValue() const { return static_cast<int64_t>(*this); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator int64_t() const noexcept { return value_; }

  friend void swap(EnumValue& lhs, EnumValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.name_, rhs.name_);
  }

 private:
  int64_t value_;
  absl::string_view name_;
};

template <typename H>
H AbslHashValue(H state, EnumValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

inline bool operator==(EnumValue lhs, EnumValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

inline bool operator!=(EnumValue lhs, EnumValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, EnumValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_ENUM_VALUE_H_
