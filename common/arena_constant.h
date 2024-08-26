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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ARENA_CONSTANT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ARENA_CONSTANT_H_

#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/arena_bytes.h"
#include "common/arena_string.h"
#include "common/constant.h"
#include "internal/time.h"

namespace cel {

enum class ArenaConstantKind {
  kUnspecified = 0,
  kNull,
  kBool,
  kInt,
  kUint,
  kDouble,
  kBytes,
  kString,
  kDuration,
  kTimestamp,
};

absl::string_view ArenaConstantKindName(ArenaConstantKind constant_kind);

template <typename S>
void AbslStringify(S& sink, ArenaConstantKind constant_kind) {
  sink.Append(ArenaConstantKindName(constant_kind));
}

using ArenaConstantValue =
    absl::variant<absl::monostate, std::nullptr_t, bool, int64_t, uint64_t,
                  double, ArenaBytes, ArenaString, absl::Duration, absl::Time>;

class ArenaConstant final {
 public:
  using Kind = ArenaConstantKind;

  ArenaConstant() = default;
  ArenaConstant(const ArenaConstant&) = default;
  ArenaConstant& operator=(const ArenaConstant&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(std::nullptr_t value)
      : value_(absl::in_place_type<std::nullptr_t>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(bool value) : value_(absl::in_place_type<bool>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(int64_t value) : value_(absl::in_place_type<int64_t>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(uint64_t value)
      : value_(absl::in_place_type<uint64_t>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(double value) : value_(absl::in_place_type<double>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(ArenaBytes value)
      : value_(absl::in_place_type<ArenaBytes>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(ArenaString value)
      : value_(absl::in_place_type<ArenaString>, value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(absl::Duration value)
      : value_(absl::in_place_type<absl::Duration>, value) {
    ABSL_DCHECK_OK(cel::internal::ValidateDuration(value));
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  ArenaConstant(absl::Time value)
      : value_(absl::in_place_type<absl::Time>, value) {
    ABSL_DCHECK_OK(cel::internal::ValidateTimestamp(value));
  }

  Kind kind() const { return static_cast<Kind>(value_.index()); }

  bool IsUnspecified() const { return kind() == Kind::kUnspecified; }

  bool IsNull() const { return kind() == Kind::kNull; }

  bool IsBool() const { return kind() == Kind::kBool; }

  bool IsInt() const { return kind() == Kind::kInt; }

  bool IsUint() const { return kind() == Kind::kUint; }

  bool IsDouble() const { return kind() == Kind::kDouble; }

  bool IsBytes() const { return kind() == Kind::kBytes; }

  bool IsString() const { return kind() == Kind::kString; }

  bool IsDuration() const { return kind() == Kind::kDuration; }

  bool IsTimestamp() const { return kind() == Kind::kTimestamp; }

  absl::optional<absl::monostate> AsUnspecified() const {
    if (const auto* alternative = absl::get_if<absl::monostate>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<std::nullptr_t> AsNull() const {
    if (const auto* alternative = absl::get_if<std::nullptr_t>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<bool> AsBool() const {
    if (const auto* alternative = absl::get_if<bool>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<int64_t> AsInt() const {
    if (const auto* alternative = absl::get_if<int64_t>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<uint64_t> AsUint() const {
    if (const auto* alternative = absl::get_if<uint64_t>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<double> AsDouble() const {
    if (const auto* alternative = absl::get_if<double>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<ArenaBytes> AsBytes() const {
    if (const auto* alternative = absl::get_if<ArenaBytes>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<ArenaString> AsString() const {
    if (const auto* alternative = absl::get_if<ArenaString>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<absl::Duration> AsDuration() const {
    if (const auto* alternative = absl::get_if<absl::Duration>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  absl::optional<absl::Time> AsTimestamp() const {
    if (const auto* alternative = absl::get_if<absl::Time>(&value());
        alternative != nullptr) {
      return *alternative;
    }
    return absl::nullopt;
  }

  explicit operator absl::monostate() const {
    ABSL_DCHECK(IsUnspecified());
    return absl::get<absl::monostate>(value());
  }

  explicit operator std::nullptr_t() const {
    ABSL_DCHECK(IsNull());
    return absl::get<std::nullptr_t>(value());
  }

  explicit operator bool() const {
    ABSL_DCHECK(IsBool());
    return absl::get<bool>(value());
  }

  explicit operator int64_t() const {
    ABSL_DCHECK(IsInt());
    return absl::get<int64_t>(value());
  }

  explicit operator uint64_t() const {
    ABSL_DCHECK(IsUint());
    return absl::get<uint64_t>(value());
  }

  explicit operator double() const {
    ABSL_DCHECK(IsDouble());
    return absl::get<double>(value());
  }

  explicit operator ArenaBytes() const {
    ABSL_DCHECK(IsBytes());
    return absl::get<ArenaBytes>(value());
  }

  explicit operator ArenaString() const {
    ABSL_DCHECK(IsString());
    return absl::get<ArenaString>(value());
  }

  explicit operator absl::Duration() const {
    ABSL_DCHECK(IsDuration());
    return absl::get<absl::Duration>(value());
  }

  explicit operator absl::Time() const {
    ABSL_DCHECK(IsTimestamp());
    return absl::get<absl::Time>(value());
  }

  friend bool operator==(const ArenaConstant& lhs, const ArenaConstant& rhs) {
    return lhs.value() == rhs.value();
  }

  template <typename H>
  friend H AbslHashValue(H state, const ArenaConstant& constant) {
    return H::combine(std::move(state), constant.value());
  }

 private:
  // Variant holding the content. We do not expose it directly because once we
  // do we cannot easily add additional types without breaking source
  // compatibility. `std::visit` requires being exhaustive, but definition
  // adding another type would break existing code.
  using Value = absl::variant<absl::monostate, std::nullptr_t, bool, int64_t,
                              uint64_t, double, ArenaBytes, ArenaString,
                              absl::Duration, absl::Time>;

  const Value& value() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return value_; }

  Value& value() ABSL_ATTRIBUTE_LIFETIME_BOUND { return value_; }

  Value value_;
};

inline bool operator!=(const ArenaConstant& lhs, const ArenaConstant& rhs) {
  return !operator==(lhs, rhs);
}

ArenaConstant MakeArenaConstant(
    absl::Nonnull<ArenaStringPool*> string_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
    absl::Nonnull<ArenaBytesPool*> bytes_pool ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const Constant& constant);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_CONSTANT_H_
