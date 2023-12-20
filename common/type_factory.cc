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
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/types/type_cache.h"
#include "internal/names.h"

namespace cel {

namespace {

using common_internal::ListTypeCacheMap;
using common_internal::MapTypeCacheMap;
using common_internal::OpaqueTypeCacheMap;
using common_internal::ProcessLocalTypeCache;
using common_internal::StructTypeCacheMap;

bool IsValidMapKeyType(TypeView type) {
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

ListType TypeFactory::CreateListType(TypeView element) {
  if (auto list_type = ProcessLocalTypeCache::Get()->FindListType(element);
      list_type.has_value()) {
    return ListType(*list_type);
  }
  return CreateListTypeImpl(element);
}

MapType TypeFactory::CreateMapType(TypeView key, TypeView value) {
  ABSL_DCHECK(IsValidMapKeyType(key)) << key;
  if (auto map_type = ProcessLocalTypeCache::Get()->FindMapType(key, value);
      map_type.has_value()) {
    return MapType(*map_type);
  }
  return CreateMapTypeImpl(key, value);
}

StructType TypeFactory::CreateStructType(absl::string_view name) {
  ABSL_DCHECK(internal::IsValidRelativeName(name)) << name;
  return CreateStructTypeImpl(name);
}

OpaqueType TypeFactory::CreateOpaqueType(
    absl::string_view name, const SizedInputView<TypeView>& parameters) {
  ABSL_DCHECK(internal::IsValidRelativeName(name)) << name;
  if (auto opaque_type =
          ProcessLocalTypeCache::Get()->FindOpaqueType(name, parameters);
      opaque_type.has_value()) {
    return OpaqueType(*opaque_type);
  }
  return CreateOpaqueTypeImpl(name, parameters);
}

OptionalType TypeFactory::CreateOptionalType(TypeView parameter) {
  return Cast<OptionalType>(CreateOpaqueType(OptionalType::kName, {parameter}));
}

ListTypeView TypeFactory::GetDynListType() {
  return ProcessLocalTypeCache::Get()->GetDynListType();
}

MapTypeView TypeFactory::GetDynDynMapType() {
  return ProcessLocalTypeCache::Get()->GetDynDynMapType();
}

MapTypeView TypeFactory::GetStringDynMapType() {
  return ProcessLocalTypeCache::Get()->GetStringDynMapType();
}

OptionalTypeView TypeFactory::GetDynOptionalType() {
  return ProcessLocalTypeCache::Get()->GetDynOptionalType();
}

}  // namespace cel
