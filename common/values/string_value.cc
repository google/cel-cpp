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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/overloaded.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"
#include "internal/strings.h"

namespace cel {

namespace {

template <typename Bytes>
std::string StringDebugString(const Bytes& value) {
  return value.NativeValue(internal::Overloaded{
      [](absl::string_view string) -> std::string {
        return internal::FormatStringLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatStringLiteral(*flat);
        }
        return internal::FormatStringLiteral(static_cast<std::string>(cord));
      }});
}

}  // namespace

std::string StringValue::DebugString() const {
  return StringDebugString(*this);
}

absl::StatusOr<size_t> StringValue::GetSerializedSize() const {
  return NativeValue([](const auto& bytes) -> size_t {
    return internal::SerializedStringValueSize(bytes);
  });
}

absl::Status StringValue::SerializeTo(absl::Cord& value) const {
  return NativeValue([&value](const auto& bytes) -> absl::Status {
    return internal::SerializeStringValue(bytes, value);
  });
}

absl::StatusOr<absl::Cord> StringValue::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> StringValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.StringValue");
}

absl::StatusOr<Any> StringValue::ConvertToAny(absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> StringValue::ConvertToJson() const { return NativeCord(); }

absl::StatusOr<ValueView> StringValue::Equal(ValueManager&, ValueView other,
                                             Value&) const {
  if (auto other_value = As<StringValueView>(other); other_value.has_value()) {
    return NativeValue([other_value](const auto& value) -> BoolValueView {
      return other_value->NativeValue(
          [&value](const auto& other_value) -> BoolValueView {
            return BoolValueView{value == other_value};
          });
    });
  }
  return BoolValueView{false};
}

std::string StringValueView::DebugString() const {
  return StringDebugString(*this);
}

absl::StatusOr<size_t> StringValueView::GetSerializedSize() const {
  return NativeValue([](const auto& bytes) -> size_t {
    return internal::SerializedStringValueSize(bytes);
  });
}

absl::Status StringValueView::SerializeTo(absl::Cord& value) const {
  return NativeValue([&value](const auto& bytes) -> absl::Status {
    return internal::SerializeStringValue(bytes, value);
  });
}

absl::StatusOr<absl::Cord> StringValueView::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> StringValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.StringValue");
}

absl::StatusOr<Any> StringValueView::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> StringValueView::ConvertToJson() const {
  return NativeCord();
}

absl::StatusOr<ValueView> StringValueView::Equal(ValueManager&, ValueView other,
                                                 Value&) const {
  if (auto other_value = As<StringValueView>(other); other_value.has_value()) {
    return NativeValue([other_value](const auto& value) -> BoolValueView {
      return other_value->NativeValue(
          [&value](const auto& other_value) -> BoolValueView {
            return BoolValueView{value == other_value};
          });
    });
  }
  return BoolValueView{false};
}

}  // namespace cel
