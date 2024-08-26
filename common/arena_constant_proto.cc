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

#include "common/arena_constant_proto.h"

#include <cstdint>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "common/arena_bytes.h"
#include "common/arena_bytes_pool.h"
#include "common/arena_constant.h"
#include "common/arena_string.h"
#include "common/arena_string_pool.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"
#include "internal/time.h"

namespace cel {

using ConstantProto = ::google::api::expr::v1alpha1::Constant;

absl::StatusOr<ArenaConstant> ArenaConstantFromProto(
    absl::Nonnull<ArenaStringPool*> string_pool,
    absl::Nonnull<ArenaBytesPool*> bytes_pool, const ConstantProto& proto) {
  switch (proto.constant_kind_case()) {
    case ConstantProto::CONSTANT_KIND_NOT_SET:
      return ArenaConstant();
    case ConstantProto::kNullValue:
      return nullptr;
    case ConstantProto::kBoolValue:
      return proto.bool_value();
    case ConstantProto::kInt64Value:
      return static_cast<int64_t>(proto.int64_value());
    case ConstantProto::kUint64Value:
      return static_cast<uint64_t>(proto.uint64_value());
    case ConstantProto::kDoubleValue:
      return static_cast<double>(proto.double_value());
    case ConstantProto::kBytesValue:
      return bytes_pool->InternBytes(proto.bytes_value());
    case ConstantProto::kStringValue:
      return string_pool->InternString(proto.string_value());
    case ConstantProto::kDurationValue: {
      auto value = cel::internal::DecodeDuration(proto.duration_value());
      CEL_RETURN_IF_ERROR(cel::internal::ValidateDuration(value));
      return value;
    }
    case ConstantProto::kTimestampValue: {
      auto value = cel::internal::DecodeTime(proto.timestamp_value());
      CEL_RETURN_IF_ERROR(cel::internal::ValidateTimestamp(value));
      return value;
    }
    default:
      return absl::DataLossError(absl::StrCat("unexpected constant kind case: ",
                                              proto.constant_kind_case()));
  }
}

absl::Status ArenaConstantToProto(const ArenaConstant& constant,
                                  absl::Nonnull<ConstantProto*> proto) {
  switch (constant.kind()) {
    case ArenaConstantKind::kUnspecified:
      proto->constant_kind_case();
      break;
    case ArenaConstantKind::kNull:
      proto->set_null_value(google::protobuf::NULL_VALUE);
      break;
    case ArenaConstantKind::kBool:
      proto->set_bool_value(static_cast<bool>(constant));
      break;
    case ArenaConstantKind::kInt:
      proto->set_int64_value(static_cast<int64_t>(constant));
      break;
    case ArenaConstantKind::kUint:
      proto->set_uint64_value(static_cast<uint64_t>(constant));
      break;
    case ArenaConstantKind::kDouble:
      proto->set_double_value(static_cast<double>(constant));
      break;
    case ArenaConstantKind::kBytes:
      proto->set_bytes_value(static_cast<ArenaBytes>(constant));
      break;
    case ArenaConstantKind::kString:
      proto->set_string_value(static_cast<ArenaString>(constant));
      break;
    case ArenaConstantKind::kDuration:
      return cel::internal::EncodeDuration(
          static_cast<absl::Duration>(constant),
          proto->mutable_duration_value());
    case ArenaConstantKind::kTimestamp:
      return cel::internal::EncodeTime(static_cast<absl::Time>(constant),
                                       proto->mutable_timestamp_value());
    default:
      return absl::DataLossError(
          absl::StrCat("unexpected constant kind: ", constant.kind()));
  }
  return absl::OkStatus();
}

}  // namespace cel
