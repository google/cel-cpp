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
#include "common/value.h"
#include "internal/overloaded.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"
#include "internal/strings.h"

namespace cel {

namespace {

template <typename Bytes>
std::string BytesDebugString(const Bytes& value) {
  return value.NativeValue(internal::Overloaded{
      [](absl::string_view string) -> std::string {
        return internal::FormatBytesLiteral(string);
      },
      [](const absl::Cord& cord) -> std::string {
        if (auto flat = cord.TryFlat(); flat.has_value()) {
          return internal::FormatBytesLiteral(*flat);
        }
        return internal::FormatBytesLiteral(static_cast<std::string>(cord));
      }});
}

}  // namespace

std::string BytesValue::DebugString() const { return BytesDebugString(*this); }

absl::StatusOr<size_t> BytesValue::GetSerializedSize() const {
  return NativeValue([](const auto& bytes) -> size_t {
    return internal::SerializedBytesValueSize(bytes);
  });
}

absl::Status BytesValue::SerializeTo(absl::Cord& value) const {
  return NativeValue([&value](const auto& bytes) -> absl::Status {
    return internal::SerializeBytesValue(bytes, value);
  });
}

absl::StatusOr<absl::Cord> BytesValue::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> BytesValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.BytesValue");
}

absl::StatusOr<Any> BytesValue::ConvertToAny(absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

std::string BytesValueView::DebugString() const {
  return BytesDebugString(*this);
}

absl::StatusOr<size_t> BytesValueView::GetSerializedSize() const {
  return NativeValue([](const auto& bytes) -> size_t {
    return internal::SerializedBytesValueSize(bytes);
  });
}

absl::Status BytesValueView::SerializeTo(absl::Cord& value) const {
  return NativeValue([&value](const auto& bytes) -> absl::Status {
    return internal::SerializeBytesValue(bytes, value);
  });
}

absl::StatusOr<absl::Cord> BytesValueView::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> BytesValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.BytesValue");
}

absl::StatusOr<Any> BytesValueView::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

}  // namespace cel
