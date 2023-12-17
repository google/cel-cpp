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

// TODO(uncreated-issue/61): finish implementing this is just a placeholder for now

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class UnknownValue;
class UnknownValueView;

// `UnknownValue` represents values of the primitive `duration` type.
class UnknownValue final {
 public:
  using view_alternative_type = UnknownValueView;

  static constexpr ValueKind kKind = ValueKind::kUnknown;

  explicit UnknownValue(UnknownValueView) noexcept;

  UnknownValue() = default;
  UnknownValue(const UnknownValue&) = default;
  UnknownValue(UnknownValue&&) = default;
  UnknownValue& operator=(const UnknownValue&) = default;
  UnknownValue& operator=(UnknownValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  UnknownTypeView type() const { return UnknownTypeView(); }

  std::string DebugString() const { return ""; }

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `UnknownValue`
  // is not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `UnknownValue` is
  // not serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  void swap(UnknownValue& other) noexcept {}

 private:
  friend class UnknownValueView;
};

inline void swap(UnknownValue& lhs, UnknownValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, const UnknownValue& value) {
  return out << value.DebugString();
}

class UnknownValueView final {
 public:
  using alternative_type = UnknownValue;

  static constexpr ValueKind kKind = UnknownValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  UnknownValueView(const UnknownValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND
                       ABSL_ATTRIBUTE_UNUSED) noexcept {}

  UnknownValueView() = default;
  UnknownValueView(const UnknownValueView&) = default;
  UnknownValueView(UnknownValueView&&) = default;
  UnknownValueView& operator=(const UnknownValueView&) = default;
  UnknownValueView& operator=(UnknownValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  UnknownTypeView type() const { return UnknownTypeView(); }

  std::string DebugString() const { return ""; }

  // `GetSerializedSize` always returns `FAILED_PRECONDITION` as `UnknownValue`
  // is not serializable.
  absl::StatusOr<size_t> GetSerializedSize() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::Status SerializeTo(absl::Cord& value) const;

  // `Serialize` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<absl::Cord> Serialize() const;

  // `GetTypeUrl` always returns `FAILED_PRECONDITION` as `UnknownValue` is not
  // serializable.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // `ConvertToAny` always returns `FAILED_PRECONDITION` as `UnknownValue` is
  // not serializable.
  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  void swap(UnknownValueView& other) noexcept {}

 private:
  friend class UnknownValue;
};

inline void swap(UnknownValueView& lhs, UnknownValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, UnknownValueView value) {
  return out << value.DebugString();
}

inline UnknownValue::UnknownValue(UnknownValueView) noexcept {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_
