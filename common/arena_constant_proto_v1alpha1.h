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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ARENA_CONSTANT_PROTO_V1ALPHA1_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ARENA_CONSTANT_PROTO_V1ALPHA1_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/arena_bytes_pool.h"
#include "common/arena_constant.h"
#include "common/arena_string_pool.h"

namespace cel {

absl::StatusOr<ArenaConstant> ArenaConstantFromProtoV1Alpha1(
    absl::Nonnull<ArenaStringPool*> string_pool,
    absl::Nonnull<ArenaBytesPool*> bytes_pool,
    const google::api::expr::v1alpha1::Constant& proto);

absl::Status ArenaConstantToProtoV1Alpha1(
    const ArenaConstant& constant,
    absl::Nonnull<google::api::expr::v1alpha1::Constant*> proto);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_CONSTANT_PROTO_V1ALPHA1_H_
