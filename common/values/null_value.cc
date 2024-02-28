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
#include "internal/serialize.h"
#include "internal/status_macros.h"

namespace cel {

absl::StatusOr<size_t> NullValue::GetSerializedSize(ValueManager&) const {
  return internal::SerializedValueSize(kJsonNull);
}

absl::Status NullValue::SerializeTo(ValueManager&, absl::Cord& value) const {
  return internal::SerializeValue(kJsonNull, value);
}

absl::StatusOr<absl::Cord> NullValue::Serialize(
    ValueManager& value_manager) const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, value));
  return value;
}

absl::StatusOr<std::string> NullValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Value");
}

absl::StatusOr<Any> NullValue::ConvertToAny(ValueManager& value_manager,
                                            absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<ValueView> NullValue::Equal(ValueManager&, ValueView other,
                                           Value&) const {
  return BoolValueView{InstanceOf<NullValueView>(other)};
}

absl::StatusOr<size_t> NullValueView::GetSerializedSize(ValueManager&) const {
  return internal::SerializedValueSize(kJsonNull);
}

absl::Status NullValueView::SerializeTo(ValueManager&,
                                        absl::Cord& value) const {
  return internal::SerializeValue(kJsonNull, value);
}

absl::StatusOr<absl::Cord> NullValueView::Serialize(
    ValueManager& value_manager) const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, value));
  return value;
}

absl::StatusOr<std::string> NullValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Value");
}

absl::StatusOr<Any> NullValueView::ConvertToAny(
    ValueManager& value_manager, absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<ValueView> NullValueView::Equal(ValueManager&, ValueView other,
                                               Value&) const {
  return BoolValueView{InstanceOf<NullValueView>(other)};
}

}  // namespace cel
