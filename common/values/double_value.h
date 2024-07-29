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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class Value;
class ValueManager;
class DoubleValue;
class TypeManager;

class DoubleValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kDouble;

  explicit DoubleValue(double value) noexcept : value_(value) {}

  template <typename T,
            typename = std::enable_if_t<std::conjunction_v<
                std::is_floating_point<T>, std::is_convertible<T, double>>>>
  DoubleValue& operator=(T value) noexcept {
    value_ = value;
    return *this;
  }

  DoubleValue() = default;
  DoubleValue(const DoubleValue&) = default;
  DoubleValue(DoubleValue&&) = default;
  DoubleValue& operator=(const DoubleValue&) = default;
  DoubleValue& operator=(DoubleValue&&) = default;

  ValueKind kind() const { return kKind; }

  DoubleType GetType(TypeManager&) const { return DoubleType(); }

  absl::string_view GetTypeName() const { return DoubleType::kName; }

  std::string DebugString() const;

  // `SerializeTo` serializes this value and appends it to `value`.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter&) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return NativeValue() == 0.0; }

  double NativeValue() const { return static_cast<double>(*this); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator double() const noexcept { return value_; }

  friend void swap(DoubleValue& lhs, DoubleValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  double value_ = 0.0;
};

inline std::ostream& operator<<(std::ostream& out, DoubleValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DOUBLE_VALUE_H_
