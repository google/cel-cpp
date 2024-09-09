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
#include <type_traits>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/list_value_interface.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel {

class Value;
class ValueManager;
class ValueIterator;

// ParsedJsonListValue is a ListValue backed by the google.protobuf.ListValue
// well known message type.
class ParsedJsonListValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kList;
  static constexpr absl::string_view kName = "google.protobuf.ListValue";

  template <typename T,
            typename = std::enable_if_t<std::disjunction_v<
                std::is_same<google::protobuf::ListValue, absl::remove_cv_t<T>>,
                std::is_same<google::protobuf::Message, absl::remove_cv_t<T>>>>>
  explicit ParsedJsonListValue(Owned<T> value)
      : ParsedJsonListValue(
            Owned<const google::protobuf::MessageLite>(std::move(value))) {}

  // Constructs an empty `ParsedJsonListValue`.
  ParsedJsonListValue();

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
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  bool IsEmpty() const;

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

  // Returns `true` if `ParsedJsonListValue` is in a valid state. Currently only
  // moves place `ParsedJsonListValue` in an invalid state.
  explicit operator bool() const { return static_cast<bool>(value_); }

  friend void swap(ParsedJsonListValue& lhs,
                   ParsedJsonListValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  static bool IsListValue(const google::protobuf::MessageLite& message) {
    return google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(&message) !=
               nullptr ||
           google::protobuf::DownCastMessage<google::protobuf::Message>(message)
                   .GetDescriptor()
                   ->well_known_type() ==
               google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE;
  }

  explicit ParsedJsonListValue(Owned<const google::protobuf::MessageLite> value)
      : value_(std::move(value)) {
    ABSL_DCHECK(!value_ || IsListValue(*value_))
        << value_->GetTypeName() << " must be google.protobuf.ListValue";
  }

  // This is either the generated `google::protobuf::ListValue` message, which
  // may be lite, or a dynamic message representing `google.protobuf.ListValue`.
  Owned<const google::protobuf::MessageLite> value_;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedJsonListValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_LIST_VALUE_H_
