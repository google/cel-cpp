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

#include "base/types/list_type.h"

#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/types/dyn_type.h"
#include "base/value_factory.h"
#include "base/values/list_value.h"
#include "base/values/list_value_builder.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(ListType);

absl::Span<const absl::string_view> ListType::aliases() const {
  static constexpr absl::string_view kAliases[] = {"google.protobuf.ListValue"};
  if (element()->kind() == Kind::kDyn) {
    // Currently google.protobuf.ListValue resolves to list<dyn>.
    return absl::MakeConstSpan(kAliases);
  }
  return absl::Span<const absl::string_view>();
}

std::string ListType::DebugString() const {
  return absl::StrCat(name(), "(", element()->DebugString(), ")");
}

const Handle<Type>& ListType::element() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyListType&>(*this).element();
  }
  return static_cast<const base_internal::ModernListType&>(*this).element();
}

absl::StatusOr<UniqueRef<ListValueBuilderInterface>> ListType::NewValueBuilder(
    ValueFactory& value_factory) const {
  switch (element()->kind()) {
    case Kind::kBool:
      return MakeUnique<ListValueBuilder<BoolValue>>(
          value_factory.memory_manager(), base_internal::kComposedListType,
          value_factory, handle_from_this());
    case Kind::kInt:
      return MakeUnique<ListValueBuilder<IntValue>>(
          value_factory.memory_manager(), base_internal::kComposedListType,
          value_factory, handle_from_this());
    case Kind::kUint:
      return MakeUnique<ListValueBuilder<UintValue>>(
          value_factory.memory_manager(), base_internal::kComposedListType,
          value_factory, handle_from_this());
    case Kind::kDouble:
      return MakeUnique<ListValueBuilder<DoubleValue>>(
          value_factory.memory_manager(), base_internal::kComposedListType,
          value_factory, handle_from_this());
    case Kind::kDuration:
      return MakeUnique<ListValueBuilder<DurationValue>>(
          value_factory.memory_manager(), base_internal::kComposedListType,
          value_factory, handle_from_this());
    case Kind::kTimestamp:
      return MakeUnique<ListValueBuilder<TimestampValue>>(
          value_factory.memory_manager(), base_internal::kComposedListType,
          value_factory, handle_from_this());
    default:
      return MakeUnique<ListValueBuilder<Value>>(
          value_factory.memory_manager(), base_internal::kComposedListType,
          value_factory, handle_from_this());
  }
}

namespace base_internal {

const Handle<Type>& LegacyListType::element() const {
  return DynType::Get().As<Type>();
}

ModernListType::ModernListType(Handle<Type> element)
    : ListType(), HeapData(kKind), element_(std::move(element)) {
  // Ensure `Type*` and `HeapData*` are not thunked.
  ABSL_ASSERT(reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
              reinterpret_cast<uintptr_t>(static_cast<HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
