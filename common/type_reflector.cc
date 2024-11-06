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

#include "common/type_reflector.h"

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/values/thread_compatible_type_reflector.h"

namespace cel {

absl::StatusOr<absl::Nullable<ValueBuilderPtr>> TypeReflector::NewValueBuilder(
    ValueFactory& value_factory, absl::string_view name) const {
  return nullptr;
}

absl::StatusOr<absl::optional<Value>> TypeReflector::DeserializeValue(
    ValueFactory& value_factory, absl::string_view type_url,
    const absl::Cord& value) const {
  return DeserializeValueImpl(value_factory, type_url, value);
}

absl::StatusOr<absl::optional<Value>> TypeReflector::DeserializeValueImpl(
    ValueFactory&, absl::string_view, const absl::Cord&) const {
  return absl::nullopt;
}

absl::StatusOr<absl::Nullable<StructValueBuilderPtr>>
TypeReflector::NewStructValueBuilder(ValueFactory& value_factory,
                                     const StructType& type) const {
  return nullptr;
}

absl::StatusOr<bool> TypeReflector::FindValue(ValueFactory&, absl::string_view,
                                              Value&) const {
  return false;
}

TypeReflector& TypeReflector::Builtin() {
  static absl::NoDestructor<TypeReflector> instance;
  return *instance;
}

Shared<TypeReflector> NewThreadCompatibleTypeReflector(
    MemoryManagerRef memory_manager) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleTypeReflector>();
}

}  // namespace cel
