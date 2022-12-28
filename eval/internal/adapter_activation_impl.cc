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

#include "eval/internal/adapter_activation_impl.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/memory_manager.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "google/protobuf/arena.h"

namespace cel::interop_internal {

absl::optional<Handle<Value>> AdapterActivationImpl::ResolveVariable(
    MemoryManager& manager, absl::string_view name) const {
  google::protobuf::Arena* arena =
      extensions::ProtoMemoryManager::CastToProtoArena(manager);

  absl::optional<google::api::expr::runtime::CelValue> legacy_value =
      legacy_activation_.FindValue(name, arena);
  if (!legacy_value.has_value()) {
    return absl::nullopt;
  }
  return LegacyValueToModernValueOrDie(arena, *legacy_value);
}

}  // namespace cel::interop_internal
