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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_BUILDER_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "common/native_type.h"
#include "common/value.h"
#include "eval/public/cel_value.h"
#include "google/protobuf/arena.h"

namespace cel {

class ValueFactory;

namespace common_internal {

// Special implementation of map which is both a modern map and legacy map. Do
// not try this at home. This should only be implemented in
// `map_value_builder.cc`.
class CompatMapValue : public ParsedMapValueInterface,
                       public google::api::expr::runtime::CelMap {
 private:
  NativeTypeId GetNativeTypeId() const final {
    return NativeTypeId::For<CompatMapValue>();
  }
};

absl::Nonnull<const CompatMapValue*> EmptyCompatMapValue();

absl::StatusOr<absl::Nonnull<const CompatMapValue*>> MakeCompatMapValue(
    absl::Nonnull<google::protobuf::Arena*> arena, const ParsedMapValue& value);

absl::Nonnull<cel::MapValueBuilderPtr> NewMapValueBuilder(
    ValueFactory& value_factory);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_BUILDER_H_
