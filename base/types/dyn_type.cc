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

#include "base/types/dyn_type.h"

#include <utility>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/value_factory.h"
#include "internal/deserialize.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::DeserializeValue;

}  // namespace

CEL_INTERNAL_TYPE_IMPL(DynType);

absl::StatusOr<Handle<Value>> DynType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.Value.
  CEL_ASSIGN_OR_RETURN(auto deserialized_value, DeserializeValue(value));
  return value_factory.CreateValueFromJson(std::move(deserialized_value));
}

absl::Span<const absl::string_view> DynType::aliases() const {
  // Currently google.protobuf.Value also resolves to dyn.
  static constexpr absl::string_view kAliases[] = {"google.protobuf.Value"};
  return absl::MakeConstSpan(kAliases);
}

}  // namespace cel
