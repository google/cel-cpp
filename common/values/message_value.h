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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MESSAGE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MESSAGE_VALUE_H_

#include <type_traits>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/parsed_message_value.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class Value;

class MessageValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  // NOLINTNEXTLINE(google-explicit-constructor)
  MessageValue(const ParsedMessageValue& other)
      : variant_(absl::in_place_type<ParsedMessageValue>, other) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  MessageValue(ParsedMessageValue&& other)
      : variant_(absl::in_place_type<ParsedMessageValue>, std::move(other)) {}

  // Places the `MessageValue` into an unspecified state. Anything except
  // assigning to `MessageValue` is undefined behavior.
  MessageValue() = default;

  MessageValue(const MessageValue&) = default;
  MessageValue(MessageValue&&) = default;
  MessageValue& operator=(const MessageValue&) = default;
  MessageValue& operator=(MessageValue&&) = default;

  static ValueKind kind() { return kKind; }

  absl::string_view GetTypeName() const { return GetDescriptor()->full_name(); }

  MessageType GetRuntimeType() const { return MessageType(GetDescriptor()); }

  absl::Nonnull<const google::protobuf::Descriptor*> GetDescriptor() const;

  bool IsParsed() const {
    return absl::holds_alternative<ParsedMessageValue>(variant_);
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>, bool> Is() const {
    return IsParsed();
  }

  cel::optional_ref<const ParsedMessageValue> AsParsed()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;

  cel::optional_ref<const ParsedMessageValue> AsParsed() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::optional<ParsedMessageValue> AsParsed() const&&;

  absl::optional<ParsedMessageValue> AsParsed() &&;

  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   cel::optional_ref<const ParsedMessageValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return IsParsed();
  }

  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       cel::optional_ref<const ParsedMessageValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AsParsed();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                   absl::optional<ParsedMessageValue>>
  As() const&& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::move(*this).AsParsed();
  }

  template <typename T>
      std::enable_if_t<std::is_same_v<ParsedMessageValue, T>,
                       absl::optional<ParsedMessageValue>>
      As() && ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::move(*this).AsParsed();
  }

  explicit operator const ParsedMessageValue&()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;

  explicit operator const ParsedMessageValue&() & ABSL_ATTRIBUTE_LIFETIME_BOUND;

  explicit operator ParsedMessageValue() const&&;

  explicit operator ParsedMessageValue() &&;

  explicit operator bool() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  friend void swap(MessageValue& lhs, MessageValue& rhs) noexcept {
    lhs.variant_.swap(rhs.variant_);
  }

 private:
  absl::variant<absl::monostate, ParsedMessageValue> variant_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MESSAGE_VALUE_H_
