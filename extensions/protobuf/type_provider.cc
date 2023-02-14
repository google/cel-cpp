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

#include "extensions/protobuf/type_provider.h"

#include "extensions/protobuf/enum_type.h"
#include "extensions/protobuf/struct_type.h"

namespace cel::extensions {

absl::StatusOr<Handle<Type>> ProtoTypeProvider::ProvideType(
    TypeFactory& type_factory, absl::string_view name) const {
  {
    const auto* desc = pool_->FindMessageTypeByName(std::string(name));
    if (desc != nullptr) {
      return ProtoStructType::Create(type_factory, desc);
    }
  }
  const auto* desc = pool_->FindEnumTypeByName(name);
  if (desc != nullptr) {
    return ProtoEnumType::Create(type_factory, desc);
  }
  return Handle<Type>();
}

}  // namespace cel::extensions
