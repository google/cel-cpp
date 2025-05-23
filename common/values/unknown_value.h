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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/unknown.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class UnknownValue;

// `UnknownValue` represents values of the primitive `duration` type.
class UnknownValue final : private common_internal::ValueMixin<UnknownValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kUnknown;

  explicit UnknownValue(Unknown unknown) : unknown_(std::move(unknown)) {}

  UnknownValue() = default;
  UnknownValue(const UnknownValue&) = default;
  UnknownValue(UnknownValue&&) = default;
  UnknownValue& operator=(const UnknownValue&) = default;
  UnknownValue& operator=(UnknownValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return UnknownType::kName; }

  std::string DebugString() const { return ""; }

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::io::ZeroCopyOutputStream* ABSL_NONNULL output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
      google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
      google::protobuf::Message* ABSL_NONNULL json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* ABSL_NONNULL descriptor_pool,
                     google::protobuf::MessageFactory* ABSL_NONNULL message_factory,
                     google::protobuf::Arena* ABSL_NONNULL arena,
                     Value* ABSL_NONNULL result) const;
  using ValueMixin::Equal;

  bool IsZeroValue() const { return false; }

  void swap(UnknownValue& other) noexcept {
    using std::swap;
    swap(unknown_, other.unknown_);
  }

  const Unknown& NativeValue() const& ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return unknown_;
  }

  Unknown NativeValue() && {
    Unknown unknown = std::move(unknown_);
    return unknown;
  }

  const AttributeSet& attribute_set() const {
    return unknown_.unknown_attributes();
  }

  const FunctionResultSet& function_result_set() const {
    return unknown_.unknown_function_results();
  }

 private:
  friend class common_internal::ValueMixin<UnknownValue>;

  Unknown unknown_;
};

inline void swap(UnknownValue& lhs, UnknownValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, const UnknownValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_UNKNOWN_VALUE_H_
