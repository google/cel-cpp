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

#include "base/values/timestamp_value.h"

#include <string>
#include <utility>

#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "base/value_factory.h"
#include "common/any.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"
#include "internal/time.h"

namespace cel {

namespace {

using internal::ProtoWireEncoder;
using internal::ProtoWireTag;
using internal::ProtoWireType;

}  // namespace

CEL_INTERNAL_VALUE_IMPL(TimestampValue);

std::string TimestampValue::DebugString(absl::Time value) {
  return internal::DebugStringTimestamp(value);
}

std::string TimestampValue::DebugString() const { return DebugString(value()); }

absl::StatusOr<Any> TimestampValue::ConvertToAny(ValueFactory&) const {
  static constexpr absl::string_view kTypeName = "google.protobuf.Timestamp";
  auto value = this->value() - absl::UnixEpoch();
  if (ABSL_PREDICT_FALSE(value == absl::InfiniteDuration() ||
                         value == -absl::InfiniteDuration())) {
    return absl::FailedPreconditionError(
        "infinite timestamp values cannot be converted to google.protobuf.Any");
  }
  absl::Cord data;
  if (value != absl::ZeroDuration()) {
    auto seconds = absl::IDivDuration(value, absl::Seconds(1), &value);
    auto nanos = static_cast<int32_t>(
        absl::IDivDuration(value, absl::Nanoseconds(1), &value));
    ProtoWireEncoder encoder(kTypeName, data);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(seconds));
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(2, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(nanos));
    encoder.EnsureFullyEncoded();
  }
  return MakeAny(MakeTypeUrl(kTypeName), std::move(data));
}

absl::StatusOr<Json> TimestampValue::ConvertToJson(ValueFactory&) const {
  CEL_ASSIGN_OR_RETURN(auto formatted,
                       internal::EncodeTimestampToJson(value()));
  return JsonString(std::move(formatted));
}

absl::StatusOr<Handle<Value>> TimestampValue::Equals(
    ValueFactory& value_factory, const Value& other) const {
  return value_factory.CreateBoolValue(other.Is<TimestampValue>() &&
                                       value() ==
                                           other.As<TimestampValue>().value());
}

}  // namespace cel
