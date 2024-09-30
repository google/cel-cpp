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
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/list_value_interface.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ValueManager;
class ValueIterator;

namespace common_internal {
absl::Status CheckWellKnownListValueMessage(const google::protobuf::Message& message);
}  // namespace common_internal

// ParsedJsonListValue is a ListValue backed by the google.protobuf.ListValue
// well known message type.
class ParsedJsonListValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;
  static constexpr absl::string_view kName = "google.protobuf.ListValue";

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

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const {
    CEL_ASSIGN_OR_RETURN(auto value, ConvertToJson(converter));
    return absl::get<JsonArray>(std::move(value));
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return IsEmpty(); }

  bool IsEmpty() const { return Size() == 0; }

  size_t Size() const;

  // See ListValueInterface::Get for documentation.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager, size_t index) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename ListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>> NewIterator(
      ValueManager& value_manager) const;

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const;
  absl::StatusOr<Value> Contains(ValueManager& value_manager,
                                 const Value& other) const;

  friend void swap(ParsedJsonListValue& lhs,
                   ParsedJsonListValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

  friend bool operator==(const ParsedJsonListValue& lhs,
                         const ParsedJsonListValue& rhs);

 private:
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

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_LIST_VALUE_H_
