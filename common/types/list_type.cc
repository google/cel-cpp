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

#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace common_internal {

namespace {

ABSL_CONST_INIT const ListTypeData kDynListTypeData;

}  // namespace

absl::Nonnull<ListTypeData*> ListTypeData::Create(
    absl::Nonnull<google::protobuf::Arena*> arena, const Type& element) {
  return ::new (arena->AllocateAligned(
      sizeof(ListTypeData), alignof(ListTypeData))) ListTypeData(element);
}

ListTypeData::ListTypeData(const Type& element) : element(element) {}

}  // namespace common_internal

ListType::ListType() : ListType(&common_internal::kDynListTypeData) {}

ListType::ListType(absl::Nonnull<google::protobuf::Arena*> arena, const Type& element)
    : ListType(element.IsDyn()
                   ? &common_internal::kDynListTypeData
                   : common_internal::ListTypeData::Create(arena, element)) {}

std::string ListType::DebugString() const {
  return absl::StrCat("list<", element().DebugString(), ">");
}

absl::Span<const Type> ListType::parameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return absl::MakeConstSpan(&data_->element, 1);
}

const Type& ListType::element() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->element;
}

}  // namespace cel
