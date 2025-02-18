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
//
// Converters to/from serialized Value to/from runtime values.
#ifndef THIRD_PARTY_CEL_CPP_CONFORMANCE_VALUE_CONVERSION_H_
#define THIRD_PARTY_CEL_CPP_CONFORMANCE_VALUE_CONVERSION_H_

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/api/expr/v1alpha1/value.pb.h"
#include "cel/expr/value.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "common/type.h"
#include "common/value.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel::conformance_internal {

ABSL_MUST_USE_RESULT
inline bool UnsafeConvertWireCompatProto(
    const google::protobuf::MessageLite& src, absl::Nonnull<google::protobuf::MessageLite*> dest) {
  absl::Cord serialized;
  return src.SerializePartialToCord(&serialized) &&
         dest->ParsePartialFromCord(serialized);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::CheckedExpr& src,
    absl::Nonnull<google::api::expr::v1alpha1::CheckedExpr*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const google::api::expr::v1alpha1::CheckedExpr& src,
    absl::Nonnull<cel::expr::CheckedExpr*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::ParsedExpr& src,
    absl::Nonnull<google::api::expr::v1alpha1::ParsedExpr*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const google::api::expr::v1alpha1::ParsedExpr& src,
    absl::Nonnull<cel::expr::ParsedExpr*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::Expr& src,
    absl::Nonnull<google::api::expr::v1alpha1::Expr*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const google::api::expr::v1alpha1::Expr& src,
    absl::Nonnull<cel::expr::Expr*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const cel::expr::Value& src,
    absl::Nonnull<google::api::expr::v1alpha1::Value*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

ABSL_MUST_USE_RESULT
inline bool ConvertWireCompatProto(
    const google::api::expr::v1alpha1::Value& src,
    absl::Nonnull<cel::expr::Value*> dest) {
  return UnsafeConvertWireCompatProto(src, dest);
}

absl::StatusOr<Value> FromConformanceValue(
    const cel::expr::Value& value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena);

absl::StatusOr<cel::expr::Value> ToConformanceValue(
    const Value& value,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena);

absl::StatusOr<Type> FromConformanceType(google::protobuf::Arena* arena,
                                         const cel::expr::Type& type);

}  // namespace cel::conformance_internal
#endif  // THIRD_PARTY_CEL_CPP_CONFORMANCE_VALUE_CONVERSION_H_
