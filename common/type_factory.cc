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

#include "common/type_factory.h"

#include "absl/base/attributes.h"
#include "common/type.h"
#include "common/type_kind.h"

namespace cel {

namespace {

bool IsValidMapKeyType(const Type& type) {
  switch (type.kind()) {
    case TypeKind::kDyn:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kString:
      return true;
    default:
      return false;
  }
}

}  // namespace

ListType TypeFactory::GetDynListType() { return ListType(); }

MapType TypeFactory::GetDynDynMapType() { return MapType(); }

MapType TypeFactory::GetStringDynMapType() { return JsonMapType(); }

OptionalType TypeFactory::GetDynOptionalType() { return OptionalType(); }

}  // namespace cel
