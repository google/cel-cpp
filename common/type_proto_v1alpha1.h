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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_V1ALPHA1_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_V1ALPHA1_H_

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "common/type_pool.h"

namespace cel {

// TypeFromProtoV1Alpha1 converts `google::api::expr::v1alpha1::Type` to
// `cel::Type`.
absl::StatusOr<Type> TypeFromProtoV1Alpha1(
    absl::Nonnull<TypePool*> type_pool,
    const google::api::expr::v1alpha1::Type& proto);

// TypeToProtoV1Alpha1 converts `cel::Type` to
// `google::api::expr::v1alpha1::Type`.
absl::Status TypeToProtoV1Alpha1(
    const Type& type, absl::Nonnull<google::api::expr::v1alpha1::Type*> proto);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_V1ALPHA1_H_
