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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_MAP_VALUE_H_

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
#include "common/values/map_value_interface.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel {

class Value;
class ValueManager;
class ListValue;
class ValueIterator;

// ParsedJsonMapValue is a MapValue backed by the google.protobuf.Struct
// well known message type.
class ParsedJsonMapValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kMap;
  static constexpr absl::string_view kName = "google.protobuf.Struct";

  template <typename T,
            typename = std::enable_if_t<std::disjunction_v<
                std::is_same<google::protobuf::Struct, absl::remove_cv_t<T>>,
                std::is_same<google::protobuf::Message, absl::remove_cv_t<T>>>>>
  explicit ParsedJsonMapValue(Owned<T> value)
      : ParsedJsonMapValue(Owned<const google::protobuf::MessageLite>(std::move(value))) {
  }

  // Constructs an empty `ParsedJsonMapValue`.
  ParsedJsonMapValue();

  ParsedJsonMapValue(const ParsedJsonMapValue&) = default;
  ParsedJsonMapValue(ParsedJsonMapValue&&) = default;
  ParsedJsonMapValue& operator=(const ParsedJsonMapValue&) = default;
  ParsedJsonMapValue& operator=(ParsedJsonMapValue&&) = default;

  static ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return kName; }

  static MapType GetRuntimeType() { return JsonMapType(); }

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  bool IsEmpty() const;

  size_t Size() const;

  absl::Status Get(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager,
                            const Value& key) const;

  absl::StatusOr<bool> Find(ValueManager& value_manager, const Value& key,
                            Value& result) const;
  absl::StatusOr<std::pair<Value, bool>> Find(ValueManager& value_manager,
                                              const Value& key) const;

  absl::Status Has(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Has(ValueManager& value_manager,
                            const Value& key) const;

  absl::Status ListKeys(ValueManager& value_manager, ListValue& result) const;
  absl::StatusOr<ListValue> ListKeys(ValueManager& value_manager) const;

  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>> NewIterator(
      ValueManager& value_manager) const;

  // Returns `true` if `ParsedJsonMapValue` is in a valid state. Currently only
  // moves place `ParsedJsonMapValue` in an invalid state.
  explicit operator bool() const { return static_cast<bool>(value_); }

  friend void swap(ParsedJsonMapValue& lhs, ParsedJsonMapValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  static bool IsStruct(const google::protobuf::MessageLite& message) {
    return google::protobuf::DynamicCastMessage<google::protobuf::Struct>(&message) !=
               nullptr ||
           google::protobuf::DownCastMessage<google::protobuf::Message>(message)
                   .GetDescriptor()
                   ->well_known_type() ==
               google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT;
  }

  explicit ParsedJsonMapValue(Owned<const google::protobuf::MessageLite> value)
      : value_(std::move(value)) {
    ABSL_DCHECK(!value_ || IsStruct(*value_))
        << value_->GetTypeName() << " must be google.protobuf.Struct";
  }

  // This is either the generated `google::protobuf::Struct` message, which
  // may be lite, or a dynamic message representing `google.protobuf.Struct`.
  Owned<const google::protobuf::MessageLite> value_;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedJsonMapValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_MAP_VALUE_H_
