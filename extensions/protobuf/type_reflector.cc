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

#include "extensions/protobuf/type_reflector.h"

#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "common/value_factory.h"

namespace cel::extensions {

absl::StatusOr<absl::optional<Value>> ProtoTypeReflector::DeserializeValueImpl(
    ValueFactory& value_factory, absl::string_view type_url,
    const absl::Cord& value) const {
  // This should not be reachable, as we provide both the pool and the factory
  // which should trigger DeserializeValue to handle the call and not call us.
  return absl::nullopt;
}

}  // namespace cel::extensions
