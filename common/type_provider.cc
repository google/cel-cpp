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

#include "common/type_provider.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>

#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/types/thread_compatible_type_provider.h"
#include "common/types/thread_safe_type_provider.h"
#include "common/types/type_cache.h"

namespace cel {

namespace {

struct FieldNameComparer {
  using is_transparent = void;

  bool operator()(StructTypeFieldView lhs, StructTypeFieldView rhs) const {
    return (*this)(lhs.name, rhs.name);
  }

  bool operator()(StructTypeFieldView lhs, absl::string_view rhs) const {
    return (*this)(lhs.name, rhs);
  }

  bool operator()(absl::string_view lhs, StructTypeFieldView rhs) const {
    return (*this)(lhs, rhs.name);
  }

  bool operator()(absl::string_view lhs, absl::string_view rhs) const {
    return lhs < rhs;
  }
};

struct FieldNumberComparer {
  using is_transparent = void;

  bool operator()(StructTypeFieldView lhs, StructTypeFieldView rhs) const {
    return (*this)(lhs.number, rhs.number);
  }

  bool operator()(StructTypeFieldView lhs, int64_t rhs) const {
    return (*this)(lhs.number, rhs);
  }

  bool operator()(int64_t lhs, StructTypeFieldView rhs) const {
    return (*this)(lhs, rhs.number);
  }

  bool operator()(int64_t lhs, int64_t rhs) const { return lhs < rhs; }
};

struct WellKnownType {
  WellKnownType(TypeView type,
                std::initializer_list<StructTypeFieldView> fields)
      : type(type), fields_by_name(fields), fields_by_number(fields) {
    std::sort(fields_by_name.begin(), fields_by_name.end(),
              FieldNameComparer{});
    std::sort(fields_by_number.begin(), fields_by_number.end(),
              FieldNumberComparer{});
  }

  TypeView type;
  // We use `2` as that accommodates most well known types.
  absl::InlinedVector<StructTypeFieldView, 2> fields_by_name;
  absl::InlinedVector<StructTypeFieldView, 2> fields_by_number;

  absl::optional<StructTypeFieldView> FieldByName(
      absl::string_view name) const {
    // Basically `std::binary_search`.
    auto it = std::lower_bound(fields_by_name.begin(), fields_by_name.end(),
                               name, FieldNameComparer{});
    if (it == fields_by_name.end() || it->name != name) {
      return absl::nullopt;
    }
    return *it;
  }

  absl::optional<StructTypeFieldView> FieldByNumber(int64_t number) const {
    // Basically `std::binary_search`.
    auto it = std::lower_bound(fields_by_number.begin(), fields_by_number.end(),
                               number, FieldNumberComparer{});
    if (it == fields_by_number.end() || it->number != number) {
      return absl::nullopt;
    }
    return *it;
  }
};

using WellKnownTypesMap = absl::flat_hash_map<absl::string_view, WellKnownType>;

const WellKnownTypesMap& GetWellKnownTypesMap() {
  static const WellKnownTypesMap* types = []() -> WellKnownTypesMap* {
    WellKnownTypesMap* types = new WellKnownTypesMap();
    types->insert_or_assign(
        "google.protobuf.BoolValue",
        WellKnownType{BoolWrapperTypeView{},
                      {StructTypeFieldView{"value", BoolTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.Int32Value",
        WellKnownType{IntWrapperTypeView{},
                      {StructTypeFieldView{"value", IntTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.Int64Value",
        WellKnownType{IntWrapperTypeView{},
                      {StructTypeFieldView{"value", IntTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.UInt32Value",
        WellKnownType{UintWrapperTypeView{},
                      {StructTypeFieldView{"value", UintTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.UInt64Value",
        WellKnownType{UintWrapperTypeView{},
                      {StructTypeFieldView{"value", UintTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.FloatValue",
        WellKnownType{DoubleWrapperTypeView{},
                      {StructTypeFieldView{"value", DoubleTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.DoubleValue",
        WellKnownType{DoubleWrapperTypeView{},
                      {StructTypeFieldView{"value", DoubleTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.StringValue",
        WellKnownType{StringWrapperTypeView{},
                      {StructTypeFieldView{"value", StringTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.BytesValue",
        WellKnownType{BytesWrapperTypeView{},
                      {StructTypeFieldView{"value", BytesTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.Duration",
        WellKnownType{DurationTypeView{},
                      {StructTypeFieldView{"seconds", IntTypeView{}, 1},
                       StructTypeFieldView{"nanos", IntTypeView{}, 2}}});
    types->insert_or_assign(
        "google.protobuf.Timestamp",
        WellKnownType{TimestampTypeView{},
                      {StructTypeFieldView{"seconds", IntTypeView{}, 1},
                       StructTypeFieldView{"nanos", IntTypeView{}, 2}}});
    types->insert_or_assign(
        "google.protobuf.Value",
        WellKnownType{
            DynTypeView{},
            {StructTypeFieldView{"null_value", NullTypeView{}, 1},
             StructTypeFieldView{"number_value", DoubleTypeView{}, 2},
             StructTypeFieldView{"string_value", StringTypeView{}, 3},
             StructTypeFieldView{"bool_value", BoolTypeView{}, 4},
             StructTypeFieldView{"struct_value",
                                 common_internal::ProcessLocalTypeCache::Get()
                                     ->GetStringDynMapType(),
                                 5},
             StructTypeFieldView{"list_value", ListTypeView{}, 6}}});
    types->insert_or_assign(
        "google.protobuf.ListValue",
        WellKnownType{ListTypeView{},
                      {StructTypeFieldView{"values", ListTypeView{}, 1}}});
    types->insert_or_assign(
        "google.protobuf.Struct",
        WellKnownType{
            common_internal::ProcessLocalTypeCache::Get()
                ->GetStringDynMapType(),
            {StructTypeFieldView{"fields",
                                 common_internal::ProcessLocalTypeCache::Get()
                                     ->GetStringDynMapType(),
                                 1}}});
    types->insert_or_assign(
        "google.protobuf.Any",
        WellKnownType{AnyTypeView{},
                      {StructTypeFieldView{"type_url", StringTypeView{}, 1},
                       StructTypeFieldView{"value", BytesTypeView{}, 2}}});
    return types;
  }();
  return *types;
}

}  // namespace

absl::StatusOr<absl::optional<TypeView>> TypeProvider::FindType(
    TypeFactory& type_factory, absl::string_view name, Type& scratch) {
  const auto& well_known_types = GetWellKnownTypesMap();
  if (auto it = well_known_types.find(name); it != well_known_types.end()) {
    return it->second.type;
  }
  return FindTypeImpl(type_factory, name, scratch);
}

absl::StatusOr<absl::optional<StructTypeFieldView>>
TypeProvider::FindStructTypeFieldByName(TypeFactory& type_factory,
                                        absl::string_view type,
                                        absl::string_view name,
                                        StructTypeField& scratch) {
  const auto& well_known_types = GetWellKnownTypesMap();
  if (auto it = well_known_types.find(type); it != well_known_types.end()) {
    return it->second.FieldByName(name);
    ;
  }
  return FindStructTypeFieldByNameImpl(type_factory, type, name, scratch);
}

Shared<TypeProvider> NewThreadCompatibleTypeProvider(
    MemoryManagerRef memory_manager) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleTypeProvider>();
}

Shared<TypeProvider> NewThreadSafeTypeProvider(
    MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<common_internal::ThreadSafeTypeProvider>();
}

}  // namespace cel
