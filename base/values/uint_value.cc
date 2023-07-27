// Copyright 2022 Google LLC
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

#include "base/values/uint_value.h"

#include <string>
#include <utility>

#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::ProtoWireEncoder;
using internal::ProtoWireTag;
using internal::ProtoWireType;

}  // namespace

CEL_INTERNAL_VALUE_IMPL(UintValue);

std::string UintValue::DebugString(uint64_t value) {
  return absl::StrCat(value, "u");
}

std::string UintValue::DebugString() const { return DebugString(value()); }

absl::StatusOr<Any> UintValue::ConvertToAny(ValueFactory&) const {
  static constexpr absl::string_view kTypeName = "google.protobuf.UInt64Value";
  const auto value = this->value();
  absl::Cord data;
  if (value) {
    ProtoWireEncoder encoder(kTypeName, data);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(value));
    encoder.EnsureFullyEncoded();
  }
  return MakeAny(MakeTypeUrl(kTypeName), std::move(data));
}

absl::StatusOr<Json> UintValue::ConvertToJson(ValueFactory&) const {
  return JsonUint(value());
}

}  // namespace cel
