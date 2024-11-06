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

#include "runtime/type_registry.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "runtime/internal/legacy_runtime_type_provider.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

TypeRegistry::TypeRegistry(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nullable<google::protobuf::MessageFactory*> message_factory)
    : type_provider_(descriptor_pool),
      legacy_type_provider_(
          std::make_shared<runtime_internal::LegacyRuntimeTypeProvider>(
              descriptor_pool, message_factory)) {
  RegisterEnum("google.protobuf.NullValue", {{"NULL_VALUE", 0}});
}

void TypeRegistry::RegisterEnum(absl::string_view enum_name,
                                std::vector<Enumerator> enumerators) {
  enum_types_[enum_name] =
      Enumeration{std::string(enum_name), std::move(enumerators)};
}

}  // namespace cel
