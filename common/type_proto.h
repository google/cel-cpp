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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_H_

#include "cel/expr/checked.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "common/type_pool.h"

namespace cel {

// TypeFromProto converts `cel::expr::Type` to `cel::Type`.
absl::StatusOr<Type> TypeFromProto(absl::Nonnull<TypePool*> type_pool,
                                   const cel::expr::Type& proto);

// TypeToProto converts `cel::Type` to `cel::expr::Type`.
absl::Status TypeToProto(const Type& type,
                         absl::Nonnull<cel::expr::Type*> proto);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_H_
